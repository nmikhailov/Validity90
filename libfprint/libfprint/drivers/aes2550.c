/*
 * AuthenTec AES2550/AES2810 driver for libfprint
 * Copyright (C) 2007-2008 Daniel Drake <dsd@gentoo.org>
 * Copyright (C) 2007 Cyrille Bagard
 * Copyright (C) 2007-2012 Vasily Khoruzhick
 *
 * Based on AES2501 driver
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

#define FP_COMPONENT "aes2550"

#include <errno.h>
#include <string.h>

#include <libusb.h>

#include <assembling.h>
#include <aeslib.h>
#include <fp_internal.h>

#include "aes2550.h"
#include "driver_ids.h"

static void start_capture(struct fp_img_dev *dev);
static void complete_deactivation(struct fp_img_dev *dev);

#define EP_IN			(1 | LIBUSB_ENDPOINT_IN)
#define EP_OUT			(2 | LIBUSB_ENDPOINT_OUT)
#define BULK_TIMEOUT 4000

/*
 * The AES2550 is an imaging device using a swipe-type sensor. It samples
 * the finger at preprogrammed intervals, sending a 192x16 frame to the
 * computer.
 * Unless the user is scanning their finger unreasonably fast, the frames
 * *will* overlap. The implementation below detects this overlap and produces
 * a contiguous image as the end result.
 * The fact that the user determines the length of the swipe (and hence the
 * number of useful frames) and also the fact that overlap varies means that
 * images returned from this driver vary in height.
 */

#define FRAME_WIDTH		192
#define FRAME_HEIGHT		8
#define FRAME_SIZE		(FRAME_WIDTH * FRAME_HEIGHT)
#define IMAGE_WIDTH		(FRAME_WIDTH + (FRAME_WIDTH / 2))

struct aes2550_dev {
	GSList *strips;
	size_t strips_len;
	gboolean deactivating;
	int heartbeat_cnt;
};

static struct fpi_frame_asmbl_ctx assembling_ctx = {
	.frame_width = FRAME_WIDTH,
	.frame_height = FRAME_HEIGHT,
	.image_width = IMAGE_WIDTH,
	.get_pixel = aes_get_pixel,
};

/****** FINGER PRESENCE DETECTION ******/

static unsigned char finger_det_reqs[] = {
	0x80, AES2550_REG80_MASTER_RESET,
	0x95, (8 << AES2550_REG95_COL_SCANNED_OFS) | (1 << AES2550_REG95_EPIX_AVG_OFS),
	0xad, 0x00,
	0xbd, (0 << AES2550_REGBD_LPO_IN_15_8_OFS),
	0xbe, (0 << AES2550_REGBE_LPO_IN_7_0_OFS),
	0xcf, AES2550_REGCF_INTERFERENCE_CHK_EN,
	AES2550_CMD_HEARTBEAT, 0x00, 0x01, 0x00, /* Heart beat off */
	AES2550_CMD_RUN_FD,
};

static void start_finger_detection(struct fp_img_dev *dev);

static void finger_det_data_cb(struct libusb_transfer *transfer)
{
	struct fp_img_dev *dev = transfer->user_data;
	unsigned char *data = transfer->buffer;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fp_dbg("data transfer status %d\n", transfer->status);
		fpi_imgdev_session_error(dev, -EIO);
		goto out;
	}

	fp_dbg("transfer completed, len: %.4x, data: %.2x %.2x",
		transfer->actual_length, (int)data[0], (int)data[1]);

	/* Check if we got 2 bytes, reg address 0x83 and its value */
	if ((transfer->actual_length >= 2) && (data[0] == 0x83) && (data[1] & AES2550_REG83_FINGER_PRESENT)) {
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

static void finger_det_reqs_cb(struct libusb_transfer *t)
{
	struct libusb_transfer *transfer;
	unsigned char *data;
	int r;
	struct fp_img_dev *dev = t->user_data;

	if (t->status != LIBUSB_TRANSFER_COMPLETED) {
		fp_dbg("req transfer status %d\n", t->status);
		fpi_imgdev_session_error(dev, -EIO);
		goto exit_free_transfer;
	} else if (t->length != t->actual_length) {
		fp_dbg("expected %d, got %d bytes", t->length, t->actual_length);
		fpi_imgdev_session_error(dev, -EPROTO);
		goto exit_free_transfer;
	}

	transfer = libusb_alloc_transfer(0);
	if (!transfer) {
		fpi_imgdev_session_error(dev, -ENOMEM);
		goto exit_free_transfer;
	}

	/* 2 bytes of result */
	data = g_malloc(AES2550_EP_IN_BUF_SIZE);
	libusb_fill_bulk_transfer(transfer, dev->udev, EP_IN, data, AES2550_EP_IN_BUF_SIZE,
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
	struct aes2550_dev *aesdev = dev->priv;
	struct libusb_transfer *transfer;
	fp_dbg("");

	if (aesdev->deactivating) {
		complete_deactivation(dev);
		return;
	}

	transfer = libusb_alloc_transfer(0);
	if (!transfer) {
		fpi_imgdev_session_error(dev, -ENOMEM);
		return;
	}
	libusb_fill_bulk_transfer(transfer, dev->udev, EP_OUT, finger_det_reqs,
		sizeof(finger_det_reqs), finger_det_reqs_cb, dev, BULK_TIMEOUT);
	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		libusb_free_transfer(transfer);
		fpi_imgdev_session_error(dev, r);
	}
}

/****** CAPTURE ******/

static unsigned char capture_reqs[] = {
	0x80, AES2550_REG80_MASTER_RESET,
	0x80, (1 << AES2550_REG80_SENSOR_MODE_OFS) | (AES2550_REG80_HGC_ENABLE),
	0x85, AES2550_REG85_FLUSH_PER_FRAME,
	0x8f, AES2550_REG8F_AUTH_DISABLE | AES2550_REG8F_EHISTO_DISABLE,
	0xbf, AES2550_REGBF_RSR_DIR_UPDOWN_MOTION | AES2550_REGBF_RSR_LEVEL_SUPER_RSR,
	0xcf, (3 << AES2550_REGCF_INTERFERENCE_AVG_OFFS) | AES2550_REGCF_INTERFERENCE_AVG_EN,
	0xdc, (1 << AES2550_REGDC_BP_NUM_REF_SWEEP_OFS),
	AES2550_CMD_HEARTBEAT, 0x00, 0x01, 0x03, /* Heart beat cmd, 3 * 16 cycles without sending image */
	AES2550_CMD_GET_ENROLL_IMG,
};

static unsigned char capture_set_idle_reqs[] = {
	0x80, AES2550_REG80_MASTER_RESET,
	AES2550_CMD_HEARTBEAT, 0x00, 0x01, 0x00, /* Heart beat off */
	AES2550_CMD_SET_IDLE_MODE,
};

enum capture_states {
	CAPTURE_WRITE_REQS,
	CAPTURE_READ_DATA,
	CAPTURE_SET_IDLE,
	CAPTURE_NUM_STATES,
};

/* Returns number of processed bytes */
static int process_strip_data(struct fpi_ssm *ssm, unsigned char *data)
{
	unsigned char *stripdata;
	struct fp_img_dev *dev = ssm->priv;
	struct aes2550_dev *aesdev = dev->priv;
	struct fpi_frame *stripe;
	int len;

	if (data[0] != AES2550_EDATA_MAGIC) {
		fp_dbg("Bogus magic: %.2x\n", (int)(data[0]));
		return -EPROTO;
	}
	len = data[1] * 256 + data[2];
	if (len != (AES2550_STRIP_SIZE - 3)) {
		fp_dbg("Bogus frame len: %.4x\n", len);
	}
	stripe = g_malloc(FRAME_WIDTH * FRAME_HEIGHT / 2 + sizeof(struct fpi_frame)); /* 4 bits per pixel */
	stripe->delta_x = (int8_t)data[6];
	stripe->delta_y = -(int8_t)data[7];
	stripdata = stripe->data;
	memcpy(stripdata, data + 33, FRAME_WIDTH * FRAME_HEIGHT / 2);
	aesdev->strips = g_slist_prepend(aesdev->strips, stripe);
	aesdev->strips_len++;

	fp_dbg("deltas: %dx%d", stripe->delta_x, stripe->delta_y);

	return 0;
}

static void capture_reqs_cb(struct libusb_transfer *transfer)
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

static void capture_set_idle_reqs_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct aes2550_dev *aesdev = dev->priv;

	if ((transfer->status == LIBUSB_TRANSFER_COMPLETED) &&
		(transfer->length == transfer->actual_length) &&
		aesdev->strips_len) {
		struct fp_img *img;

		aesdev->strips = g_slist_reverse(aesdev->strips);
		img = fpi_assemble_frames(&assembling_ctx,
					  aesdev->strips, aesdev->strips_len);
		img->flags |= FP_IMG_PARTIAL;
		g_slist_free_full(aesdev->strips, g_free);
		aesdev->strips = NULL;
		aesdev->strips_len = 0;
		fpi_imgdev_image_captured(dev, img);
		fpi_imgdev_report_finger_status(dev, FALSE);
		/* marking machine complete will re-trigger finger detection loop */
		fpi_ssm_mark_completed(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
	libusb_free_transfer(transfer);
}

static void capture_read_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct aes2550_dev *aesdev = dev->priv;
	unsigned char *data = transfer->buffer;
	int r;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fp_dbg("request is not completed, %d", transfer->status);
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	}

	fp_dbg("request completed, len: %.4x", transfer->actual_length);
	if (transfer->actual_length >= 2)
		fp_dbg("data: %.2x %.2x", (int)data[0], (int)data[1]);

	switch (transfer->actual_length) {
		case AES2550_STRIP_SIZE:
			r = process_strip_data(ssm, data);
			if (r < 0) {
				fp_dbg("Processing strip data failed: %d", r);
				fpi_ssm_mark_aborted(ssm, -EPROTO);
				goto out;
			}
			aesdev->heartbeat_cnt = 0;
			fpi_ssm_jump_to_state(ssm, CAPTURE_READ_DATA);
			break;
		case AES2550_HEARTBEAT_SIZE:
			if (data[0] == AES2550_HEARTBEAT_MAGIC) {
				/* No data for a long time => finger was removed or there's no movement */
				aesdev->heartbeat_cnt++;
				if (aesdev->heartbeat_cnt == 3) {
					/* Got 3 heartbeat message, that's enough to consider that finger was removed,
					 * assemble image and submit it to the library */
					fp_dbg("Got 3 heartbeats => finger removed");
					fpi_ssm_next_state(ssm);
				} else {
					fpi_ssm_jump_to_state(ssm, CAPTURE_READ_DATA);
				}
			}
			break;
		default:
			fp_dbg("Short frame %d, skip", transfer->actual_length);
			fpi_ssm_jump_to_state(ssm, CAPTURE_READ_DATA);
			break;
	}
out:
	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static void capture_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	int r;

	switch (ssm->cur_state) {
	case CAPTURE_WRITE_REQS:
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			return;
		}
		libusb_fill_bulk_transfer(transfer, dev->udev, EP_OUT, capture_reqs,
			sizeof(capture_reqs), capture_reqs_cb, ssm, BULK_TIMEOUT);
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

		data = g_malloc(AES2550_EP_IN_BUF_SIZE);
		libusb_fill_bulk_transfer(transfer, dev->udev, EP_IN, data, AES2550_EP_IN_BUF_SIZE,
			capture_read_data_cb, ssm, BULK_TIMEOUT);

		r = libusb_submit_transfer(transfer);
		if (r < 0) {
			g_free(data);
			libusb_free_transfer(transfer);
			fpi_ssm_mark_aborted(ssm, r);
		}
	}
	break;
	case CAPTURE_SET_IDLE:
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			return;
		}
		libusb_fill_bulk_transfer(transfer, dev->udev, EP_OUT, capture_set_idle_reqs,
			sizeof(capture_set_idle_reqs), capture_set_idle_reqs_cb, ssm, BULK_TIMEOUT);
		r = libusb_submit_transfer(transfer);
		if (r < 0) {
			libusb_free_transfer(transfer);
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
		}
	}
	break;
	};
}

static void capture_sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct aes2550_dev *aesdev = dev->priv;

	fp_dbg("Capture completed");
	if (aesdev->deactivating)
		complete_deactivation(dev);
	else if (ssm->error)
		fpi_imgdev_session_error(dev, ssm->error);
	else
		start_finger_detection(dev);
	fpi_ssm_free(ssm);
}

static void start_capture(struct fp_img_dev *dev)
{
	struct aes2550_dev *aesdev = dev->priv;
	struct fpi_ssm *ssm;

	if (aesdev->deactivating) {
		complete_deactivation(dev);
		return;
	}

	aesdev->heartbeat_cnt = 0;
	ssm = fpi_ssm_new(dev->dev, capture_run_state, CAPTURE_NUM_STATES);
	fp_dbg("");
	ssm->priv = dev;
	fpi_ssm_start(ssm, capture_sm_complete);
}

/****** INITIALIZATION/DEINITIALIZATION ******/

static unsigned char init_reqs[] = {
	0x80, AES2550_REG80_MASTER_RESET, /* Master reset */
	0x80, (1 << AES2550_REG80_SENSOR_MODE_OFS) | (AES2550_REG80_FORCE_FINGER_PRESENT),
	0x85, AES2550_REG85_FLUSH_PER_FRAME,
	0xa8, AES2550_REGA8_DIG_BIT_EN,
	0x81, AES2550_REG81_NSHOT,
};

static unsigned char calibrate_reqs[] = {
	0x80, AES2550_REG80_MASTER_RESET, /* Master reset */
	AES2550_CMD_CALIBRATE,
	AES2550_CMD_READ_CALIBRATION_DATA,
};

enum activate_states {
	WRITE_INIT,
	READ_DATA,
	CALIBRATE,
	READ_CALIB_TABLE,
	ACTIVATE_NUM_STATES,
};

static void init_reqs_cb(struct libusb_transfer *transfer)
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

static void init_read_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
		fpi_ssm_next_state(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

/* TODO: use calibration table, datasheet is rather terse on that
 * need more info for implementaion */
static void calibrate_read_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
		fpi_ssm_next_state(ssm);
	} else {
		fpi_ssm_mark_aborted(ssm, -EIO);
	}
	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static void activate_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	int r;

	switch (ssm->cur_state) {
	case WRITE_INIT:
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			return;
		}
		libusb_fill_bulk_transfer(transfer, dev->udev, EP_OUT, init_reqs,
			sizeof(init_reqs), init_reqs_cb, ssm, BULK_TIMEOUT);
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

		data = g_malloc(AES2550_EP_IN_BUF_SIZE);
		libusb_fill_bulk_transfer(transfer, dev->udev, EP_IN, data, AES2550_EP_IN_BUF_SIZE,
			init_read_data_cb, ssm, BULK_TIMEOUT);

		r = libusb_submit_transfer(transfer);
		if (r < 0) {
			g_free(data);
			libusb_free_transfer(transfer);
			fpi_ssm_mark_aborted(ssm, r);
		}
	}
	break;
	case CALIBRATE:
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			return;
		}
		libusb_fill_bulk_transfer(transfer, dev->udev, EP_OUT, calibrate_reqs,
			sizeof(calibrate_reqs), init_reqs_cb, ssm, BULK_TIMEOUT);
		r = libusb_submit_transfer(transfer);
		if (r < 0) {
			libusb_free_transfer(transfer);
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
		}
	}
	break;
	case READ_CALIB_TABLE:
	{
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		unsigned char *data;

		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			break;
		}

		data = g_malloc(AES2550_EP_IN_BUF_SIZE);
		libusb_fill_bulk_transfer(transfer, dev->udev, EP_IN, data, AES2550_EP_IN_BUF_SIZE,
			calibrate_read_data_cb, ssm, BULK_TIMEOUT);

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

static int dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, activate_run_state,
		ACTIVATE_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, activate_sm_complete);
	return 0;
}

static void dev_deactivate(struct fp_img_dev *dev)
{
	struct aes2550_dev *aesdev = dev->priv;

	aesdev->deactivating = TRUE;
}

static void complete_deactivation(struct fp_img_dev *dev)
{
	struct aes2550_dev *aesdev = dev->priv;
	fp_dbg("");

	aesdev->deactivating = FALSE;
	g_slist_free(aesdev->strips);
	aesdev->strips = NULL;
	aesdev->strips_len = 0;
	fpi_imgdev_deactivate_complete(dev);
}

static int dev_init(struct fp_img_dev *dev, unsigned long driver_data)
{
	/* TODO check that device has endpoints we're using */
	int r;

	r = libusb_claim_interface(dev->udev, 0);
	if (r < 0) {
		fp_err("could not claim interface 0: %s", libusb_error_name(r));
		return r;
	}

	dev->priv = g_malloc0(sizeof(struct aes2550_dev));
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
	{ .vendor = 0x08ff, .product = 0x2550 }, /* AES2550 */
	{ .vendor = 0x08ff, .product = 0x2810 }, /* AES2810 */
	{ 0, 0, 0, },
};

struct fp_img_driver aes2550_driver = {
	.driver = {
		.id = AES2550_ID,
		.name = FP_COMPONENT,
		.full_name = "AuthenTec AES2550/AES2810",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_SWIPE,
	},
	.flags = 0,
	.img_height = -1,
	.img_width = FRAME_WIDTH + FRAME_WIDTH / 2,

	.open = dev_init,
	.close = dev_deinit,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};
