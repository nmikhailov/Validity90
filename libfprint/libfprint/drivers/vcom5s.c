/*
 * Veridicom 5thSense driver for libfprint
 * Copyright (C) 2008 Daniel Drake <dsd@gentoo.org>
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

#define FP_COMPONENT "vcom5s"

/* TODO:
 * calibration?
 * image size: windows gets 300x300 through vpas enrollment util?
 * (probably just increase bulk read size?)
 * powerdown? does windows do anything special on exit?
 */

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <libusb.h>

#include <fp_internal.h>

#include "driver_ids.h"

#define CTRL_IN 0xc0
#define CTRL_OUT 0x40
#define CTRL_TIMEOUT	1000
#define EP_IN			(1 | LIBUSB_ENDPOINT_IN)

#define IMG_WIDTH		300
#define IMG_HEIGHT		288
#define ROWS_PER_RQ		12
#define NR_REQS			(IMG_HEIGHT / ROWS_PER_RQ)
#define RQ_SIZE			(IMG_WIDTH * ROWS_PER_RQ)
#define IMG_SIZE		(IMG_WIDTH * IMG_HEIGHT)

struct v5s_dev {
	int capture_iteration;
	struct fp_img *capture_img;
	gboolean loop_running;
	gboolean deactivating;
};

enum v5s_regs {
	/* when using gain 0x29:
	 * a value of 0x00 produces mostly-black image
	 * 0x09 destroys ridges (too white)
	 * 0x01 or 0x02 seem good values */
	REG_CONTRAST = 0x02,

	/* when using contrast 0x01:
	 * a value of 0x00 will produce an all-black image.
	 * 0x29 produces a good contrast image: ridges quite dark, but some
	 * light grey noise as background
	 * 0x46 produces all-white image with grey ridges (not very dark) */
	REG_GAIN = 0x03,
};

enum v5s_cmd {
	/* scan one row. has parameter, at a guess this is which row to scan? */
	CMD_SCAN_ONE_ROW = 0xc0,

	/* scan whole image */
	CMD_SCAN = 0xc1,
};

/***** REGISTER I/O *****/

static void sm_write_reg_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		fpi_ssm_mark_aborted(ssm, -EIO);
	else
		fpi_ssm_next_state(ssm);

	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static void sm_write_reg(struct fpi_ssm *ssm, unsigned char reg,
	unsigned char value)
{
	struct fp_img_dev *dev = ssm->priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *data;
	int r;
	
	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	fp_dbg("set %02x=%02x", reg, value);
	data = g_malloc(LIBUSB_CONTROL_SETUP_SIZE);
	libusb_fill_control_setup(data, CTRL_OUT, reg, value, 0, 0);
	libusb_fill_control_transfer(transfer, dev->udev, data, sm_write_reg_cb,
		ssm, CTRL_TIMEOUT);
	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		g_free(data);
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

static void sm_exec_cmd_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		fpi_ssm_mark_aborted(ssm, -EIO);
	else
		fpi_ssm_next_state(ssm);

	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static void sm_exec_cmd(struct fpi_ssm *ssm, unsigned char cmd,
	unsigned char param)
{
	struct fp_img_dev *dev = ssm->priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *data;
	int r;
	
	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	fp_dbg("cmd %02x param %02x", cmd, param);
	data = g_malloc(LIBUSB_CONTROL_SETUP_SIZE);
	libusb_fill_control_setup(data, CTRL_IN, cmd, param, 0, 0);
	libusb_fill_control_transfer(transfer, dev->udev, data, sm_exec_cmd_cb,
		ssm, CTRL_TIMEOUT);
	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		g_free(data);
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

/***** FINGER DETECTION *****/

/* We take 64x64 pixels at the center of the image, determine the average
 * pixel intensity, and threshold it. */
#define DETBOX_ROW_START 111
#define DETBOX_COL_START 117
#define DETBOX_ROWS 64
#define DETBOX_COLS 64
#define DETBOX_ROW_END (DETBOX_ROW_START + DETBOX_ROWS)
#define DETBOX_COL_END (DETBOX_COL_START + DETBOX_COLS)
#define FINGER_PRESENCE_THRESHOLD 100

static gboolean finger_is_present(unsigned char *data)
{
	int row;
	uint16_t imgavg = 0;

	for (row = DETBOX_ROW_START; row < DETBOX_ROW_END; row++) {
		unsigned char *rowdata = data + (row * IMG_WIDTH);
		uint16_t rowavg = 0;
		int col;

		for (col = DETBOX_COL_START; col < DETBOX_COL_END; col++)
			rowavg += rowdata[col];
		rowavg /= DETBOX_COLS;
		imgavg += rowavg;
	}
	imgavg /= DETBOX_ROWS;
	fp_dbg("img avg %d", imgavg);

	return (imgavg <= FINGER_PRESENCE_THRESHOLD);
}



/***** IMAGE ACQUISITION *****/

static void capture_iterate(struct fpi_ssm *ssm);

static void capture_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct v5s_dev *vdev = dev->priv;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	}

	if (++vdev->capture_iteration == NR_REQS) {
		struct fp_img *img = vdev->capture_img;
		/* must clear this early, otherwise the call chain takes us into
		 * loopsm_complete where we would free it, when in fact we are
		 * supposed to be handing off this image */
		vdev->capture_img = NULL;

		fpi_imgdev_report_finger_status(dev, finger_is_present(img->data));
		fpi_imgdev_image_captured(dev, img);
		fpi_ssm_next_state(ssm);
	} else {
		capture_iterate(ssm);
	}

out:
	libusb_free_transfer(transfer);
}

static void capture_iterate(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct v5s_dev *vdev = dev->priv;
	int iteration = vdev->capture_iteration;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	int r;

	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	libusb_fill_bulk_transfer(transfer, dev->udev, EP_IN,
		vdev->capture_img->data + (RQ_SIZE * iteration), RQ_SIZE,
		capture_cb, ssm, CTRL_TIMEOUT);
	transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;
	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}


static void sm_do_capture(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct v5s_dev *vdev = dev->priv;

	fp_dbg("");
	vdev->capture_img = fpi_img_new_for_imgdev(dev);
	vdev->capture_iteration = 0;
	capture_iterate(ssm);
}

/***** CAPTURE LOOP *****/

enum loop_states {
	LOOP_SET_CONTRAST,
	LOOP_SET_GAIN,
	LOOP_CMD_SCAN,
	LOOP_CAPTURE,
	LOOP_CAPTURE_DONE,
	LOOP_NUM_STATES,
};

static void loop_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct v5s_dev *vdev = dev->priv;

	switch (ssm->cur_state) {
	case LOOP_SET_CONTRAST:
		sm_write_reg(ssm, REG_CONTRAST, 0x01);
		break;
	case LOOP_SET_GAIN:
		sm_write_reg(ssm, REG_GAIN, 0x29);
		break;
	case LOOP_CMD_SCAN:
		if (vdev->deactivating) {
			fp_dbg("deactivating, marking completed");
			fpi_ssm_mark_completed(ssm);
		} else
			sm_exec_cmd(ssm, CMD_SCAN, 0x00);
		break;
	case LOOP_CAPTURE:
		sm_do_capture(ssm);
		break;
	case LOOP_CAPTURE_DONE:
		fpi_ssm_jump_to_state(ssm, LOOP_CMD_SCAN);
		break;
	}
}

static void loopsm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct v5s_dev *vdev = dev->priv;
	int r = ssm->error;

	fpi_ssm_free(ssm);
	fp_img_free(vdev->capture_img);
	vdev->capture_img = NULL;
	vdev->loop_running = FALSE;

	if (r)
		fpi_imgdev_session_error(dev, r);

	if (vdev->deactivating)
		fpi_imgdev_deactivate_complete(dev);
}

static int dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct v5s_dev *vdev = dev->priv;
	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, loop_run_state,
		LOOP_NUM_STATES);
	ssm->priv = dev;
	vdev->deactivating = FALSE;
	fpi_ssm_start(ssm, loopsm_complete);
	vdev->loop_running = TRUE;
	fpi_imgdev_activate_complete(dev, 0);
	return 0;
}

static void dev_deactivate(struct fp_img_dev *dev)
{
	struct v5s_dev *vdev = dev->priv;
	if (vdev->loop_running)
		vdev->deactivating = TRUE;
	else
		fpi_imgdev_deactivate_complete(dev);
}

static int dev_init(struct fp_img_dev *dev, unsigned long driver_data)
{
	int r;
	dev->priv = g_malloc0(sizeof(struct v5s_dev));

	r = libusb_claim_interface(dev->udev, 0);
	if (r < 0)
		fp_err("could not claim interface 0: %s", libusb_error_name(r));

	if (r == 0)
		fpi_imgdev_open_complete(dev, 0);

	return r;
}

static void dev_deinit(struct fp_img_dev *dev)
{
	g_free(dev->priv);
	libusb_release_interface(dev->udev, 0);
	fpi_imgdev_close_complete(dev);
}

static const struct usb_id id_table[] = {
	{ .vendor = 0x061a, .product = 0x0110 },
	{ 0, 0, 0, },
};

struct fp_img_driver vcom5s_driver = {
	.driver = {
		.id = VCOM5S_ID,
		.name = FP_COMPONENT,
		.full_name = "Veridicom 5thSense",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_PRESS,
	},
	.flags = 0,
	.img_height = IMG_HEIGHT,
	.img_width = IMG_WIDTH,

	.open = dev_init,
	.close = dev_deinit,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};

