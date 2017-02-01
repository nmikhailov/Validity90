/*
 * AuthenTec AES1660/AES2660 common routines
 * Copyright (C) 2007-2008 Daniel Drake <dsd@gentoo.org>
 * Copyright (C) 2007 Cyrille Bagard
 * Copyright (C) 2007-2008,2012 Vasily Khoruzhick
 *
 * Based on AES2550 driver
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

#define FP_COMPONENT "aesX660"

#include <stdio.h>

#include <errno.h>
#include <string.h>

#include <libusb.h>

#include <assembling.h>
#include <aeslib.h>
#include <fp_internal.h>

#include "aesx660.h"

static void start_capture(struct fp_img_dev *dev);
static void complete_deactivation(struct fp_img_dev *dev);

#define EP_IN			(1 | LIBUSB_ENDPOINT_IN)
#define EP_OUT			(2 | LIBUSB_ENDPOINT_OUT)
#define BULK_TIMEOUT		4000
#define FRAME_HEIGHT		AESX660_FRAME_HEIGHT

#define min(a, b) (((a) < (b)) ? (a) : (b))

static void aesX660_send_cmd_timeout(struct fpi_ssm *ssm, const unsigned char *cmd,
	size_t cmd_len, libusb_transfer_cb_fn callback, int timeout)
{
	struct fp_img_dev *dev = ssm->priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	int r;

	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	libusb_fill_bulk_transfer(transfer, dev->udev, EP_OUT,
		(unsigned char *)cmd, cmd_len,
		callback, ssm, timeout);
	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		fp_dbg("failed to submit transfer\n");
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
	}
}

static void aesX660_send_cmd(struct fpi_ssm *ssm, const unsigned char *cmd,
	size_t cmd_len, libusb_transfer_cb_fn callback)
{
	return aesX660_send_cmd_timeout(ssm, cmd, cmd_len, callback, BULK_TIMEOUT);
}

static void aesX660_read_response(struct fpi_ssm *ssm, size_t buf_len,
	libusb_transfer_cb_fn callback)
{
	struct fp_img_dev *dev = ssm->priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *data;
	int r;

	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	data = g_malloc(buf_len);
	libusb_fill_bulk_transfer(transfer, dev->udev, EP_IN,
		data, buf_len,
		callback, ssm, BULK_TIMEOUT);

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		fp_dbg("Failed to submit rx transfer: %d\n", r);
		g_free(data);
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

static void aesX660_send_cmd_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if ((transfer->status == LIBUSB_TRANSFER_COMPLETED) &&
		(transfer->length == transfer->actual_length)) {
		fpi_ssm_next_state(ssm);
	} else {
		fp_dbg("tx transfer status: %d, actual_len: %.4x\n",
			transfer->status, transfer->actual_length);
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
	libusb_free_transfer(transfer);
}

static void aesX660_read_calibrate_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	unsigned char *data = transfer->buffer;

	if ((transfer->status != LIBUSB_TRANSFER_COMPLETED) ||
		(transfer->length != transfer->actual_length)) {
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	}
	/* Calibrate response was read correctly? */
	if (data[AESX660_RESPONSE_TYPE_OFFSET] != AESX660_CALIBRATE_RESPONSE) {
		fp_dbg("Bogus calibrate response: %.2x\n", data[0]);
		fpi_ssm_mark_aborted(ssm, -EPROTO);
		goto out;
	}

	fpi_ssm_next_state(ssm);
out:
	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

/****** FINGER PRESENCE DETECTION ******/

enum finger_det_states {
	FINGER_DET_SEND_LED_CMD,
	FINGER_DET_SEND_FD_CMD,
	FINGER_DET_READ_FD_DATA,
	FINGER_DET_SET_IDLE,
	FINGER_DET_NUM_STATES,
};

static void finger_det_read_fd_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct aesX660_dev *aesdev = dev->priv;
	unsigned char *data = transfer->buffer;

	aesdev->fd_data_transfer = NULL;

	if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
		fp_dbg("Cancelling transfer...\n");
		fpi_ssm_next_state(ssm);
		goto out;
	}

	if ((transfer->status != LIBUSB_TRANSFER_COMPLETED) ||
	   (transfer->length != transfer->actual_length)) {
		fp_dbg("Failed to read FD data\n");
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	}

	if (data[AESX660_RESPONSE_TYPE_OFFSET] != AESX660_FINGER_DET_RESPONSE) {
		fp_dbg("Bogus FD response: %.2x\n", data[0]);
		fpi_ssm_mark_aborted(ssm, -EPROTO);
		goto out;
	}

	if (data[AESX660_FINGER_PRESENT_OFFSET] == AESX660_FINGER_PRESENT || aesdev->deactivating) {
		/* Finger present or we're deactivating... */
		fpi_ssm_next_state(ssm);
	} else {
		fp_dbg("Wait for finger returned %.2x as result\n",
			data[AESX660_FINGER_PRESENT_OFFSET]);
		fpi_ssm_jump_to_state(ssm, FINGER_DET_SEND_FD_CMD);
	}
out:
	g_free(data);
	libusb_free_transfer(transfer);
}

static void finger_det_set_idle_cmd_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if ((transfer->status == LIBUSB_TRANSFER_COMPLETED) &&
		(transfer->length == transfer->actual_length)) {
		fpi_ssm_mark_completed(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
	libusb_free_transfer(transfer);
}

static void finger_det_sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct aesX660_dev *aesdev = dev->priv;
	int err = ssm->error;

	fp_dbg("Finger detection completed");
	fpi_imgdev_report_finger_status(dev, TRUE);
	fpi_ssm_free(ssm);

	if (aesdev->deactivating)
		complete_deactivation(dev);
	else if (err)
		fpi_imgdev_session_error(dev, err);
	else {
		fpi_imgdev_report_finger_status(dev, TRUE);
		start_capture(dev);
	}
}

static void finger_det_run_state(struct fpi_ssm *ssm)
{
	switch (ssm->cur_state) {
	case FINGER_DET_SEND_LED_CMD:
		aesX660_send_cmd(ssm, led_blink_cmd, sizeof(led_blink_cmd),
			aesX660_send_cmd_cb);
	break;
	case FINGER_DET_SEND_FD_CMD:
		aesX660_send_cmd_timeout(ssm, wait_for_finger_cmd, sizeof(wait_for_finger_cmd),
			aesX660_send_cmd_cb, 0);
	break;
	case FINGER_DET_READ_FD_DATA:
		/* Should return 4 byte of response */
		aesX660_read_response(ssm, 4, finger_det_read_fd_data_cb);
	break;
	case FINGER_DET_SET_IDLE:
		aesX660_send_cmd(ssm, set_idle_cmd, sizeof(set_idle_cmd),
			finger_det_set_idle_cmd_cb);
	break;
	}
}

static void start_finger_detection(struct fp_img_dev *dev)
{
	struct fpi_ssm *ssm;
	struct aesX660_dev *aesdev = dev->priv;

	if (aesdev->deactivating) {
		complete_deactivation(dev);
		return;
	}

	ssm = fpi_ssm_new(dev->dev, finger_det_run_state, FINGER_DET_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, finger_det_sm_complete);
}

/****** CAPTURE ******/

enum capture_states {
	CAPTURE_SEND_LED_CMD,
	CAPTURE_SEND_CAPTURE_CMD,
	CAPTURE_READ_STRIPE_DATA,
	CAPTURE_SET_IDLE,
	CAPTURE_NUM_STATES,
};

/* Returns number of processed bytes */
static int process_stripe_data(struct fpi_ssm *ssm, unsigned char *data)
{
	struct fpi_frame *stripe;
	unsigned char *stripdata;
	struct fp_img_dev *dev = ssm->priv;
	struct aesX660_dev *aesdev = dev->priv;

	stripe = g_malloc(aesdev->assembling_ctx->frame_width * FRAME_HEIGHT / 2 + sizeof(struct fpi_frame)); /* 4 bpp */
	stripdata = stripe->data;

	fp_dbg("Processing frame %.2x %.2x", data[AESX660_IMAGE_OK_OFFSET],
		data[AESX660_LAST_FRAME_OFFSET]);

	stripe->delta_x = (int8_t)data[AESX660_FRAME_DELTA_X_OFFSET];
	stripe->delta_y = -(int8_t)data[AESX660_FRAME_DELTA_Y_OFFSET];
	fp_dbg("Offset to previous frame: %d %d", stripe->delta_x, stripe->delta_y);

	if (data[AESX660_IMAGE_OK_OFFSET] == AESX660_IMAGE_OK) {
		memcpy(stripdata, data + AESX660_IMAGE_OFFSET, aesdev->assembling_ctx->frame_width * FRAME_HEIGHT / 2);

		aesdev->strips = g_slist_prepend(aesdev->strips, stripe);
		aesdev->strips_len++;
		return (data[AESX660_LAST_FRAME_OFFSET] & AESX660_LAST_FRAME_BIT);
	} else {
		return 0;
	}

}

static void capture_set_idle_cmd_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct aesX660_dev *aesdev = dev->priv;

	if ((transfer->status == LIBUSB_TRANSFER_COMPLETED) &&
		(transfer->length == transfer->actual_length)) {
		struct fp_img *img;

		aesdev->strips = g_slist_reverse(aesdev->strips);
		img = fpi_assemble_frames(aesdev->assembling_ctx, aesdev->strips, aesdev->strips_len);
		img->flags |= aesdev->extra_img_flags;
		g_slist_foreach(aesdev->strips, (GFunc) g_free, NULL);
		g_slist_free(aesdev->strips);
		aesdev->strips = NULL;
		aesdev->strips_len = 0;
		fpi_imgdev_image_captured(dev, img);
		fpi_imgdev_report_finger_status(dev, FALSE);
		fpi_ssm_mark_completed(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
	libusb_free_transfer(transfer);
}

static void capture_read_stripe_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct aesX660_dev *aesdev = dev->priv;
	unsigned char *data = transfer->buffer;
	int finger_missing = 0;
	size_t copied, actual_len = transfer->actual_length;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	}

	fp_dbg("Got %d bytes of data", actual_len);
	do {
		copied = min(aesdev->buffer_max - aesdev->buffer_size, actual_len);
		memcpy(aesdev->buffer + aesdev->buffer_size,
			data,
			copied);
		actual_len -= copied;
		data += copied;
		aesdev->buffer_size += copied;
		fp_dbg("Copied %.4x bytes into internal buffer",
			copied);
		if (aesdev->buffer_size == aesdev->buffer_max) {
			if (aesdev->buffer_max == AESX660_HEADER_SIZE) {
				aesdev->buffer_max = aesdev->buffer[AESX660_RESPONSE_SIZE_LSB_OFFSET] +
					(aesdev->buffer[AESX660_RESPONSE_SIZE_MSB_OFFSET] << 8) + AESX660_HEADER_SIZE;
				fp_dbg("Got frame, type %.2x size %.4x",
					aesdev->buffer[AESX660_RESPONSE_TYPE_OFFSET],
					aesdev->buffer_max);
				continue;
			} else {
				finger_missing |= process_stripe_data(ssm, aesdev->buffer);
				aesdev->buffer_max = AESX660_HEADER_SIZE;
				aesdev->buffer_size = 0;
			}
		}
	} while (actual_len);

	fp_dbg("finger %s\n", finger_missing ? "missing" : "present");

	if (finger_missing) {
		fpi_ssm_next_state(ssm);
	} else {
		fpi_ssm_jump_to_state(ssm, CAPTURE_READ_STRIPE_DATA);
	}
out:
	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static void capture_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct aesX660_dev *aesdev = dev->priv;

	switch (ssm->cur_state) {
	case CAPTURE_SEND_LED_CMD:
		aesX660_send_cmd(ssm, led_solid_cmd, sizeof(led_solid_cmd),
			aesX660_send_cmd_cb);
	break;
	case CAPTURE_SEND_CAPTURE_CMD:
		aesdev->buffer_size = 0;
		aesdev->buffer_max = AESX660_HEADER_SIZE;
		aesX660_send_cmd(ssm, aesdev->start_imaging_cmd,
			aesdev->start_imaging_cmd_len,
			aesX660_send_cmd_cb);
	break;
	case CAPTURE_READ_STRIPE_DATA:
		aesX660_read_response(ssm, AESX660_BULK_TRANSFER_SIZE,
			capture_read_stripe_data_cb);
	break;
	case CAPTURE_SET_IDLE:
		fp_dbg("Got %d frames\n", aesdev->strips_len);
		aesX660_send_cmd(ssm, set_idle_cmd, sizeof(set_idle_cmd),
			capture_set_idle_cmd_cb);
	break;
	}
}

static void capture_sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct aesX660_dev *aesdev = dev->priv;
	int err = ssm->error;

	fp_dbg("Capture completed");
	fpi_ssm_free(ssm);

	if (aesdev->deactivating)
		complete_deactivation(dev);
	else if (err)
		fpi_imgdev_session_error(dev, err);
	else
		start_finger_detection(dev);
}

static void start_capture(struct fp_img_dev *dev)
{
	struct aesX660_dev *aesdev = dev->priv;
	struct fpi_ssm *ssm;

	if (aesdev->deactivating) {
		complete_deactivation(dev);
		return;
	}

	ssm = fpi_ssm_new(dev->dev, capture_run_state, CAPTURE_NUM_STATES);
	fp_dbg("");
	ssm->priv = dev;
	fpi_ssm_start(ssm, capture_sm_complete);
}

/****** INITIALIZATION/DEINITIALIZATION ******/

enum activate_states {
	ACTIVATE_SET_IDLE,
	ACTIVATE_SEND_READ_ID_CMD,
	ACTIVATE_READ_ID,
	ACTIVATE_SEND_CALIBRATE_CMD,
	ACTIVATE_READ_CALIBRATE_DATA,
	ACTIVATE_SEND_INIT_CMD,
	ACTIVATE_READ_INIT_RESPONSE,
	ACTIVATE_NUM_STATES,
};

static void activate_read_id_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct aesX660_dev *aesdev = dev->priv;
	unsigned char *data = transfer->buffer;

	if ((transfer->status != LIBUSB_TRANSFER_COMPLETED) ||
		(transfer->length != transfer->actual_length)) {
		fp_dbg("read_id cmd failed\n");
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	}
	/* ID was read correctly */
	if (data[0] == 0x07) {
		fp_dbg("Sensor device id: %.2x%2x, bcdDevice: %.2x.%.2x, init status: %.2x\n",
			data[4], data[3], data[5], data[6], data[7]);
	} else {
		fp_dbg("Bogus read ID response: %.2x\n", data[AESX660_RESPONSE_TYPE_OFFSET]);
		fpi_ssm_mark_aborted(ssm, -EPROTO);
		goto out;
	}

	switch (aesdev->init_seq_idx) {
	case 0:
		aesdev->init_seq = aesdev->init_seqs[0];
		aesdev->init_seq_len = aesdev->init_seqs_len[0];
		aesdev->init_seq_idx = 1;
		aesdev->init_cmd_idx = 0;
		/* Do calibration only after 1st init sequence */
		fpi_ssm_jump_to_state(ssm, ACTIVATE_SEND_INIT_CMD);
		break;
	case 1:
		aesdev->init_seq = aesdev->init_seqs[1];
		aesdev->init_seq_len = aesdev->init_seqs_len[1];
		aesdev->init_seq_idx = 2;
		aesdev->init_cmd_idx = 0;
		fpi_ssm_next_state(ssm);
		break;
	default:
		fp_dbg("Failed to init device! init status: %.2x\n", data[7]);
		fpi_ssm_mark_aborted(ssm, -EPROTO);
		break;

	}

out:
	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static void activate_read_init_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct aesX660_dev *aesdev = dev->priv;
	unsigned char *data = transfer->buffer;

	fp_dbg("read_init_cb\n");

	if ((transfer->status != LIBUSB_TRANSFER_COMPLETED) ||
		(transfer->length != transfer->actual_length)) {
		fp_dbg("read_init transfer status: %d, actual_len: %d\n", transfer->status, transfer->actual_length);
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	}
	/* ID was read correctly */
	if (data[0] != 0x42 || data[3] != 0x01) {
		fp_dbg("Bogus read init response: %.2x %.2x\n", data[0],
			data[3]);
		fpi_ssm_mark_aborted(ssm, -EPROTO);
		goto out;
	}
	aesdev->init_cmd_idx++;
	if (aesdev->init_cmd_idx == aesdev->init_seq_len) {
		if (aesdev->init_seq_idx < 2)
			fpi_ssm_jump_to_state(ssm, ACTIVATE_SEND_READ_ID_CMD);
		else
			fpi_ssm_mark_completed(ssm);
		goto out;
	}

	fpi_ssm_jump_to_state(ssm, ACTIVATE_SEND_INIT_CMD);
out:
	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static void activate_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct aesX660_dev *aesdev = dev->priv;

	switch (ssm->cur_state) {
	case ACTIVATE_SET_IDLE:
		aesdev->init_seq_idx = 0;
		fp_dbg("Activate: set idle\n");
		aesX660_send_cmd(ssm, set_idle_cmd, sizeof(set_idle_cmd),
			aesX660_send_cmd_cb);
	break;
	case ACTIVATE_SEND_READ_ID_CMD:
		fp_dbg("Activate: read ID\n");
		aesX660_send_cmd(ssm, read_id_cmd, sizeof(read_id_cmd),
			aesX660_send_cmd_cb);
	break;
	case ACTIVATE_READ_ID:
		/* Should return 8-byte response */
		aesX660_read_response(ssm, 8, activate_read_id_cb);
	break;
	case ACTIVATE_SEND_INIT_CMD:
		fp_dbg("Activate: send init seq #%d cmd #%d\n",
			aesdev->init_seq_idx,
			aesdev->init_cmd_idx);
		aesX660_send_cmd(ssm,
			aesdev->init_seq[aesdev->init_cmd_idx].cmd,
			aesdev->init_seq[aesdev->init_cmd_idx].len,
			aesX660_send_cmd_cb);
	break;
	case ACTIVATE_READ_INIT_RESPONSE:
		fp_dbg("Activate: read init response\n");
		/* Should return 4-byte response */
		aesX660_read_response(ssm, 4, activate_read_init_cb);
	break;
	case ACTIVATE_SEND_CALIBRATE_CMD:
		aesX660_send_cmd(ssm, calibrate_cmd, sizeof(calibrate_cmd),
			aesX660_send_cmd_cb);
	break;
	case ACTIVATE_READ_CALIBRATE_DATA:
		/* Should return 4-byte response */
		aesX660_read_response(ssm, 4, aesX660_read_calibrate_data_cb);
	break;
	}
}

static void activate_sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	int err = ssm->error;
	fp_dbg("status %d", err);
	fpi_imgdev_activate_complete(dev, err);
	fpi_ssm_free(ssm);

	if (!err)
		start_finger_detection(dev);
}

int aesX660_dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, activate_run_state,
		ACTIVATE_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, activate_sm_complete);
	return 0;
}

void aesX660_dev_deactivate(struct fp_img_dev *dev)
{
	struct aesX660_dev *aesdev = dev->priv;

	if (aesdev->fd_data_transfer)
		libusb_cancel_transfer(aesdev->fd_data_transfer);

	aesdev->deactivating = TRUE;
}

static void complete_deactivation(struct fp_img_dev *dev)
{
	struct aesX660_dev *aesdev = dev->priv;
	fp_dbg("");

	aesdev->deactivating = FALSE;
	g_slist_free(aesdev->strips);
	aesdev->strips = NULL;
	aesdev->strips_len = 0;
	fpi_imgdev_deactivate_complete(dev);
}
