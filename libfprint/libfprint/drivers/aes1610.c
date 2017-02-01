/*
 * AuthenTec AES1610 driver for libfprint
 * Copyright (C) 2007-2008 Daniel Drake <dsd@gentoo.org>
 * Copyright (C) 2007 Cyrille Bagard
 * Copyright (C) 2007 Vasily Khoruzhick
 * Copyright (C) 2009 Guido Grazioli <guido.grazioli@gmail.com>
 * Copyright (C) 2012 Vasily Khoruzhick <anarsoul@gmail.com>
 *
 * Based on code from libfprint aes2501 driver.
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

#define FP_COMPONENT "aes1610"

#include <errno.h>
#include <string.h>

#include <libusb.h>

#include <assembling.h>
#include <aeslib.h>
#include <fp_internal.h>

#include "driver_ids.h"

static void start_capture(struct fp_img_dev *dev);
static void complete_deactivation(struct fp_img_dev *dev);
static int adjust_gain(unsigned char *buffer, int status);

#define FIRST_AES1610_REG	0x1B
#define LAST_AES1610_REG	0xFF

#define GAIN_STATUS_FIRST 1
#define GAIN_STATUS_NORMAL 2

/* FIXME these need checking */
#define EP_IN			(1 | LIBUSB_ENDPOINT_IN)
#define EP_OUT			(2 | LIBUSB_ENDPOINT_OUT)

#define BULK_TIMEOUT 4000

/*
 * The AES1610 is an imaging device using a swipe-type sensor. It samples
 * the finger at preprogrammed intervals, sending a 128x8 frame to the
 * computer.
 * Unless the user is scanning their finger unreasonably fast, the frames
 * *will* overlap. The implementation below detects this overlap and produces
 * a contiguous image as the end result.
 * The fact that the user determines the length of the swipe (and hence the
 * number of useful frames) and also the fact that overlap varies means that
 * images returned from this driver vary in height.
 */

#define FRAME_WIDTH	128
#define FRAME_HEIGHT	8
#define FRAME_SIZE	(FRAME_WIDTH * FRAME_HEIGHT)
#define IMAGE_WIDTH	(FRAME_WIDTH + (FRAME_WIDTH / 2))
/* maximum number of frames to read during a scan */
/* FIXME reduce substantially */
#define MAX_FRAMES		350

/****** GENERAL FUNCTIONS ******/

struct aes1610_dev {
	uint8_t read_regs_retry_count;
	GSList *strips;
	size_t strips_len;
	gboolean deactivating;
	uint8_t blanks_count;
};

static struct fpi_frame_asmbl_ctx assembling_ctx = {
	.frame_width = FRAME_WIDTH,
	.frame_height = FRAME_HEIGHT,
	.image_width = IMAGE_WIDTH,
	.get_pixel = aes_get_pixel,
};

typedef void (*aes1610_read_regs_cb)(struct fp_img_dev *dev, int status,
	unsigned char *regs, void *user_data);

struct aes1610_read_regs {
	struct fp_img_dev *dev;
	aes1610_read_regs_cb callback;
	struct aes_regwrite *regwrite;
	void *user_data;
};

/* FIXME: what to do here? */
static void stub_capture_stop_cb(struct fp_img_dev *dev, int result, void *user_data)
{

}


/* check that read succeeded but ignore all data */
static void generic_ignore_data_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		fpi_ssm_mark_aborted(ssm, -EIO);
	else if (transfer->length != transfer->actual_length)
		fpi_ssm_mark_aborted(ssm, -EPROTO);
	else
		fpi_ssm_next_state(ssm);

	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static void generic_write_regv_cb(struct fp_img_dev *dev, int result,
	void *user_data)
{
	struct fpi_ssm *ssm = user_data;
	if (result == 0)
		fpi_ssm_next_state(ssm);
	else
		fpi_ssm_mark_aborted(ssm, result);
}

/* read the specified number of bytes from the IN endpoint but throw them
 * away, then increment the SSM */
static void generic_read_ignore_data(struct fpi_ssm *ssm, size_t bytes)
{
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *data;
	int r;

	if (!transfer) {
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	data = g_malloc(bytes);
	libusb_fill_bulk_transfer(transfer, ssm->dev->udev, EP_IN, data, bytes,
		generic_ignore_data_cb, ssm, BULK_TIMEOUT);

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		g_free(data);
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

/****** FINGER PRESENCE DETECTION ******/


static const struct aes_regwrite finger_det_reqs[] = {
	{ 0x80, 0x01 },
	{ 0x80, 0x12 },
	{ 0x85, 0x00 },
	{ 0x8A, 0x00 },
	{ 0x8B, 0x0E },
	{ 0x8C, 0x90 },
	{ 0x8D, 0x83 },
	{ 0x8E, 0x07 },
	{ 0x8F, 0x07 },
	{ 0x96, 0x00 },
	{ 0x97, 0x48 },
	{ 0xA1, 0x00 },
	{ 0xA2, 0x50 },
	{ 0xA6, 0xE4 },
	{ 0xAD, 0x08 },
	{ 0xAE, 0x5B },
	{ 0xAF, 0x54 },
	{ 0xB1, 0x28 },
	{ 0xB5, 0xAB },
	{ 0xB6, 0x0E },
	{ 0x1B, 0x2D },
	{ 0x81, 0x04 }
};

static void start_finger_detection(struct fp_img_dev *dev);

static void finger_det_data_cb(struct libusb_transfer *transfer)
{
	struct fp_img_dev *dev = transfer->user_data;
	unsigned char *data = transfer->buffer;
	int i;
	int sum = 0;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fpi_imgdev_session_error(dev, -EIO);
		goto out;
	} else if (transfer->length != transfer->actual_length) {
		fpi_imgdev_session_error(dev, -EPROTO);
		goto out;
	}

	/* examine histogram to determine finger presence */
	for (i = 3; i < 17; i++)
		sum += (data[i] & 0xf) + (data[i] >> 4);
	if (sum > 20) {
		/* reset default gain */
		adjust_gain(data,GAIN_STATUS_FIRST);
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

static void finger_det_reqs_cb(struct fp_img_dev *dev, int result, void *user_data)
{
	struct libusb_transfer *transfer;
	unsigned char *data;
	int r;

	if (result) {
		fpi_imgdev_session_error(dev, result);
		return;
	}

	transfer = libusb_alloc_transfer(0);
	if (!transfer) {
		fpi_imgdev_session_error(dev, -ENOMEM);
		return;
	}

	data = g_malloc(19);
	libusb_fill_bulk_transfer(transfer, dev->udev, EP_IN, data, 19,
		finger_det_data_cb, dev, BULK_TIMEOUT);

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		g_free(data);
		libusb_free_transfer(transfer);
		fpi_imgdev_session_error(dev, r);
	}

}

static void start_finger_detection(struct fp_img_dev *dev)
{
	struct aes1610_dev *aesdev = dev->priv;

	if (aesdev->deactivating) {
		complete_deactivation(dev);
		return;
	}

	aes_write_regv(dev, finger_det_reqs, G_N_ELEMENTS(finger_det_reqs), finger_det_reqs_cb, NULL);

}

/****** CAPTURE ******/

static struct aes_regwrite capture_reqs[] = {
	{ 0x80, 0x01 },
	{ 0x80, 0x12 },
	{ 0x84, 0x01 },
	{ 0x85, 0x00 },
	{ 0x89, 0x64 },
	{ 0x8A, 0x00 },
	{ 0x8B, 0x0E },
	{ 0x8C, 0x90 },
	{ 0xBE, 0x23 },
	{ 0x29, 0x04 },
	{ 0x2A, 0xFF },
	{ 0x96, 0x00 },
	{ 0x98, 0x03 },
	{ 0x99, 0x00 },
	{ 0x9C, 0xA5 },
	{ 0x9D, 0x40 },
	{ 0x9E, 0xC6 },
	{ 0x9F, 0x8E },
	{ 0xA2, 0x50 },
	{ 0xA3, 0xF0 },
	{ 0xAD, 0x08 },
	{ 0xBD, 0x4F },
	{ 0xAF, 0x54 },
	{ 0xB1, 0x08 },
	{ 0xB5, 0xAB },
	{ 0x1B, 0x2D },
	{ 0xB6, 0x4E },
	{ 0xB8, 0x70 },
	{ 0x2B, 0xB3 },
	{ 0x2C, 0x5D },
	{ 0x2D, 0x98 },
	{ 0x2E, 0xB0 },
	{ 0x2F, 0x20 },
	{ 0xA2, 0xD0 },
	{ 0x1D, 0x21 },
	{ 0x1E, 0xBE },
	{ 0x1C, 0x00 },
	{ 0x1D, 0x30 },
	{ 0x1E, 0x29 },
	{ 0x1C, 0x01 },
	{ 0x1D, 0x00 },
	{ 0x1E, 0x9E },
	{ 0x1C, 0x02 },
	{ 0x1D, 0x30 },
	{ 0x1E, 0xBB },
	{ 0x1C, 0x03 },
	{ 0x1D, 0x00 },
	{ 0x1E, 0x9D },
	{ 0x1C, 0x04 },
	{ 0x1D, 0x22 },
	{ 0x1E, 0xFF },
	{ 0x1C, 0x05 },
	{ 0x1D, 0x1B },
	{ 0x1E, 0x4E },
	{ 0x1C, 0x06 },
	{ 0x1D, 0x16 },
	{ 0x1E, 0x28 },
	{ 0x1C, 0x07 },
	{ 0x1D, 0x22 },
	{ 0x1E, 0xFF },
	{ 0x1C, 0x08 },
	{ 0x1D, 0x15 },
	{ 0x1E, 0xF1 },
	{ 0x1C, 0x09 },
	{ 0x1D, 0x30 },
	{ 0x1E, 0xD5 },
	{ 0x1C, 0x0A },
	{ 0x1D, 0x00 },
	{ 0x1E, 0x9E },
	{ 0x1C, 0x0B },
	{ 0x1D, 0x17 },
	{ 0x1E, 0x9D },
	{ 0x1C, 0x0C },
	{ 0x1D, 0x28 },
	{ 0x1E, 0xD7 },
	{ 0x1C, 0x0D },
	{ 0x1D, 0x17 },
	{ 0x1E, 0xD7 },
	{ 0x1C, 0x0E },
	{ 0x1D, 0x0A },
	{ 0x1E, 0xCB },
	{ 0x1C, 0x0F },
	{ 0x1D, 0x24 },
	{ 0x1E, 0x14 },
	{ 0x1C, 0x10 },
	{ 0x1D, 0x17 },
	{ 0x1E, 0x85 },
	{ 0x1C, 0x11 },
	{ 0x1D, 0x15 },
	{ 0x1E, 0x71 },
	{ 0x1C, 0x12 },
	{ 0x1D, 0x2B },
	{ 0x1E, 0x36 },
	{ 0x1C, 0x13 },
	{ 0x1D, 0x12 },
	{ 0x1E, 0x06 },
	{ 0x1C, 0x14 },
	{ 0x1D, 0x30 },
	{ 0x1E, 0x97 },
	{ 0x1C, 0x15 },
	{ 0x1D, 0x21 },
	{ 0x1E, 0x32 },
	{ 0x1C, 0x16 },
	{ 0x1D, 0x06 },
	{ 0x1E, 0xE6 },
	{ 0x1C, 0x17 },
	{ 0x1D, 0x16 },
	{ 0x1E, 0x06 },
	{ 0x1C, 0x18 },
	{ 0x1D, 0x30 },
	{ 0x1E, 0x01 },
	{ 0x1C, 0x19 },
	{ 0x1D, 0x21 },
	{ 0x1E, 0x37 },
	{ 0x1C, 0x1A },
	{ 0x1D, 0x00 },
	{ 0x1E, 0x08 },
	{ 0x1C, 0x1B },
	{ 0x1D, 0x80 },
	{ 0x1E, 0xD5 },
	{ 0xA2, 0x50 },
	{ 0xA2, 0x50 },
	{ 0x81, 0x01 }
};

static struct aes_regwrite strip_scan_reqs[] = {
	{ 0xBE, 0x23 },
	{ 0x29, 0x04 },
	{ 0x2A, 0xFF },
	{ 0xBD, 0x4F },
	{ 0xFF, 0x00 }
};

static const struct aes_regwrite capture_stop[] = {
	{ 0x81,0x00 }
};

/*
 * The different possible values for 0xBE register */
static unsigned char list_BE_values[10] = {
	0x23, 0x43, 0x63, 0x64, 0x65, 0x67, 0x6A, 0x6B
};

/*
 * The different possible values for 0xBD register */
static unsigned char list_BD_values[10] = {
	0x28, 0x2b, 0x30, 0x3b, 0x45, 0x49, 0x4B
};

/*
 * Adjust the gain according to the histogram data
 * 0xbd, 0xbe, 0x29 and 0x2A registers are affected
 * Returns 0 if no problem occured
 * TODO: This is a basic support for gain. It needs testing/tweaking.  */
static int adjust_gain(unsigned char *buffer, int status)
{
	// The position in the array of possible values for 0xBE and 0xBD registers
	static int pos_list_BE = 0;
	static int pos_list_BD = 0;

	// This is the first adjustement (we begin acquisition)
	// We adjust strip_scan_reqs for future strips and capture_reqs that is sent just after this step
	if (status == GAIN_STATUS_FIRST) {
		if (buffer[1] > 0x78) { // maximum gain needed
			strip_scan_reqs[0].value = 0x6B;
			strip_scan_reqs[1].value = 0x06;
			strip_scan_reqs[2].value = 0x35;
			strip_scan_reqs[3].value = 0x4B;
		}
		else if (buffer[1] > 0x55) {
			strip_scan_reqs[0].value = 0x63;
			strip_scan_reqs[1].value = 0x15;
			strip_scan_reqs[2].value = 0x35;
			strip_scan_reqs[3].value = 0x3b;
		}
		else if (buffer[1] > 0x40 || buffer[16] > 0x19) {
			strip_scan_reqs[0].value = 0x43;
			strip_scan_reqs[1].value = 0x13;
			strip_scan_reqs[2].value = 0x35;
			strip_scan_reqs[3].value = 0x30;
		}
		else { // minimum gain needed
			strip_scan_reqs[0].value = 0x23;
			strip_scan_reqs[1].value = 0x07;
			strip_scan_reqs[2].value = 0x35;
			strip_scan_reqs[3].value = 0x28;
		}

		// Now copy this values in capture_reqs
		capture_reqs[8].value = strip_scan_reqs[0].value;
		capture_reqs[9].value = strip_scan_reqs[1].value;
		capture_reqs[10].value = strip_scan_reqs[2].value;
		capture_reqs[21].value = strip_scan_reqs[3].value;

		fp_dbg("first gain: %x %x %x %x %x %x %x %x", strip_scan_reqs[0].reg, strip_scan_reqs[0].value, strip_scan_reqs[1].reg, strip_scan_reqs[1].value, strip_scan_reqs[2].reg, strip_scan_reqs[2].value, strip_scan_reqs[3].reg, strip_scan_reqs[3].value);
	}

	// Every 2/3 strips
	// We try to soften big changes of the gain (at least for 0xBE and 0xBD
	// FIXME: This softenning will need testing and tweaking too
	else if (status == GAIN_STATUS_NORMAL) {
		if (buffer[514] > 0x78) { // maximum gain needed
			if (pos_list_BE < 7)
				pos_list_BE++;

			if (pos_list_BD < 6)
				pos_list_BD++;

			strip_scan_reqs[1].value = 0x04;
			strip_scan_reqs[2].value = 0x35;
		}
		else if (buffer[514] > 0x55) {
			if (pos_list_BE < 2)
				pos_list_BE++;
			else if (pos_list_BE > 2)
				pos_list_BE--;

			if (pos_list_BD < 2)
				pos_list_BD++;
			else if (pos_list_BD > 2)
				pos_list_BD--;

			strip_scan_reqs[1].value = 0x15;
			strip_scan_reqs[2].value = 0x35;
		}
		else if (buffer[514] > 0x40 || buffer[529] > 0x19) {
			if (pos_list_BE < 1)
				pos_list_BE++;
			else if (pos_list_BE > 1)
				pos_list_BE--;

			if (pos_list_BD < 1)
				pos_list_BD++;
			else if (pos_list_BD > 1)
				pos_list_BD--;

			strip_scan_reqs[1].value = 0x13;
			strip_scan_reqs[2].value = 0x35;
		}
		else { // minimum gain needed
			if (pos_list_BE > 0)
				pos_list_BE--;

			if (pos_list_BD > 0)
				pos_list_BD--;

			strip_scan_reqs[1].value = 0x07;
			strip_scan_reqs[2].value = 0x35;
		}

		strip_scan_reqs[0].value = list_BE_values[pos_list_BE];
		strip_scan_reqs[3].value = list_BD_values[pos_list_BD];

		fp_dbg("gain: %x %x %x %x %x %x %x %x", strip_scan_reqs[0].reg, strip_scan_reqs[0].value, strip_scan_reqs[1].reg, strip_scan_reqs[1].value, strip_scan_reqs[2].reg, strip_scan_reqs[2].value, strip_scan_reqs[3].reg, strip_scan_reqs[3].value);
	}
	// Unknown status
	else {
		fp_err("Unexpected gain status.");
		return 1;
	}

	return 0;
}

/*
 * Restore the default gain values */
static void restore_gain(void)
{
	strip_scan_reqs[0].value = list_BE_values[0];
	strip_scan_reqs[1].value = 0x04;
	strip_scan_reqs[2].value = 0xFF;
	strip_scan_reqs[3].value = list_BD_values[0];

	capture_reqs[8].value = list_BE_values[0];
	capture_reqs[9].value = 0x04;
	capture_reqs[10].value = 0xFF;
	capture_reqs[21].value = list_BD_values[0];
}


/* capture SM movement:
 * request and read strip,
 * jump back to request UNLESS theres no finger, in which case exit SM,
 * report lack of finger presence, and move to finger detection */

enum capture_states {
	CAPTURE_WRITE_REQS,
	CAPTURE_READ_DATA,
	CAPTURE_REQUEST_STRIP,
	CAPTURE_READ_STRIP,
	CAPTURE_NUM_STATES,
};

static void capture_read_strip_cb(struct libusb_transfer *transfer)
{
	unsigned char *stripdata;
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct aes1610_dev *aesdev = dev->priv;
	unsigned char *data = transfer->buffer;
	int sum, i;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	} else if (transfer->length != transfer->actual_length) {
		fpi_ssm_mark_aborted(ssm, -EPROTO);
		goto out;
	}

	/* FIXME: would preallocating strip buffers be a decent optimization? */

	sum = 0;
	for (i = 516; i < 530; i++)
	{
		/* histogram[i] = number of pixels of value i
		   Only the pixel values from 10 to 15 are used to detect finger. */
		sum += data[i];
	}

	if (sum > 0) {
		/* FIXME: would preallocating strip buffers be a decent optimization? */
		struct fpi_frame *stripe = g_malloc(FRAME_WIDTH * (FRAME_HEIGHT / 2) + sizeof(struct fpi_frame));
		stripe->delta_x = 0;
		stripe->delta_y = 0;
		stripdata = stripe->data;
		memcpy(stripdata, data + 1, FRAME_WIDTH * (FRAME_HEIGHT / 2));
		aesdev->strips = g_slist_prepend(aesdev->strips, stripe);
		aesdev->strips_len++;
		aesdev->blanks_count = 0;
	}

	if (sum < 0) {
		fpi_ssm_mark_aborted(ssm, sum);
		goto out;
	}
	fp_dbg("sum=%d", sum);

	/* FIXME: 0 might be too low as a threshold */
	/* FIXME: sometimes we get 0 in the middle of a scan, should we wait for
	 * a few consecutive zeroes? */

	/* If sum is 0 for a reasonable # of frames, finger has been removed */
	if (sum == 0) {
		aesdev->blanks_count++;
		fp_dbg("got blank frame");
	}

	/* use histogram data above for gain calibration (0xbd, 0xbe, 0x29 and 0x2A ) */
	adjust_gain(data, GAIN_STATUS_NORMAL);

	/* stop capturing if MAX_FRAMES is reached */
	if (aesdev->blanks_count > 10 || g_slist_length(aesdev->strips) >= MAX_FRAMES) {
		struct fp_img *img;

		fp_dbg("sending stop capture.... blanks=%d  frames=%d", aesdev->blanks_count, g_slist_length(aesdev->strips));
		/* send stop capture bits */
		aes_write_regv(dev, capture_stop, G_N_ELEMENTS(capture_stop), stub_capture_stop_cb, NULL);
		aesdev->strips = g_slist_reverse(aesdev->strips);
		fpi_do_movement_estimation(&assembling_ctx, aesdev->strips, aesdev->strips_len);
		img = fpi_assemble_frames(&assembling_ctx, aesdev->strips, aesdev->strips_len);
		img->flags |= FP_IMG_PARTIAL;
		g_slist_free_full(aesdev->strips, g_free);
		aesdev->strips = NULL;
		aesdev->strips_len = 0;
		aesdev->blanks_count = 0;
		fpi_imgdev_image_captured(dev, img);
		fpi_imgdev_report_finger_status(dev, FALSE);
		/* marking machine complete will re-trigger finger detection loop */
		fpi_ssm_mark_completed(ssm);
		/* Acquisition finished: restore default gain values */
		restore_gain();
	} else {
		/* obtain next strip */
		fpi_ssm_jump_to_state(ssm, CAPTURE_REQUEST_STRIP);
	}

out:
	g_free(data);
	libusb_free_transfer(transfer);
}

static void capture_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct aes1610_dev *aesdev = dev->priv;
	int r;

	switch (ssm->cur_state) {
	case CAPTURE_WRITE_REQS:
		fp_dbg("write reqs");
		aes_write_regv(dev, capture_reqs, G_N_ELEMENTS(capture_reqs),
			generic_write_regv_cb, ssm);
		break;
	case CAPTURE_READ_DATA:
		fp_dbg("read data");
		generic_read_ignore_data(ssm, 665);
		break;
	case CAPTURE_REQUEST_STRIP:
		fp_dbg("request strip");
		if (aesdev->deactivating)
			fpi_ssm_mark_completed(ssm);
		else
			aes_write_regv(dev, strip_scan_reqs, G_N_ELEMENTS(strip_scan_reqs),
				generic_write_regv_cb, ssm);
		break;
	case CAPTURE_READ_STRIP: ;
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		unsigned char *data;

		if (!transfer) {
			fpi_ssm_mark_aborted(ssm, -ENOMEM);
			break;
		}

		data = g_malloc(665);
		libusb_fill_bulk_transfer(transfer, dev->udev, EP_IN, data, 665,
			capture_read_strip_cb, ssm, BULK_TIMEOUT);

		r = libusb_submit_transfer(transfer);
		if (r < 0) {
			g_free(data);
			libusb_free_transfer(transfer);
			fpi_ssm_mark_aborted(ssm, r);
		}
		break;
	};
}

static void capture_sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct aes1610_dev *aesdev = dev->priv;

	fp_dbg("");
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
	struct aes1610_dev *aesdev = dev->priv;
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

static const struct aes_regwrite init[] = {
	{ 0x82, 0x00 }
};

static const struct aes_regwrite stop_reader[] = {
	{ 0xFF, 0x00 }
};


enum activate_states {
	WRITE_INIT,
	ACTIVATE_NUM_STATES,
};

static void activate_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;

	/* activation on aes1610 seems much more straightforward compared to aes2501 */
	/* verify theres anything missing here */
	switch (ssm->cur_state) {
	case WRITE_INIT:
		fp_dbg("write init");
		aes_write_regv(dev, init, G_N_ELEMENTS(init), generic_write_regv_cb, ssm);
		break;
	}
}

/* jump to finger detection */
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
	struct aes1610_dev *aesdev = dev->priv;
	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, activate_run_state,
		ACTIVATE_NUM_STATES);
	ssm->priv = dev;
	aesdev->read_regs_retry_count = 0;
	fpi_ssm_start(ssm, activate_sm_complete);
	return 0;
}

static void dev_deactivate(struct fp_img_dev *dev)
{
	struct aes1610_dev *aesdev = dev->priv;
	/* FIXME: audit cancellation points, probably need more, specifically
	 * in error handling paths? */
	aesdev->deactivating = TRUE;
}

static void complete_deactivation(struct fp_img_dev *dev)
{
	struct aes1610_dev *aesdev = dev->priv;
	fp_dbg("");

	/* FIXME: if we're in the middle of a scan, we should cancel the scan.
	 * maybe we can do this with a master reset, unconditionally? */

	aesdev->deactivating = FALSE;
	g_slist_free(aesdev->strips);
	aesdev->strips = NULL;
	aesdev->strips_len = 0;
	aesdev->blanks_count = 0;
	fpi_imgdev_deactivate_complete(dev);
}

static int dev_init(struct fp_img_dev *dev, unsigned long driver_data)
{
	/* FIXME check endpoints */
	int r;

	r = libusb_claim_interface(dev->udev, 0);
	if (r < 0) {
		fp_err("could not claim interface 0: %s", libusb_error_name(r));
		return r;
	}

	dev->priv = g_malloc0(sizeof(struct aes1610_dev));
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
	{ .vendor = 0x08ff, .product = 0x1600 }, /* AES1600 */
	{ 0, 0, 0, },
};

struct fp_img_driver aes1610_driver = {
	.driver = {
		.id = AES1610_ID,
		.name = FP_COMPONENT,
		.full_name = "AuthenTec AES1610",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_SWIPE,
	},
	.flags = 0,
	.img_height = -1,
	.img_width = IMAGE_WIDTH,

	.bz3_threshold = 20,

	.open = dev_init,
	.close = dev_deinit,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};

