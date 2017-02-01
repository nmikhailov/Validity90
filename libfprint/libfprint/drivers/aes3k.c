/*
 * AuthenTec AES3500/AES4000 common routines
 *
 * The AES3500 and AES4000 sensors are press-typed, and could capture
 * fingerprint images in 128x128 and 96x96 pixels respectively. They
 * share a same communication interface: a number of frames are
 * transferred and captured, from which a final image could be
 * assembled. Each frame has fixed height of 16 pixels.
 *
 * As the imaging area is a bit small, only a part of finger could be
 * captured, the detected minutiae are not so many that the NBIS
 * matching works not so good. The verification rate is very low at the
 * moment.
 *
 * This work is derived from Daniel Drake's AES4000 driver.
 *
 * Copyright (C) 2013 Juvenn Woo <machese@gmail.com>
 * Copyright (C) 2007-2008 Daniel Drake <dsd@gentoo.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#define FP_COMPONENT "aes3k"

#include <errno.h>

#include <glib.h>
#include <libusb.h>

#include <aeslib.h>
#include <fp_internal.h>

#include "aes3k.h"

#define CTRL_TIMEOUT	1000
#define EP_IN		(1 | LIBUSB_ENDPOINT_IN)
#define EP_OUT		(2 | LIBUSB_ENDPOINT_OUT)

static void do_capture(struct fp_img_dev *dev);

static void aes3k_assemble_image(unsigned char *input, size_t width, size_t height,
	unsigned char *output)
{
	size_t row, column;

	for (column = 0; column < width; column++) {
		for (row = 0; row < height; row += 2) {
			output[width * row + column] = (*input & 0x0f) * 17;
			output[width * (row + 1) + column] = ((*input & 0xf0) >> 4) * 17;
			input++;
		}
	}
}

static void img_cb(struct libusb_transfer *transfer)
{
	struct fp_img_dev *dev = transfer->user_data;
	struct aes3k_dev *aesdev = dev->priv;
	unsigned char *ptr = transfer->buffer;
	struct fp_img *tmp;
	struct fp_img *img;
	int i;

	if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
		goto err;
	} else if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fpi_imgdev_session_error(dev, -EIO);
		goto err;
	} else if (transfer->length != transfer->actual_length) {
		fpi_imgdev_session_error(dev, -EPROTO);
		goto err;
	}

	fpi_imgdev_report_finger_status(dev, TRUE);

	tmp = fpi_img_new(aesdev->frame_width * aesdev->frame_width);
	tmp->width = aesdev->frame_width;
	tmp->height = aesdev->frame_width;
	tmp->flags = FP_IMG_COLORS_INVERTED | FP_IMG_V_FLIPPED | FP_IMG_H_FLIPPED;
	for (i = 0; i < aesdev->frame_number; i++) {
		fp_dbg("frame header byte %02x", *ptr);
		ptr++;
		aes3k_assemble_image(ptr, aesdev->frame_width, AES3K_FRAME_HEIGHT, tmp->data + (i * aesdev->frame_width * AES3K_FRAME_HEIGHT));
		ptr += aesdev->frame_size;
	}

	/* FIXME: this is an ugly hack to make the image big enough for NBIS
	 * to process reliably */
	img = fpi_im_resize(tmp, aesdev->enlarge_factor, aesdev->enlarge_factor);
	fp_img_free(tmp);
	fpi_imgdev_image_captured(dev, img);

	/* FIXME: rather than assuming finger has gone, we should poll regs until
	 * it really has, then restart the capture */
	fpi_imgdev_report_finger_status(dev, FALSE);

	do_capture(dev);

err:
	g_free(transfer->buffer);
	aesdev->img_trf = NULL;
	libusb_free_transfer(transfer);
}

static void do_capture(struct fp_img_dev *dev)
{
	struct aes3k_dev *aesdev = dev->priv;
	unsigned char *data;
	int r;

	aesdev->img_trf = libusb_alloc_transfer(0);
	if (!aesdev->img_trf) {
		fpi_imgdev_session_error(dev, -EIO);
		return;
	}

	data = g_malloc(aesdev->data_buflen);
	libusb_fill_bulk_transfer(aesdev->img_trf, dev->udev, EP_IN, data,
		aesdev->data_buflen, img_cb, dev, 0);

	r = libusb_submit_transfer(aesdev->img_trf);
	if (r < 0) {
		g_free(data);
		libusb_free_transfer(aesdev->img_trf);
		aesdev->img_trf = NULL;
		fpi_imgdev_session_error(dev, r);
	}
}

static void init_reqs_cb(struct fp_img_dev *dev, int result, void *user_data)
{
	fpi_imgdev_activate_complete(dev, result);
	if (result == 0)
		do_capture(dev);
}

int aes3k_dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct aes3k_dev *aesdev = dev->priv;
	aes_write_regv(dev, aesdev->init_reqs, aesdev->init_reqs_len, init_reqs_cb, NULL);
	return 0;
}

void aes3k_dev_deactivate(struct fp_img_dev *dev)
{
	struct aes3k_dev *aesdev = dev->priv;

	/* FIXME: should wait for cancellation to complete before returning
	 * from deactivation, otherwise app may legally exit before we've
	 * cleaned up */
	if (aesdev->img_trf)
		libusb_cancel_transfer(aesdev->img_trf);
	fpi_imgdev_deactivate_complete(dev);
}

