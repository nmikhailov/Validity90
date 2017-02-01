/*
 * UPEK TouchChip driver for libfprint
 * Copyright (C) 2007 Jan-Michael Brummer <buzz2@gmx.de>
 * Copyright (C) 2012 Vasily Khoruzhick <anarsoul@gmail.com>
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

#define FP_COMPONENT "upektc"

#include <errno.h>
#include <string.h>
#include <libusb.h>
#include <fp_internal.h>

#include "upektc.h"
#include "driver_ids.h"

#define UPEKTC_EP_IN (2 | LIBUSB_ENDPOINT_IN)
#define UPEKTC_EP_OUT (3 | LIBUSB_ENDPOINT_OUT)
#define UPEKET_EP_IN (1 | LIBUSB_ENDPOINT_IN)
#define UPEKET_EP_OUT (2 | LIBUSB_ENDPOINT_OUT)
#define BULK_TIMEOUT 4000

struct upektc_dev {
	gboolean deactivating;
	const struct setup_cmd *setup_commands;
	size_t setup_commands_len;
	int ep_in;
	int ep_out;
	int init_idx;
	int sum_threshold;
};

enum upektc_driver_data {
	UPEKTC_2015,
	UPEKTC_3001,
};

static void start_capture(struct fp_img_dev *dev);
static void complete_deactivation(struct fp_img_dev *dev);
static void start_finger_detection(struct fp_img_dev *dev);

/****** INITIALIZATION/DEINITIALIZATION ******/

enum activate_states {
	WRITE_INIT,
	READ_DATA,
	ACTIVATE_NUM_STATES,
};

static void upektc_next_init_cmd(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_dev *upekdev = dev->priv;

	upekdev->init_idx += 1;
	if (upekdev->init_idx == upekdev->setup_commands_len)
		fpi_ssm_mark_completed(ssm);
	else
		fpi_ssm_jump_to_state(ssm, WRITE_INIT);
}

static void write_init_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_dev *upekdev = dev->priv;

	if ((transfer->status == LIBUSB_TRANSFER_COMPLETED) &&
		(transfer->length == transfer->actual_length)) {
		if (upekdev->setup_commands[upekdev->init_idx].response_len)
			fpi_ssm_next_state(ssm);
		else
			upektc_next_init_cmd(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
	libusb_free_transfer(transfer);
}

static void read_init_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED)
		upektc_next_init_cmd(ssm);
	else
		fpi_ssm_mark_aborted(ssm, -EIO);
	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static void activate_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_dev *upekdev = dev->priv;
	int r;

	switch (ssm->cur_state) {
	case WRITE_INIT:
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			return;
		}
		libusb_fill_bulk_transfer(transfer, dev->udev, upekdev->ep_out,
			(unsigned char*)upekdev->setup_commands[upekdev->init_idx].cmd,
			UPEKTC_CMD_LEN, write_init_cb, ssm, BULK_TIMEOUT);
		r = libusb_submit_transfer(transfer);
		if (r < 0) {
			libusb_free_transfer(transfer);
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
		}
	}
	break;
	case READ_DATA:
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		unsigned char *data;

		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			break;
		}

		data = g_malloc(upekdev->setup_commands[upekdev->init_idx].response_len);
		libusb_fill_bulk_transfer(transfer, dev->udev, upekdev->ep_in, data,
			upekdev->setup_commands[upekdev->init_idx].response_len,
			read_init_data_cb, ssm, BULK_TIMEOUT);

		r = libusb_submit_transfer(transfer);
		if (r < 0) {
			g_free(data);
			libusb_free_transfer(transfer);
			fpi_ssm_mark_aborted(ssm, r);
		}
	}
	break;
	}
}

static void activate_sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	fp_dbg("status %d", ssm->error);
	fpi_imgdev_activate_complete(dev, ssm->error);

	if (!ssm->error)
		start_finger_detection(dev);
	fpi_ssm_free(ssm);
}


/****** FINGER PRESENCE DETECTION ******/

static int finger_present(unsigned char *img, size_t len, int sum_threshold)
{
	int i, sum;

	sum = 0;

	for (i = 0; i < len; i++) {
		if (img[i] < 160) {
			sum++;
		}
	}

	fp_dbg("finger_present: sum is %d\n", sum);
	return sum < sum_threshold ? 0 : 1;
}

static void finger_det_data_cb(struct libusb_transfer *transfer)
{
	struct fp_img_dev *dev = transfer->user_data;
	struct upektc_dev *upekdev = dev->priv;
	unsigned char *data = transfer->buffer;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fp_dbg("data transfer status %d\n", transfer->status);
		fpi_imgdev_session_error(dev, -EIO);
		goto out;
	} else if (transfer->length != transfer->actual_length) {
		fp_dbg("expected %d, got %d bytes", transfer->length,
			transfer->actual_length);
		fpi_imgdev_session_error(dev, -EPROTO);
	}

	if (finger_present(data, IMAGE_SIZE, upekdev->sum_threshold)) {
		/* finger present, start capturing */
		fpi_imgdev_report_finger_status(dev, TRUE);
		start_capture(dev);
	} else {
		/* no finger, poll for a new histogram */
		start_finger_detection(dev);
	}

out:
	g_free(data);
	libusb_free_transfer(transfer);
}

static void finger_det_cmd_cb(struct libusb_transfer *t)
{
	struct libusb_transfer *transfer;
	unsigned char *data;
	int r;
	struct fp_img_dev *dev = t->user_data;
	struct upektc_dev *upekdev = dev->priv;

	if (t->status != LIBUSB_TRANSFER_COMPLETED) {
		fp_dbg("req transfer status %d\n", t->status);
		fpi_imgdev_session_error(dev, -EIO);
		goto exit_free_transfer;
	} else if (t->length != t->actual_length) {
		fp_dbg("expected %d, sent %d bytes", t->length, t->actual_length);
		fpi_imgdev_session_error(dev, -EPROTO);
		goto exit_free_transfer;
	}

	transfer = libusb_alloc_transfer(0);
	if (!transfer) {
		fpi_imgdev_session_error(dev, -ENOMEM);
		goto exit_free_transfer;
	}

	data = g_malloc(IMAGE_SIZE);
	libusb_fill_bulk_transfer(transfer, dev->udev, upekdev->ep_in, data, IMAGE_SIZE,
		finger_det_data_cb, dev, BULK_TIMEOUT);

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		g_free(data);
		libusb_free_transfer(transfer);
		fpi_imgdev_session_error(dev, r);
	}
exit_free_transfer:
	libusb_free_transfer(t);
}

static void start_finger_detection(struct fp_img_dev *dev)
{
	int r;
	struct upektc_dev *upekdev = dev->priv;
	struct libusb_transfer *transfer;
	fp_dbg("");

	if (upekdev->deactivating) {
		complete_deactivation(dev);
		return;
	}

	transfer = libusb_alloc_transfer(0);
	if (!transfer) {
		fpi_imgdev_session_error(dev, -ENOMEM);
		return;
	}
	libusb_fill_bulk_transfer(transfer, dev->udev, upekdev->ep_out,
		(unsigned char *)scan_cmd, UPEKTC_CMD_LEN,
		finger_det_cmd_cb, dev, BULK_TIMEOUT);
	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		libusb_free_transfer(transfer);
		fpi_imgdev_session_error(dev, r);
	}
}

/****** CAPTURE ******/

enum capture_states {
	CAPTURE_WRITE_CMD,
	CAPTURE_READ_DATA,
	CAPTURE_NUM_STATES,
};

static void capture_cmd_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if ((transfer->status == LIBUSB_TRANSFER_COMPLETED) &&
		(transfer->length == transfer->actual_length)) {
		fpi_ssm_next_state(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
	libusb_free_transfer(transfer);
}

static void capture_read_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	unsigned char *data = transfer->buffer;
	struct fp_img *img;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fp_dbg("request is not completed, %d", transfer->status);
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	} else if (transfer->length != transfer->actual_length) {
		fp_dbg("expected %d, sent %d bytes", transfer->length, transfer->actual_length);
		fpi_ssm_mark_aborted(ssm, -EPROTO);
		goto out;
	}

	img = fpi_img_new(IMAGE_SIZE);
	memcpy(img->data, data, IMAGE_SIZE);
	fpi_imgdev_image_captured(dev, img);
	fpi_imgdev_report_finger_status(dev, FALSE);
	fpi_ssm_mark_completed(ssm);
out:
	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static void capture_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_dev *upekdev = dev->priv;
	int r;

	switch (ssm->cur_state) {
	case CAPTURE_WRITE_CMD:
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			return;
		}
		libusb_fill_bulk_transfer(transfer, dev->udev, upekdev->ep_out,
			(unsigned char *)scan_cmd, UPEKTC_CMD_LEN,
			capture_cmd_cb, ssm, BULK_TIMEOUT);
		r = libusb_submit_transfer(transfer);
		if (r < 0) {
			libusb_free_transfer(transfer);
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
		}
	}
	break;
	case CAPTURE_READ_DATA:
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		unsigned char *data;

		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			break;
		}

		data = g_malloc(IMAGE_SIZE);
		libusb_fill_bulk_transfer(transfer, dev->udev, upekdev->ep_in, data, IMAGE_SIZE,
			capture_read_data_cb, ssm, BULK_TIMEOUT);

		r = libusb_submit_transfer(transfer);
		if (r < 0) {
			g_free(data);
			libusb_free_transfer(transfer);
			fpi_ssm_mark_aborted(ssm, r);
		}
	}
	break;
	};
}

static void capture_sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct upektc_dev *upekdev = dev->priv;

	fp_dbg("Capture completed");
	if (upekdev->deactivating)
		complete_deactivation(dev);
	else if (ssm->error)
		fpi_imgdev_session_error(dev, ssm->error);
	else
		start_finger_detection(dev);
	fpi_ssm_free(ssm);
}

static void start_capture(struct fp_img_dev *dev)
{
	struct upektc_dev *upekdev = dev->priv;
	struct fpi_ssm *ssm;

	if (upekdev->deactivating) {
		complete_deactivation(dev);
		return;
	}

	ssm = fpi_ssm_new(dev->dev, capture_run_state, CAPTURE_NUM_STATES);
	fp_dbg("");
	ssm->priv = dev;
	fpi_ssm_start(ssm, capture_sm_complete);
}

static int dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct upektc_dev *upekdev = dev->priv;
	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, activate_run_state,
		ACTIVATE_NUM_STATES);
	ssm->priv = dev;
	upekdev->init_idx = 0;
	fpi_ssm_start(ssm, activate_sm_complete);
	return 0;
}

static void dev_deactivate(struct fp_img_dev *dev)
{
	struct upektc_dev *upekdev = dev->priv;

	upekdev->deactivating = TRUE;
}

static void complete_deactivation(struct fp_img_dev *dev)
{
	struct upektc_dev *upekdev = dev->priv;
	fp_dbg("");

	upekdev->deactivating = FALSE;
	fpi_imgdev_deactivate_complete(dev);
}

static int dev_init(struct fp_img_dev *dev, unsigned long driver_data)
{
	/* TODO check that device has endpoints we're using */
	int r;
	struct upektc_dev *upekdev;

	r = libusb_claim_interface(dev->udev, 0);
	if (r < 0) {
		fp_err("could not claim interface 0: %s", libusb_error_name(r));
		return r;
	}

	dev->priv = upekdev = g_malloc0(sizeof(struct upektc_dev));
	switch (driver_data) {
	case UPEKTC_2015:
		upekdev->ep_in = UPEKTC_EP_IN;
		upekdev->ep_out = UPEKTC_EP_OUT;
		upekdev->setup_commands = upektc_setup_commands;
		upekdev->setup_commands_len = array_n_elements(upektc_setup_commands);
		upekdev->sum_threshold = UPEKTC_SUM_THRESHOLD;
		break;
	case UPEKTC_3001:
		upekdev->ep_in = UPEKET_EP_IN;
		upekdev->ep_out = UPEKET_EP_OUT;
		upekdev->setup_commands = upeket_setup_commands;
		upekdev->setup_commands_len = array_n_elements(upeket_setup_commands);
		upekdev->sum_threshold = UPEKET_SUM_THRESHOLD;
		break;
	default:
		fp_err("Device variant %d is not known\n", driver_data);
		g_free(upekdev);
		dev->priv = NULL;
		return -ENODEV;
		break;
	}
	fpi_imgdev_open_complete(dev, 0);
	return 0;
}

static void dev_deinit(struct fp_img_dev *dev)
{
	g_free(dev->priv);
	libusb_release_interface(dev->udev, 0);
	fpi_imgdev_close_complete(dev);
}

static const struct usb_id id_table[] = {
	{ .vendor = 0x0483, .product = 0x2015, .driver_data = UPEKTC_2015 },
	{ .vendor = 0x147e, .product = 0x3001, .driver_data = UPEKTC_3001 },
	{ 0, 0, 0, },
};

struct fp_img_driver upektc_driver = {
	.driver = {
		.id = UPEKTC_ID,
		.name = FP_COMPONENT,
		.full_name = "UPEK TouchChip/Eikon Touch 300",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_PRESS,
	},
	.flags = 0,
	.img_height = IMAGE_HEIGHT,
	.img_width = IMAGE_WIDTH,

	.bz3_threshold = 30,
	.open = dev_init,
	.close = dev_deinit,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};
