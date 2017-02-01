/*
 * AuthenTec AES3500 driver for libfprint
 *
 * AES3500 is a press-typed sensor, which captures image in 128x128
 * pixels.
 *
 * Thanks Rafael Toledo for the Windows driver and the help.
 *
 * This work is derived from Daniel Drake's AES4000 driver.
 *
 * Copyright (C) 2011-2013 Juvenn Woo <machese@gmail.com>
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
 */

#define FP_COMPONENT "aes3500"

#include <errno.h>

#include <glib.h>
#include <libusb.h>

#include <aeslib.h>
#include <fp_internal.h>

#include "aes3k.h"
#include "driver_ids.h"

#define DATA_BUFLEN	0x2089

/* image size = FRAME_WIDTH x FRAME_WIDTH */
#define FRAME_WIDTH 	128
#define FRAME_SIZE	(FRAME_WIDTH * AES3K_FRAME_HEIGHT / 2)
#define FRAME_NUMBER	(FRAME_WIDTH / AES3K_FRAME_HEIGHT)
#define ENLARGE_FACTOR 	2


static struct aes_regwrite init_reqs[] = {
	/* master reset */
	{ 0x80, 0x01 },
	{ 0, 0 },
	{ 0x80, 0x00 },
	{ 0, 0 },

	{ 0x81, 0x00 },
	{ 0x80, 0x00 },
	{ 0, 0 },

	/* scan reset */
	{ 0x80, 0x02 },
	{ 0, 0 },
	{ 0x80, 0x00 },
	{ 0, 0 },

	/* disable register buffering */
	{ 0x80, 0x04 },
	{ 0, 0 },
	{ 0x80, 0x00 },
	{ 0, 0 },

	{ 0x81, 0x00 },
	{ 0, 0 },
	/* windows driver reads registers now (81 02) */
	{ 0x80, 0x00 },
	{ 0x81, 0x00 },

	/* set excitation bias current: 2mhz drive ring frequency,
	 * 4V drive ring voltage, 16.5mA excitation bias */
	{ 0x82, 0x04 },

	/* continuously sample drive ring for finger detection,
	 * 62.50ms debounce delay */
	{ 0x83, 0x13 },

	{ 0x84, 0x07 }, /* set calibration resistance to 12 kiloohms */
	{ 0x85, 0x3d }, /* set calibration capacitance */
	{ 0x86, 0x03 }, /* detect drive voltage */
	{ 0x87, 0x01 }, /* set detection frequency to 125khz */
	{ 0x88, 0x02 }, /* set column scan period */
	{ 0x89, 0x02 }, /* set measure drive */
	{ 0x8a, 0x33 }, /* set measure frequency and sense amplifier bias */
	{ 0x8b, 0x33 }, /* set matrix pattern */
	{ 0x8c, 0x0f }, /* set demodulation phase 1 */
	{ 0x8d, 0x04 }, /* set demodulation phase 2 */
	{ 0x8e, 0x23 }, /* set sensor gain */
	{ 0x8f, 0x07 }, /* set image parameters */
	{ 0x90, 0x00 }, /* carrier offset null */
	{ 0x91, 0x1c }, /* set A/D reference high */
	{ 0x92, 0x08 }, /* set A/D reference low */
	{ 0x93, 0x00 }, /* set start row to 0 */
	{ 0x94, 0x07 }, /* set end row */
	{ 0x95, 0x00 }, /* set start column to 0 */
	{ 0x96, 0x1f }, /* set end column */
	{ 0x97, 0x04 }, /* data format and thresholds */
	{ 0x98, 0x28 }, /* image data control */
	{ 0x99, 0x00 }, /* disable general purpose outputs */
	{ 0x9a, 0x0b }, /* set initial scan state */
	{ 0x9b, 0x00 }, /* clear challenge word bits */
	{ 0x9c, 0x00 }, /* clear challenge word bits */
	{ 0x9d, 0x09 }, /* set some challenge word bits */
	{ 0x9e, 0x53 }, /* clear challenge word bits */
	{ 0x9f, 0x6b }, /* set some challenge word bits */
	{ 0, 0 },

	{ 0x80, 0x00 },
	{ 0x81, 0x00 },
	{ 0, 0 },
	{ 0x81, 0x04 },
	{ 0, 0 },
	{ 0x81, 0x00 },
};

static int dev_init(struct fp_img_dev *dev, unsigned long driver_data)
{
	int r;
	struct aes3k_dev *aesdev;

	r = libusb_claim_interface(dev->udev, 0);
	if (r < 0) {
		fp_err("could not claim interface 0: %s", libusb_error_name(r));
		return r;
	}

	aesdev = dev->priv = g_malloc0(sizeof(struct aes3k_dev));

	if (!aesdev)
		return -ENOMEM;

	aesdev->data_buflen = DATA_BUFLEN;
	aesdev->frame_width = FRAME_WIDTH;
	aesdev->frame_size = FRAME_SIZE;
	aesdev->frame_number = FRAME_NUMBER;
	aesdev->enlarge_factor = ENLARGE_FACTOR;
	aesdev->init_reqs = init_reqs;
	aesdev->init_reqs_len = G_N_ELEMENTS(init_reqs);
	fpi_imgdev_open_complete(dev, 0);

	return r;
}

static void dev_deinit(struct fp_img_dev *dev)
{
	struct aes3k_dev *aesdev = dev->priv;
	g_free(aesdev);
	libusb_release_interface(dev->udev, 0);
	fpi_imgdev_close_complete(dev);
}


static const struct usb_id id_table[] = {
	{ .vendor = 0x08ff, .product = 0x5731 },
	{ 0, 0, 0, },
};

struct fp_img_driver aes3500_driver = {
	.driver = {
		.id = AES3500_ID,
		.name = FP_COMPONENT,
		.full_name = "AuthenTec AES3500",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_PRESS,
	},
	.flags = 0,
	.img_height = FRAME_WIDTH * ENLARGE_FACTOR,
	.img_width = FRAME_WIDTH * ENLARGE_FACTOR,

	/* temporarily lowered until image quality improves */
	.bz3_threshold = 9,

	.open = dev_init,
	.close = dev_deinit,
	.activate = aes3k_dev_activate,
	.deactivate = aes3k_dev_deactivate,
};

