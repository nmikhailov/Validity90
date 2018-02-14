/*
 * Elan driver for libfprint
 *
 * Copyright (C) 2017 Igor Filatov <ia.filatov@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define FP_COMPONENT "elan"

#include <errno.h>
#include <libusb.h>
#include <assembling.h>
#include <fp_internal.h>
#include <fprint.h>

#include "elan.h"
#include "driver_ids.h"

unsigned char elan_get_pixel(struct fpi_frame_asmbl_ctx *ctx,
			     struct fpi_frame *frame, unsigned int x,
			     unsigned int y)
{
	return frame->data[x + y * ctx->frame_width];
}

static struct fpi_frame_asmbl_ctx assembling_ctx = {
	.frame_width = 0,
	.frame_height = 0,
	.image_width = 0,
	.get_pixel = elan_get_pixel,
};

struct elan_dev {
	gboolean deactivating;

	const struct elan_cmd *cmds;
	size_t cmds_len;
	int cmd_idx;
	int cmd_timeout;
	struct libusb_transfer *cur_transfer;

	unsigned char *last_read;
	unsigned char frame_width;
	unsigned char frame_height;
	unsigned char raw_frame_width;
	int num_frames;
	GSList *frames;
};

static void elan_dev_reset(struct elan_dev *elandev)
{
	fp_dbg("");

	BUG_ON(elandev->cur_transfer);

	elandev->deactivating = FALSE;

	elandev->cmds = NULL;
	elandev->cmd_idx = 0;
	elandev->cmd_timeout = ELAN_CMD_TIMEOUT;

	g_free(elandev->last_read);
	elandev->last_read = NULL;

	g_slist_free_full(elandev->frames, g_free);
	elandev->frames = NULL;
	elandev->num_frames = 0;
}

static void elan_save_frame(struct fp_img_dev *dev)
{
	struct elan_dev *elandev = dev->priv;
	unsigned char raw_height = elandev->frame_width;
	unsigned char raw_width = elandev->raw_frame_width;
	unsigned short *frame =
	    g_malloc(elandev->frame_width * elandev->frame_height * 2);

	fp_dbg("");

	/* Raw images are vertical and perpendicular to swipe direction of a
	 * normalized image, which means we need to make them horizontal before
	 * assembling. We also discard stirpes of ELAN_FRAME_MARGIN along raw
	 * height. */
	for (int y = 0; y < raw_height; y++)
		for (int x = ELAN_FRAME_MARGIN;
		     x < raw_width - ELAN_FRAME_MARGIN; x++) {
			int frame_idx =
			    y + (x - ELAN_FRAME_MARGIN) * raw_height;
			int raw_idx = x + y * raw_width;
			frame[frame_idx] =
			    ((unsigned short *)elandev->last_read)[raw_idx];
		}

	elandev->frames = g_slist_prepend(elandev->frames, frame);
	elandev->num_frames += 1;
}

/* Transform raw sesnsor data to normalized 8-bit grayscale image. */
static void elan_process_frame(unsigned short *raw_frame, GSList ** frames)
{
	unsigned int frame_size =
	    assembling_ctx.frame_width * assembling_ctx.frame_height;
	struct fpi_frame *frame =
	    g_malloc(frame_size + sizeof(struct fpi_frame));

	fp_dbg("");

	unsigned short min = 0xffff, max = 0;
	for (int i = 0; i < frame_size; i++) {
		if (raw_frame[i] < min)
			min = raw_frame[i];
		if (raw_frame[i] > max)
			max = raw_frame[i];
	}

	unsigned short px;
	for (int i = 0; i < frame_size; i++) {
		px = raw_frame[i];
		if (px <= min)
			px = 0;
		else if (px >= max)
			px = 0xff;
		else
			px = (px - min) * 0xff / (max - min);
		frame->data[i] = (unsigned char)px;
	}

	*frames = g_slist_prepend(*frames, frame);
}

static void elan_submit_image(struct fp_img_dev *dev)
{
	struct elan_dev *elandev = dev->priv;
	GSList *frames = NULL;
	struct fp_img *img;

	fp_dbg("");

	for (int i = 0; i < ELAN_SKIP_LAST_FRAMES; i++)
		elandev->frames = g_slist_next(elandev->frames);

	assembling_ctx.frame_width = elandev->frame_width;
	assembling_ctx.frame_height = elandev->frame_height;
	assembling_ctx.image_width = elandev->frame_width * 3 / 2;
	g_slist_foreach(elandev->frames, (GFunc) elan_process_frame, &frames);
	fpi_do_movement_estimation(&assembling_ctx, frames,
				   elandev->num_frames - ELAN_SKIP_LAST_FRAMES);
	img = fpi_assemble_frames(&assembling_ctx, frames,
				  elandev->num_frames - ELAN_SKIP_LAST_FRAMES);

	img->flags |= FP_IMG_PARTIAL;
	fpi_imgdev_image_captured(dev, img);
}

static void elan_cmd_done(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	elandev->cmd_idx += 1;
	if (elandev->cmd_idx < elandev->cmds_len)
		elan_run_next_cmd(ssm);
	else
		fpi_ssm_next_state(ssm);
}

static void elan_cmd_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	elandev->cur_transfer = NULL;

	switch (transfer->status) {
	case LIBUSB_TRANSFER_COMPLETED:
		if (transfer->length != transfer->actual_length) {
			fp_dbg("unexpected transfer length");
			elan_dev_reset(elandev);
			fpi_ssm_mark_aborted(ssm, -EPROTO);
		} else if (transfer->endpoint & LIBUSB_ENDPOINT_IN)
			/* just finished receiving */
			elan_cmd_done(ssm);
		else {
			/* just finished sending */
			if (elandev->cmds[elandev->cmd_idx].response_len)
				elan_cmd_read(ssm);
			else
				elan_cmd_done(ssm);
		}
		break;
	case LIBUSB_TRANSFER_CANCELLED:
		fp_dbg("transfer cancelled");
		fpi_ssm_mark_aborted(ssm, -ECANCELED);
		break;
	case LIBUSB_TRANSFER_TIMED_OUT:
		fp_dbg("transfer timed out");
		// elan_dev_reset(elandev);
		fpi_ssm_mark_aborted(ssm, -ETIMEDOUT);
		break;
	default:
		fp_dbg("transfer failed: %d", transfer->status);
		elan_dev_reset(elandev);
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
}

static void elan_cmd_read(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct elan_dev *elandev = dev->priv;
	int response_len = elandev->cmds[elandev->cmd_idx].response_len;

	fp_dbg("");

	if (elandev->cmds[elandev->cmd_idx].cmd == read_cmds[0].cmd)
		/* raw data has 2-byte "pixels" and the frame is vertical */
		response_len =
		    elandev->raw_frame_width * elandev->frame_width * 2;

	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}
	elandev->cur_transfer = transfer;

	g_free(elandev->last_read);
	elandev->last_read = g_malloc(response_len);

	libusb_fill_bulk_transfer(transfer, dev->udev,
				  elandev->cmds[elandev->cmd_idx].response_in,
				  elandev->last_read, response_len, elan_cmd_cb,
				  ssm, elandev->cmd_timeout);
	transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
	int r = libusb_submit_transfer(transfer);
	if (r < 0)
		fpi_ssm_mark_aborted(ssm, r);
}

static void elan_run_next_cmd(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}
	elandev->cur_transfer = transfer;

	libusb_fill_bulk_transfer(transfer, dev->udev, ELAN_EP_CMD_OUT,
				  (unsigned char *)elandev->cmds[elandev->
								 cmd_idx].cmd,
				  ELAN_CMD_LEN, elan_cmd_cb, ssm,
				  elandev->cmd_timeout);
	transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
	int r = libusb_submit_transfer(transfer);
	if (r < 0)
		fpi_ssm_mark_aborted(ssm, r);

}

static void elan_run_cmds(struct fpi_ssm *ssm, const struct elan_cmd *cmds,
			  size_t cmds_len, int cmd_timeout)
{
	struct fp_img_dev *dev = ssm->priv;
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	elandev->cmds = cmds;
	elandev->cmds_len = cmds_len;
	elandev->cmd_idx = 0;
	if (cmd_timeout != -1)
		elandev->cmd_timeout = cmd_timeout;
	elan_run_next_cmd(ssm);
}

enum deactivate_states {
	DEACTIVATE,
	DEACTIVATE_NUM_STATES,
};

static void elan_deactivate_run_state(struct fpi_ssm *ssm)
{
	switch (ssm->cur_state) {
	case DEACTIVATE:
		elan_run_cmds(ssm, deactivate_cmds, deactivate_cmds_len,
			      ELAN_CMD_TIMEOUT);
		break;
	}
}

static void deactivate_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;

	fpi_imgdev_deactivate_complete(dev);
}

static void elan_deactivate(struct fp_img_dev *dev)
{
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	elan_dev_reset(elandev);

	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, elan_deactivate_run_state,
					  DEACTIVATE_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, deactivate_complete);
}

enum capture_states {
	CAPTURE_START,
	CAPTURE_WAIT_FINGER,
	CAPTURE_READ_DATA,
	CAPTURE_SAVE_FRAME,
	CAPTURE_NUM_STATES,
};

static void elan_capture_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct elan_dev *elandev = dev->priv;

	switch (ssm->cur_state) {
	case CAPTURE_START:
		elan_run_cmds(ssm, capture_start_cmds, capture_start_cmds_len,
			      ELAN_CMD_TIMEOUT);
		break;
	case CAPTURE_WAIT_FINGER:
		elan_run_cmds(ssm, capture_wait_finger_cmds,
			      capture_wait_finger_cmds_len, -1);
		break;
	case CAPTURE_READ_DATA:
		/* 0x55 - finger present
		 * 0xff - device not calibrated */
		if (elandev->last_read && elandev->last_read[0] == 0x55) {
			fpi_imgdev_report_finger_status(dev, TRUE);
			elan_run_cmds(ssm, read_cmds, read_cmds_len,
				      ELAN_CMD_TIMEOUT);
		} else
			fpi_ssm_mark_aborted(ssm, FP_VERIFY_RETRY);
		break;
	case CAPTURE_SAVE_FRAME:
		elan_save_frame(dev);
		if (elandev->num_frames < ELAN_MAX_FRAMES) {
			/* quickly stop if finger is removed */
			elandev->cmd_timeout = ELAN_FINGER_TIMEOUT;
			fpi_ssm_jump_to_state(ssm, CAPTURE_WAIT_FINGER);
		}
		break;
	}
}

static void elan_capture_async(void *data)
{
	elan_capture((struct fp_img_dev *)data);
}

static void capture_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	if (elandev->deactivating)
		elan_deactivate(dev);

	/* either max frames captured or timed out waiting for the next frame */
	else if (!ssm->error
		 || (ssm->error == -ETIMEDOUT
		     && ssm->cur_state == CAPTURE_WAIT_FINGER))
		if (elandev->num_frames >= ELAN_MIN_FRAMES) {
			elan_submit_image(dev);
			fpi_imgdev_report_finger_status(dev, FALSE);
		} else
			fpi_imgdev_session_error(dev,
						 FP_VERIFY_RETRY_TOO_SHORT);

	/* other error
	 * It says "...session_error" but repotring 1 during verification
	 * makes it successful! */
	else
		fpi_imgdev_session_error(dev, FP_VERIFY_NO_MATCH);

	/* When enrolling the lib won't restart the capture after a stage has
	 * completed, so we need to keep feeding it images till it's had enough.
	 * But after that it can't finalize enrollemnt until this callback exits.
	 * That's why we schedule elan_capture instead of running it directly. */
	if (dev->dev->state == DEV_STATE_ENROLLING
	    && !fpi_timeout_add(10, elan_capture_async, dev))
		fpi_imgdev_session_error(dev, -ETIME);

	fpi_ssm_free(ssm);
}

static void elan_capture(struct fp_img_dev *dev)
{
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	elan_dev_reset(elandev);
	struct fpi_ssm *ssm =
	    fpi_ssm_new(dev->dev, elan_capture_run_state, CAPTURE_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, capture_complete);
}

enum calibrate_states {
	CALIBRATE_START_1,
	CALIBRATE_READ_DATA_1,
	CALIBRATE_END_1,
	CALIBRATE_START_2,
	CALIBRATE_READ_DATA_2,
	CALIBRATE_END_2,
	CALIBRATE_NUM_STATES,
};

static void elan_calibrate_run_state(struct fpi_ssm *ssm)
{
	switch (ssm->cur_state) {
	case CALIBRATE_START_1:
	case CALIBRATE_START_2:
		elan_run_cmds(ssm, calibrate_start_cmds,
			      calibrate_start_cmds_len, ELAN_CMD_TIMEOUT);
		break;
	case CALIBRATE_READ_DATA_1:
	case CALIBRATE_READ_DATA_2:
		elan_run_cmds(ssm, read_cmds, read_cmds_len, ELAN_CMD_TIMEOUT);
		break;
	case CALIBRATE_END_1:
	case CALIBRATE_END_2:
		elan_run_cmds(ssm, calibrate_end_cmds, calibrate_end_cmds_len,
			      ELAN_CMD_TIMEOUT);
	}
}

static void calibrate_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	if (elandev->deactivating)
		elan_deactivate(dev);
	else if (ssm->error)
		fpi_imgdev_session_error(dev, ssm->error);
	else {
		fpi_imgdev_activate_complete(dev, ssm->error);
		elan_capture(dev);
	}
	fpi_ssm_free(ssm);
}

static void elan_calibrate(struct fp_img_dev *dev)
{
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	elan_dev_reset(elandev);
	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, elan_calibrate_run_state,
					  CALIBRATE_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, calibrate_complete);
}

enum activate_states {
	ACTIVATE_GET_SENSOR_DIM,
	ACTIVATE_SET_SENSOR_DIM,
	ACTIVATE_START,
	ACTIVATE_READ_DATA,
	ACTIVATE_END,
	ACTIVATE_NUM_STATES,
};

static void elan_activate_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct elan_dev *elandev = dev->priv;

	switch (ssm->cur_state) {
	case ACTIVATE_GET_SENSOR_DIM:
		elan_run_cmds(ssm, get_sensor_dim_cmds, get_sensor_dim_cmds_len,
			      ELAN_CMD_TIMEOUT);
		break;
	case ACTIVATE_SET_SENSOR_DIM:
		elandev->frame_width = elandev->last_read[2];
		elandev->raw_frame_width = elandev->last_read[0];
		elandev->frame_height =
		    elandev->raw_frame_width - 2 * ELAN_FRAME_MARGIN;
		fpi_ssm_next_state(ssm);
		break;
	case ACTIVATE_START:
		elan_run_cmds(ssm, init_start_cmds, init_start_cmds_len,
			      ELAN_CMD_TIMEOUT);
		break;
	case ACTIVATE_READ_DATA:
		elan_run_cmds(ssm, read_cmds, read_cmds_len, ELAN_CMD_TIMEOUT);
		break;
	case ACTIVATE_END:
		elan_run_cmds(ssm, init_end_cmds, init_end_cmds_len,
			      ELAN_CMD_TIMEOUT);
	}
}

static void activate_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	if (elandev->deactivating)
		elan_deactivate(dev);
	else if (ssm->error)
		fpi_imgdev_session_error(dev, ssm->error);
	else
		elan_calibrate(dev);
	fpi_ssm_free(ssm);
}

static int dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	elan_dev_reset(elandev);
	struct fpi_ssm *ssm =
	    fpi_ssm_new(dev->dev, elan_activate_run_state, ACTIVATE_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, activate_complete);

	return 0;
}

static int dev_init(struct fp_img_dev *dev, unsigned long driver_data)
{
	struct elan_dev *elandev;
	int r;

	fp_dbg("");

	r = libusb_claim_interface(dev->udev, 0);
	if (r < 0) {
		fp_err("could not claim interface 0: %s", libusb_error_name(r));
		return r;
	}

	dev->priv = elandev = g_malloc0(sizeof(struct elan_dev));
	fpi_imgdev_open_complete(dev, 0);
	return 0;
}

static void dev_deinit(struct fp_img_dev *dev)
{
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	elan_dev_reset(elandev);
	g_free(elandev);
	libusb_release_interface(dev->udev, 0);
	fpi_imgdev_close_complete(dev);
}

static void dev_deactivate(struct fp_img_dev *dev)
{
	struct elan_dev *elandev = dev->priv;

	fp_dbg("");

	elandev->deactivating = TRUE;

	if (elandev->cur_transfer)
		libusb_cancel_transfer(elandev->cur_transfer);
	else
		elan_deactivate(dev);
}

static const struct usb_id id_table[] = {
	{.vendor = 0x04f3,.product = 0x0907},
	{0, 0, 0,},
};

struct fp_img_driver elan_driver = {
	.driver = {
		   .id = ELAN_ID,
		   .name = FP_COMPONENT,
		   .full_name = "ElanTech Fingerprint Sensor",
		   .id_table = id_table,
		   .scan_type = FP_SCAN_TYPE_SWIPE,
		   },
	.flags = 0,

	.bz3_threshold = 22,

	.open = dev_init,
	.close = dev_deinit,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};
