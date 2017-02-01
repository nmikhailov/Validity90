/*
 * vfs301/vfs300 fingerprint reader driver
 * https://github.com/andree182/vfs301
 *
 * Copyright (c) 2011-2012 Andrej Krutak <dev@andree.sk>
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

/*
 * TODO:
 * - async communication everywhere :)
 * - protocol decyphering
 *   - what is needed and what is redundant
 *   - is some part of the initial data the firmware?
 *   - describe some interesting structures better
 */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

#include "vfs301_proto.h"
#include "vfs301_proto_fragments.h"
#include <unistd.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))

/************************** USB STUFF *****************************************/

#ifdef DEBUG
static void usb_print_packet(int dir, int rv, const unsigned char *data, int length)
{
	fprintf(stderr, "%s, rv %d, len %d\n", dir ? "send" : "recv", rv, length);

#ifdef PRINT_VERBOSE
	int i;

	for (i = 0; i < min(length, 128); i++) {
		fprintf(stderr, "%.2X ", data[i]);
		if (i % 8 == 7)
			fprintf(stderr, " ");
		if (i % 32 == 31)
			fprintf(stderr, "\n");
	}
#endif

	fprintf(stderr, "\n");
}
#endif

static int usb_recv(
	vfs301_dev_t *dev,
	struct libusb_device_handle *devh, unsigned char endpoint, int max_bytes)
{
	assert(max_bytes <= sizeof(dev->recv_buf));

	int r = libusb_bulk_transfer(
		devh, endpoint,
		dev->recv_buf, max_bytes,
		&dev->recv_len, VFS301_DEFAULT_WAIT_TIMEOUT
	);

#ifdef DEBUG
	usb_print_packet(0, r, dev->recv_buf, dev->recv_len);
#endif

	if (r < 0)
		return r;
	return 0;
}

static int usb_send(
	struct libusb_device_handle *devh, const unsigned char *data, int length)
{
	int transferred = 0;

	int r = libusb_bulk_transfer(
		devh, VFS301_SEND_ENDPOINT,
		(unsigned char *)data, length, &transferred, VFS301_DEFAULT_WAIT_TIMEOUT
	);

#ifdef DEBUG
	usb_print_packet(1, r, data, length);
#endif

	assert(r == 0);

	if (r < 0)
		return r;
	if (transferred < length)
		return r;

	return 0;
}

/************************** OUT MESSAGES GENERATION ***************************/

static void vfs301_proto_generate_0B(int subtype, unsigned char *data, int *len)
{
	*data = 0x0B;
	*len = 1;
	data++;

	memset(data, 0, 39);
	*len += 38;

	data[20] = subtype;

	switch (subtype) {
	case 0x04:
		data[34] = 0x9F;
		break;
	case 0x05:
		data[34] = 0xAB;
		len++;
		break;
	default:
		assert(!"unsupported");
		break;
	}
}

#define HEX_TO_INT(c) \
	(((c) >= '0' && (c) <= '9') ? ((c) - '0') : ((c) - 'A' + 10))

static void translate_str(const char **srcL, unsigned char *data, int *len)
{
	const char *src;
	unsigned char *dataOrig = data;

	while (*srcL != NULL) {
		src = *srcL;
		while (*src != '\0') {
			assert(*src != '\0');
			assert(*(src +1) != '\0');
			*data = (unsigned char)((HEX_TO_INT(*src) << 4) | (HEX_TO_INT(*(src + 1))));

			data++;
			src += 2;
		}

		srcL++;
	}

	*len = data - dataOrig;
}

static void vfs301_proto_generate(int type, int subtype, unsigned char *data, int *len)
{
	switch (type) {
	case 0x01:
	case 0x04:
		/* After cmd 0x04 is sent, a data is received on VALIDITY_RECEIVE_ENDPOINT_CTRL.
		 * If it is 0x0000:
		 *     additional 64B and 224B are read from _DATA, then vfs301_next_scan_FA00 is
		 *     sent, 0000 received from _CTRL, and then continue with wait loop
		 * If it is 0x1204:
		 *     => reinit?
		 */
	case 0x17:
	case 0x19:
	case 0x1A:
		*data = type;
		*len = 1;
		break;
	case 0x0B:
		vfs301_proto_generate_0B(subtype, data, len);
		break;
	case 0x02D0:
		{
			const char **dataLs[] = {
				vfs301_02D0_01,
				vfs301_02D0_02,
				vfs301_02D0_03,
				vfs301_02D0_04,
				vfs301_02D0_05,
				vfs301_02D0_06,
				vfs301_02D0_07,
			};
			assert((int)subtype <= (int)(sizeof(dataLs) / sizeof(dataLs[0])));
			translate_str(dataLs[subtype - 1], data, len);
		}
		break;
	case 0x0220:
		switch (subtype) {
		case 1:
			translate_str(vfs301_0220_01, data, len);
			break;
		case 2:
			translate_str(vfs301_0220_02, data, len);
			break;
		case 3:
			translate_str(vfs301_0220_03, data, len);
			break;
		case 0xFA00:
		case 0x2C01:
		case 0x5E01:
			translate_str(vfs301_next_scan_template, data, len);
			unsigned char *field = data + *len - (sizeof(S4_TAIL) - 1) / 2 - 4;

			assert(*field == 0xDE);
			assert(*(field + 1) == 0xAD);
			assert(*(field + 2) == 0xDE);
			assert(*(field + 3) == 0xAD);

			*field = (unsigned char)((subtype >> 8) & 0xFF);
			*(field + 1) = (unsigned char)(subtype & 0xFF);
			*(field + 2) = *field;
			*(field + 3) = *(field + 1);
			break;
		default:
			assert(0);
			break;
		}
		break;
	case 0x06:
		assert(!"Not generated");
		break;
	default:
		assert(!"Unknown message type");
		break;
	}
}

/************************** SCAN IMAGE PROCESSING *****************************/

#ifdef SCAN_FINISH_DETECTION
static int img_is_finished_scan(fp_line_t *lines, int no_lines)
{
	int i;
	int j;
	int rv = 1;

	for (i = no_lines - VFS301_FP_SUM_LINES; i < no_lines; i++) {
		/* check the line for fingerprint data */
		for (j = 0; j < sizeof(lines[i].sum2); j++) {
			if (lines[i].sum2[j] > (VFS301_FP_SUM_MEDIAN + VFS301_FP_SUM_EMPTY_RANGE))
				rv = 0;
		}
	}

	return rv;
}
#endif

static int scanline_diff(const unsigned char *scanlines, int prev, int cur)
{
	const unsigned char *line1 = scanlines + prev * VFS301_FP_OUTPUT_WIDTH;
	const unsigned char *line2 = scanlines + cur * VFS301_FP_OUTPUT_WIDTH;
	int i;
	int diff;

#ifdef OUTPUT_RAW
	/* We only need the image, not the surrounding stuff. */
	line1 = ((vfs301_line_t*)line1)->scan;
	line2 = ((vfs301_line_t*)line2)->scan;
#endif

	/* TODO: This doesn't work too well when there are parallel lines in the 
	 * fingerprint. */
	for (diff = 0, i = 0; i < VFS301_FP_WIDTH; i++) {
		if (*line1 > *line2)
			diff += *line1 - *line2;
		else
			diff += *line2 - *line1;

		line1++;
		line2++;
	}

	return ((diff / VFS301_FP_WIDTH) > VFS301_FP_LINE_DIFF_THRESHOLD);
}

/** Transform the input data to a normalized fingerprint scan */
void vfs301_extract_image(
	vfs301_dev_t *vfs, unsigned char *output, int *output_height
)
{
	const unsigned char *scanlines = vfs->scanline_buf;
	int last_line;
	int i;

	assert(vfs->scanline_count >= 1);

	*output_height = 1;
	memcpy(output, scanlines, VFS301_FP_OUTPUT_WIDTH);
	last_line = 0;

	/* The following algorithm is quite trivial - it just picks lines that
	 * differ more than VFS301_FP_LINE_DIFF_THRESHOLD.
	 * TODO: A nicer approach would be to pick those lines and then do some kind 
	 * of bi/tri-linear resampling to get the output (so that we don't get so
	 * many false edges etc.).
	 */
	for (i = 1; i < vfs->scanline_count; i++) {
		if (scanline_diff(scanlines, last_line, i)) {
			memcpy(
				output + VFS301_FP_OUTPUT_WIDTH * (*output_height),
				scanlines + VFS301_FP_OUTPUT_WIDTH * i,
				VFS301_FP_OUTPUT_WIDTH
			);
			last_line = i;
			(*output_height)++;
		}
	}
}

static int img_process_data(
	int first_block, vfs301_dev_t *dev, const unsigned char *buf, int len
)
{
	vfs301_line_t *lines = (vfs301_line_t*)buf;
	int no_lines = len / sizeof(vfs301_line_t);
	int i;
	/*int no_nonempty;*/
	unsigned char *cur_line;
	int last_img_height;
#ifdef SCAN_FINISH_DETECTION
	int finished_scan;
#endif

	if (first_block) {
		last_img_height = 0;
		dev->scanline_count = no_lines;
	} else {
		last_img_height = dev->scanline_count;
		dev->scanline_count += no_lines;
	}

	dev->scanline_buf = realloc(dev->scanline_buf, dev->scanline_count * VFS301_FP_OUTPUT_WIDTH);
	assert(dev->scanline_buf != NULL);

	for (cur_line = dev->scanline_buf + last_img_height * VFS301_FP_OUTPUT_WIDTH, i = 0;
		i < no_lines;
		i++, cur_line += VFS301_FP_OUTPUT_WIDTH
	) {
#ifndef OUTPUT_RAW
		memcpy(cur_line, lines[i].scan, VFS301_FP_OUTPUT_WIDTH);
#else
		memcpy(cur_line, &lines[i], VFS301_FP_OUTPUT_WIDTH);
#endif
	}

#ifdef SCAN_FINISH_DETECTION
	finished_scan = img_is_finished_scan(lines, no_lines);

	return !finished_scan;
#else /* SCAN_FINISH_DETECTION */
	return 1; /* Just continue until data is coming */
#endif
}

/************************** PROTOCOL STUFF ************************************/

static unsigned char usb_send_buf[0x2000];

#define USB_RECV(from, len) \
	usb_recv(dev, devh, from, len)

#define USB_SEND(type, subtype) \
	{ \
		int len; \
		vfs301_proto_generate(type, subtype, usb_send_buf, &len); \
		usb_send(devh, usb_send_buf, len); \
	}

#define RAW_DATA(x) x, sizeof(x)

#define IS_VFS301_FP_SEQ_START(b) ((b[0] == 0x01) && (b[1] == 0xfe))

static int vfs301_proto_process_data(int first_block, vfs301_dev_t *dev)
{
	int i;
	const unsigned char *buf = dev->recv_buf;
	int len = dev->recv_len;

	if (first_block) {
		assert(len >= VFS301_FP_FRAME_SIZE);

		/* Skip bytes until start_sequence is found */
		for (i = 0; i < VFS301_FP_FRAME_SIZE; i++, buf++, len--) {
			if (IS_VFS301_FP_SEQ_START(buf))
				break;
		}
	}

	return img_process_data(first_block, dev, buf, len);
}

void vfs301_proto_request_fingerprint(
	struct libusb_device_handle *devh, vfs301_dev_t *dev)
{
	USB_SEND(0x0220, 0xFA00);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 000000000000 */
}

int vfs301_proto_peek_event(
	struct libusb_device_handle *devh, vfs301_dev_t *dev)
{
	const char no_event[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	const char got_event[] = {0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00};

	USB_SEND(0x17, -1);
	assert(USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 7) == 0);

	if (memcmp(dev->recv_buf, no_event, sizeof(no_event)) == 0) {
		return 0;
	} else if (memcmp(dev->recv_buf, got_event, sizeof(no_event)) == 0) {
		return 1;
	} else {
		assert(!"unexpected reply to wait");
	}
}

#define VARIABLE_ORDER(a, b) \
	{ \
		int _rv = a;\
		b; \
		if (_rv == -7) \
			a; \
	}

static void vfs301_proto_process_event_cb(struct libusb_transfer *transfer)
{
	vfs301_dev_t *dev = transfer->user_data;
	struct libusb_device_handle *devh = transfer->dev_handle;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		dev->recv_progress = VFS301_FAILURE;
		goto end;
	} else if (transfer->actual_length < dev->recv_exp_amt) {
		/* TODO: process the data anyway? */
		dev->recv_progress = VFS301_ENDED;
		goto end;
	} else {
		dev->recv_len = transfer->actual_length;
		if (!vfs301_proto_process_data(dev->recv_exp_amt == VFS301_FP_RECV_LEN_1, dev)) {
			dev->recv_progress = VFS301_ENDED;
			goto end;
		}

		dev->recv_exp_amt = VFS301_FP_RECV_LEN_2;
		libusb_fill_bulk_transfer(
			transfer, devh, VFS301_RECEIVE_ENDPOINT_DATA,
			dev->recv_buf, dev->recv_exp_amt,
			vfs301_proto_process_event_cb, dev, VFS301_FP_RECV_TIMEOUT);

		if (libusb_submit_transfer(transfer) < 0) {
			printf("cb::continue fail\n");
			dev->recv_progress = VFS301_FAILURE;
			goto end;
		}
		return;
	}

end:
	libusb_free_transfer(transfer);
}

void vfs301_proto_process_event_start(
	struct libusb_device_handle *devh, vfs301_dev_t *dev)
{
	struct libusb_transfer *transfer;

	/*
	 * Notes:
	 *
	 * seen next_scan order:
	 *    o FA00
	 *    o FA00
	 *    o 2C01
	 *    o FA00
	 *    o FA00
	 *    o 2C01
	 *    o FA00
	 *    o FA00
	 *    o 2C01
	 *    o 5E01 !?
	 *    o FA00
	 *    o FA00
	 *    o 2C01
	 *    o FA00
	 *    o FA00
	 *    o 2C01
	 */
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 64);

	/* now read the fingerprint data, while there are some */
	transfer = libusb_alloc_transfer(0);
	if (!transfer) {
		dev->recv_progress = VFS301_FAILURE;
		return;
	}

	dev->recv_progress = VFS301_ONGOING;
	dev->recv_exp_amt = VFS301_FP_RECV_LEN_1;

	libusb_fill_bulk_transfer(
		transfer, devh, VFS301_RECEIVE_ENDPOINT_DATA,
		dev->recv_buf, dev->recv_exp_amt,
		vfs301_proto_process_event_cb, dev, VFS301_FP_RECV_TIMEOUT);

	if (libusb_submit_transfer(transfer) < 0) {
		libusb_free_transfer(transfer);
		dev->recv_progress = VFS301_FAILURE;
		return;
	}
}

int /* vfs301_dev_t::recv_progress */ vfs301_proto_process_event_poll(
	struct libusb_device_handle *devh, vfs301_dev_t *dev)
{
	if (dev->recv_progress != VFS301_ENDED)
		return dev->recv_progress;

	/* Finish the scan process... */

	USB_SEND(0x04, -1);
	/* the following may come in random order, data may not come at all, don't
	* try for too long... */
	VARIABLE_ORDER(
		USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2), /* 1204 */
		USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 16384)
	);

	USB_SEND(0x0220, 2);
	VARIABLE_ORDER(
		USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 5760), /* seems to always come */
		USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2) /* 0000 */
	);

	return dev->recv_progress;
}

void vfs301_proto_init(struct libusb_device_handle *devh, vfs301_dev_t *dev)
{
	USB_SEND(0x01, -1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 38);
	USB_SEND(0x0B, 0x04);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 6); /* 000000000000 */
	USB_SEND(0x0B, 0x05);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 7); /* 00000000000000 */
	USB_SEND(0x19, -1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 64);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 4); /* 6BB4D0BC */
	usb_send(devh, RAW_DATA(vfs301_06_1));
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */

	USB_SEND(0x01, -1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 38);
	USB_SEND(0x1A, -1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	usb_send(devh, RAW_DATA(vfs301_06_2));
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	USB_SEND(0x0220, 1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 256);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 32);

	USB_SEND(0x1A, -1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	usb_send(devh, RAW_DATA(vfs301_06_3));
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */

	USB_SEND(0x01, -1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 38);
	USB_SEND(0x02D0, 1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 11648); /* 56 * vfs301_init_line_t[] */
	USB_SEND(0x02D0, 2);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 53248); /* 2 * 128 * vfs301_init_line_t[] */
	USB_SEND(0x02D0, 3);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 19968); /* 96 * vfs301_init_line_t[] */
	USB_SEND(0x02D0, 4);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 5824); /* 28 * vfs301_init_line_t[] */
	USB_SEND(0x02D0, 5);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 6656); /* 32 * vfs301_init_line_t[] */
	USB_SEND(0x02D0, 6);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 6656); /* 32 * vfs301_init_line_t[] */
	USB_SEND(0x02D0, 7);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 832);
	usb_send(devh, RAW_DATA(vfs301_12));
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */

	USB_SEND(0x1A, -1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	usb_send(devh, RAW_DATA(vfs301_06_2));
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	USB_SEND(0x0220, 2);
	VARIABLE_ORDER(
		USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2), /* 0000 */
		USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 5760)
	);

	USB_SEND(0x1A, -1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	usb_send(devh, RAW_DATA(vfs301_06_1));
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */

	USB_SEND(0x1A, -1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	usb_send(devh, RAW_DATA(vfs301_06_4));
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */
	usb_send(devh, RAW_DATA(vfs301_24)); /* turns on white */
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2); /* 0000 */

	USB_SEND(0x01, -1);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 38);
	USB_SEND(0x0220, 3);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 2368);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_CTRL, 36);
	USB_RECV(VFS301_RECEIVE_ENDPOINT_DATA, 5760);
}

void vfs301_proto_deinit(struct libusb_device_handle *devh, vfs301_dev_t *dev)
{
}
