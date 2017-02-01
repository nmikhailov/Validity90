/*
 * Secugen FDU2000 driver for libfprint
 * Copyright (C) 2007 Gustavo Chain <g@0xff.cl>
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

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <libusb.h>

#define FP_COMPONENT "fdu2000"
#include <fp_internal.h>

#include "driver_ids.h"

#ifndef HAVE_MEMMEM
gpointer
memmem(const gpointer haystack, size_t haystack_len, const gpointer needle, size_t needle_len) {
	const gchar *begin;
	const char *const last_possible = (const char *) haystack + haystack_len - needle_len;

	/* The first occurrence of the empty string is deemed to occur at 
	 * the beginning of the string. */
	if (needle_len == 0)
		return (void *) haystack;
	
	/* Sanity check, otherwise the loop might search through the whole
	 * memory.  */
	if (haystack_len < needle_len)
		return NULL;
	
	for (begin = (const char *) haystack; begin <= last_possible; ++begin)
		if (begin[0] == ((const char *) needle)[0] &&
			!memcmp((const void *) &begin[1],
				(const void *) ((const char *) needle + 1),
				needle_len - 1))
			return (void *) begin;
	
	return NULL;
}
#endif	/* HAVE_MEMMEM */

#define EP_IMAGE	( 0x02 | LIBUSB_ENDPOINT_IN )
#define EP_REPLY	( 0x01 | LIBUSB_ENDPOINT_IN )
#define EP_CMD		( 0x01 | LIBUSB_ENDPOINT_OUT )
#define BULK_TIMEOUT	200

/* fdu_req[] index */
typedef enum {
	CAPTURE_READY,
	CAPTURE_READ,
	CAPTURE_END,
	LED_OFF,
	LED_ON
} req_index;


#define CMD_LEN 2
#define ACK_LEN 8
static const struct fdu2000_req {
	const gchar cmd[CMD_LEN];	// Command to send
	const gchar ack[ACK_LEN];	// Expected ACK
	const guint ack_len;	// ACK has variable length
} fdu_req[] = {
	/* Capture */
	{
		.cmd = { 0x00, 0x04 },
		.ack = { 0x00, 0x04, 0x01, 0x01 },
		.ack_len = 4
	},
	
	{
		.cmd = { 0x00, 0x01 },
		.ack = { 0x00, 0x01, 0x01, 0x01 },
		.ack_len = 4
	},

	{
		.cmd = { 0x00, 0x05 },
		.ack = { 0x00, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
		.ack_len = 8
	},

	/* Led */
	{
		.cmd = { 0x05, 0x00 },
		.ack = {},
		.ack_len = 0
	},

	{
		.cmd = { 0x05, 0x01 },
		.ack = {},
		.ack_len = 0
	}
};

/*
 * Write a command and verify reponse
 */
static gint
bulk_write_safe(libusb_dev_handle *dev, req_index rIndex) {

	gchar reponse[ACK_LEN];
	gint r;
	gchar *cmd = (gchar *)fdu_req[rIndex].cmd;
	gchar *ack = (gchar *)fdu_req[rIndex].ack;
	gint ack_len = fdu_req[rIndex].ack_len;
	struct libusb_bulk_transfer wrmsg = {
		.endpoint = EP_CMD,
		.data = cmd,
		.length = sizeof(cmd),
	};
	struct libusb_bulk_transfer readmsg = {
		.endpoint = EP_REPLY,
		.data = reponse,
		.length = sizeof(reponse),
	};
	int trf;

	r = libusb_bulk_transfer(dev, &wrmsg, &trf, BULK_TIMEOUT);
	if (r < 0)
		return r;

	if (ack_len == 0)
		return 0;

	/* Check reply from FP */
	r = libusb_bulk_transfer(dev, &readmsg, &trf, BULK_TIMEOUT);
	if (r < 0)
		return r;

	if (!strncmp(ack, reponse, ack_len))
		return 0;
	
	fp_err("Expected different ACK from dev");
	return 1;	/* Error */
}

static gint
capture(struct fp_img_dev *dev, gboolean unconditional,
	struct fp_img **ret)
{
#define RAW_IMAGE_WIDTH		398
#define RAW_IMAGE_HEIGTH	301
#define RAW_IMAGE_SIZE		(RAW_IMAGE_WIDTH * RAW_IMAGE_HEIGTH)

	struct fp_img *img = NULL;
	int bytes, r;
	const gchar SOF[] = { 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x0c, 0x07 };  // Start of frame
	const gchar SOL[] = { 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x0b, 0x06 };  // Start of line + { L L } (L: Line num) (8 nibbles)
	gchar *buffer = g_malloc0(RAW_IMAGE_SIZE * 6);
	gchar *image;
	gchar *p;
	guint offset;
	struct libusb_bulk_transfer msg = {
		.endpoint = EP_IMAGE,
		.data = buffer,
		.length = RAW_IMAGE_SIZE * 6,
	};

	image  = g_malloc0(RAW_IMAGE_SIZE);

	if ((r = bulk_write_safe(dev->udev, LED_ON))) {
		fp_err("Command: LED_ON");
		goto out;
	}
	
	if ((r = bulk_write_safe(dev->udev, CAPTURE_READY))) {
		fp_err("Command: CAPTURE_READY");
		goto out;
	}

read:	
	if ((r = bulk_write_safe(dev->udev, CAPTURE_READ))) {
		fp_err("Command: CAPTURE_READ");
		goto out;
	}

	/* Now we are ready to read from dev */

	r = libusb_bulk_transfer(dev->udev, &msg, &bytes, BULK_TIMEOUT * 10);
	if (r < 0 || bytes < 1)
		goto read;

	/*
	 * Find SOF (start of line)
	 */
	p = memmem(buffer, RAW_IMAGE_SIZE * 6,
			(const gpointer)SOF, sizeof SOF);
	fp_dbg("Read %d byte/s from dev", bytes);
	if (!p)
		goto out;

	p += sizeof SOF;

	int i = 0;
	bytes = 0;
	while(p) {
		if ( i >= RAW_IMAGE_HEIGTH )
			break;

		offset = p - buffer;
		p = memmem(p, (RAW_IMAGE_SIZE * 6) - (offset),
				(const gpointer)SOL, sizeof SOL);
		if (p) {
			p += sizeof SOL + 4;
			int j;
			for (j = 0; j < RAW_IMAGE_WIDTH; j++) {
				/**
				 * Convert from 4 to 8 bits
				 * The SECUGEN-FDU2000 has 4 lines of data, so we need to join 2 bytes into 1
				 */
				*(image + bytes + j)  = *(p + (j * 2) + 0) << 4 & 0xf0;
				*(image + bytes + j) |= *(p + (j * 2) + 1) & 0x0f;
			}
			p += RAW_IMAGE_WIDTH * 2;
			bytes += RAW_IMAGE_WIDTH;
			i++;
		}
	}

	if ((r = bulk_write_safe(dev->udev, CAPTURE_END))) {
		fp_err("Command: CAPTURE_END");
		goto out;
	}

	if ((r = bulk_write_safe(dev->udev, LED_OFF))) {
		fp_err("Command: LED_OFF");
		goto out;
	}

	img = fpi_img_new_for_imgdev(dev);
	memcpy(img->data, image, RAW_IMAGE_SIZE);
	img->flags = FP_IMG_COLORS_INVERTED | FP_IMG_V_FLIPPED | FP_IMG_H_FLIPPED;
	*ret = img;

out:
	g_free(buffer);
	g_free(image);

	return r;
}

static
gint dev_init(struct fp_img_dev *dev, unsigned long driver_data)
{
	gint r;
	//if ( (r = usb_set_configuration(dev->udev, 1)) < 0 )
	//	goto out;

	if ( (r = libusb_claim_interface(dev->udev, 0)) < 0 ) {
		fp_err("could not claim interface 0: %s", libusb_error_name(r));
		return r;
	}

	//if ( (r = usb_set_altinterface(dev->udev, 1)) < 0 )
	//	goto out;

	//if ( (r = usb_clear_halt(dev->udev, EP_CMD)) < 0 )
	//	goto out;

	/* Make sure sensor mode is not capture_{ready|read} */
	if ((r = bulk_write_safe(dev->udev, CAPTURE_END))) {
		fp_err("Command: CAPTURE_END");
		goto out;
	}

	if ((r = bulk_write_safe(dev->udev, LED_OFF))) {
		fp_err("Command: LED_OFF");
		goto out;
	}

	return 0;

out:
	fp_err("could not init dev");
	return r;
}

static
void dev_exit(struct fp_img_dev *dev)
{
	if (bulk_write_safe(dev->udev, CAPTURE_END))
		fp_err("Command: CAPTURE_END");

	libusb_release_interface(dev->udev, 0);
}

static const struct usb_id id_table[] = {
	{ .vendor = 0x1162, .product = 0x0300 },
	{ 0, 0, 0, },
};

struct fp_img_driver fdu2000_driver = {
	.driver = {
		.id = FDU2000_ID,
		.name = FP_COMPONENT,
		.full_name = "Secugen FDU 2000",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_PRESS,
	},
	.img_height = RAW_IMAGE_HEIGTH,
	.img_width = RAW_IMAGE_WIDTH,
	.bz3_threshold = 23,

	.init = dev_init,
	.exit = dev_exit,
	.capture = capture,
};
