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
#include <libusb-1.0/libusb.h>

enum {
	VFS301_DEFAULT_WAIT_TIMEOUT = 300,

	VFS301_SEND_ENDPOINT = 0x01,
	VFS301_RECEIVE_ENDPOINT_CTRL = 0x81,
	VFS301_RECEIVE_ENDPOINT_DATA = 0x82
};

#define VFS301_FP_RECV_LEN_1 (84032)
#define VFS301_FP_RECV_LEN_2 (84096)

typedef struct {
	/* buffer for received data */
	unsigned char recv_buf[0x20000];
	int recv_len;

	/* buffer to hold raw scanlines */
	unsigned char *scanline_buf;
	int scanline_count;

	enum {
		VFS301_ONGOING = 0,
		VFS301_ENDED = 1,
		VFS301_FAILURE = -1
	} recv_progress;
	int recv_exp_amt;
} vfs301_dev_t;

enum {
	/* Width of the scanned data in px */
	VFS301_FP_WIDTH = 200,

	/* sizeof(fp_line_t) */
	VFS301_FP_FRAME_SIZE = 288,
	/* Width of output line */
#ifndef OUTPUT_RAW
	VFS301_FP_OUTPUT_WIDTH = VFS301_FP_WIDTH,
#else
	VFS301_FP_OUTPUT_WIDTH = VFS301_FP_FRAME_SIZE,
#endif

	VFS301_FP_SUM_LINES = 3,

#ifdef SCAN_FINISH_DETECTION
	/* TODO: The following changes (seen ~60 and ~80) In that
	 * case we'll need to calibrate this from empty data somehow... */
	VFS301_FP_SUM_MEDIAN = 60,
	VFS301_FP_SUM_EMPTY_RANGE = 5,
#endif

	/* Minimum average difference between returned lines */
	VFS301_FP_LINE_DIFF_THRESHOLD = 15,

	/* Maximum waiting time for a single fingerprint frame */
	VFS301_FP_RECV_TIMEOUT = 2000
};

/* Arrays of this structure is returned during the initialization as a response
 * to the 0x02D0 messages.
 * It seems to be always the same - what is it for? Some kind of confirmation?
 */
typedef struct {
	unsigned char sync_0x01;
	unsigned char sync_0xfe;

	unsigned char counter_lo;
	unsigned char counter_hi; /* FIXME ? */

	unsigned char flags[3];

	unsigned char sync_0x00;

	unsigned char scan[VFS301_FP_WIDTH];
} vfs301_init_line_t;

typedef struct {
	unsigned char sync_0x01;
	unsigned char sync_0xfe;

	unsigned char counter_lo;
	unsigned char counter_hi;

	unsigned char sync_0x08[2]; /* XXX: always? 0x08 0x08 */
	/* 0x08 | 0x18 - Looks like 0x08 marks good quality lines */
	unsigned char flag_1;
	unsigned char sync_0x00;

	unsigned char scan[VFS301_FP_WIDTH];

	/* A offseted, stretched, inverted copy of scan... probably could
	 * serve finger motion speed detection?
	 * Seems to be subdivided to some 10B + 53B + 1B blocks */
	unsigned char mirror[64];

	/* Some kind of sum of the scan, very low contrast */
	unsigned char sum1[2];
	unsigned char sum2[11];
	unsigned char sum3[3];
} vfs301_line_t;

void vfs301_proto_init(struct libusb_device_handle *devh, vfs301_dev_t *dev);
void vfs301_proto_deinit(struct libusb_device_handle *devh, vfs301_dev_t *dev);

void vfs301_proto_request_fingerprint(
	struct libusb_device_handle *devh, vfs301_dev_t *dev);

/** returns 0 if no event is ready, or 1 if there is one... */
int vfs301_proto_peek_event(
	struct libusb_device_handle *devh, vfs301_dev_t *dev);
void vfs301_proto_process_event_start(
	struct libusb_device_handle *devh, vfs301_dev_t *dev);
int vfs301_proto_process_event_poll(
	struct libusb_device_handle *devh, vfs301_dev_t *dev);

void vfs301_extract_image(vfs301_dev_t *vfs, unsigned char *output, int *output_height);
