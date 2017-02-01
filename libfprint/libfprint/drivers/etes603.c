/*
 * EgisTec ES603 driver for libfprint
 * Copyright (C) 2012 Patrick Marlier
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

/* EgisTec ES603 device information
 *   Sensor area: 192 x 4 pixels
 *   Sensor gray: 16 gray levels/sensor pixel
 *   Sensor resolution: 508 dpi
 *   USB Manufacturer ID: 1C7A
 *   USB Product ID: 0603
 *
 * Possible compatibility LTT-SS500/SS501
 *
 * Extra features not present in this driver (see https://code.google.com/p/etes603):
 *   Tuning of DTVRT for contact detection
 *   Contact detection via capacitance
 *   Capture mode using assembled frames (usually better quality)
 *
 */

#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <libusb.h>
#include <glib.h>

#define FP_COMPONENT "etes603"
#include <fp_internal.h>
#include "driver_ids.h"

/* libusb defines */
#define EP_IN              0x81
#define EP_OUT             0x02
/* Note that 1000 ms is usually enough but with CMD_READ_FE could be longer
 * since the sensor is waiting motion. */
#define BULK_TIMEOUT       1000

/* es603 defines */
#define FRAME_WIDTH        192  /* pixels per row */
#define FRAME_HEIGHT       4    /* number of rows */
#define FRAME_SIZE         384  /* size in bytes (4 bits per pixels) */
#define FE_WIDTH           256  /* pixels per row for Fly-Estimation */
#define FE_HEIGHT          500  /* number of rows for Fly-Estimation */
#define FE_SIZE            64000 /* size in bytes (4 bits per pixels) */

#define GAIN_SMALL_INIT    0x23 /* Initial small gain */
#define VRT_MAX	           0x3F /* Maximum value for VRT */
#define VRB_MAX            0x3A /* Maximum value for VRB */
#define DTVRT_MAX          0x3A /* Maximum value for DTVRT */
#define DCOFFSET_MIN       0x00 /* Minimum value for DCoffset */
#define DCOFFSET_MAX       0x35 /* Maximum value for DCoffset */

/* es603 commands */
#define CMD_READ_REG       0x01
#define CMD_WRITE_REG      0x02
#define CMD_READ_FRAME     0x03 /* Read the sensor area */
#define CMD_READ_FE        0x06 /* Read a fingerprint using Fly-Estimation */
#define CMD_20             0x20 /* ? */
#define CMD_25             0x25 /* ? */
#define CMD_60             0x60 /* ? */

#define CMD_OK             0x01 /* Command successfully executed */

/* es603 registers */
#define REG_MAX            0x18 /* Maximum number of registers in one message */
#define REG_MODE_CONTROL   0x02 /* Mode control */
#define REG_03             0x03 /* Contact register? */
#define REG_04             0x04 /* ? */
#define REG_10             0x10 /* MVS FRMBUF control */
#define REG_1A             0x1A /* ? */
/* BEGIN init sensor */
#define REG_20             0x20 /* (def: 0x00) */
#define REG_21             0x21 /* Small gain (def: 0x23) */
#define REG_22             0x22 /* Normal gain (def: 0x21) */
#define REG_23             0x23 /* Large gain (def: 0x20) */
#define REG_24             0x24 /* (def: 0x14) */
#define REG_25             0x25 /* (def: 0x6A) */
#define REG_26             0x26 /* VRB again? (def: 0x00) */
#define REG_27             0x27 /* VRT again? (def: 0x00) */
#define REG_28             0x28 /* (def: 0x00) */
#define REG_29             0x29 /* (def: 0xC0) */
#define REG_2A             0x2A /* (def: 0x50) */
#define REG_2B             0x2B /* (def: 0x50) */
#define REG_2C             0x2C /* (def: 0x4D) */
#define REG_2D             0x2D /* (def: 0x03) */
#define REG_2E             0x2E /* (def: 0x06) */
#define REG_2F             0x2F /* (def: 0x06) */
#define REG_30             0x30 /* (def: 0x10) */
#define REG_31             0x31 /* (def: 0x02) */
#define REG_32             0x32 /* (def: 0x14) */
#define REG_33             0x33 /* (def: 0x34) */
#define REG_34             0x34 /* (def: 0x01) */
#define REG_35             0x35 /* (def: 0x08) */
#define REG_36             0x36 /* (def: 0x03) */
#define REG_37             0x37 /* (def: 0x21) */
/* END init sensor */

#define REG_ENC1           0x41 /* Encryption 1 */
#define REG_ENC2           0x42
#define REG_ENC3           0x43
#define REG_ENC4           0x44
#define REG_ENC5           0x45
#define REG_ENC6           0x46
#define REG_ENC7           0x47
#define REG_ENC8           0x48 /* Encryption 8 */

#define REG_50             0x50 /* ? For contact detection */
#define REG_51             0x51 /* ? */
#define REG_59             0x59 /* ? */
#define REG_5A             0x5A /* ? */
#define REG_5B             0x5B /* ? */

#define REG_INFO0          0x70 /* Sensor model byte0 */
#define REG_INFO1          0x71 /* Sensor model byte1 */
#define REG_INFO2          0x72 /* Sensor model byte2 */
#define REG_INFO3          0x73 /* Sensor model byte3 */

#define REG_GAIN           0xE0
#define REG_VRT            0xE1
#define REG_VRB            0xE2
#define REG_DTVRT          0xE3 /* used for contact detection */
#define REG_VCO_CONTROL    0xE5 /* 0x13 (IDLE?), 0x14 (REALTIME) */
#define REG_DCOFFSET       0xE6

#define REG_F0             0xF0 /* ? init:0x00 close:0x01 */
#define REG_F2             0xF2 /* ? init:0x00 close:0x4E */

#define REG_MODE_SLEEP     0x30 /* Sleep mode */
#define REG_MODE_CONTACT   0x31 /* Contact mode */
#define REG_MODE_SENSOR    0x33 /* Sensor mode */
#define REG_MODE_FP        0x34 /* FingerPrint mode (Fly-Estimation®) */

#define REG_VCO_IDLE       0x13
#define REG_VCO_RT         0x14 /* Realtime */

/* The size of the message header is 5 plus 1 for the command. */
#define MSG_HDR_SIZE       6

/* This structure must be packed because it is a the raw message sent. */
struct egis_msg {
	uint8_t magic[5]; /* out: 'EGIS' 0x09 / in: 'SIGE' 0x0A */
	uint8_t cmd;
	union {
		struct {
			uint8_t nb;
			uint8_t regs[REG_MAX];
		} egis_readreg;
		struct {
			uint8_t regs[REG_MAX];
		} sige_readreg;
		struct {
			uint8_t nb;
			struct {
				uint8_t reg;
				uint8_t val;
			} regs[REG_MAX];
		} egis_writereg;
		struct {
			uint8_t length_factor;
			uint8_t length;
			uint8_t use_gvv;
			uint8_t gain;
			uint8_t vrt;
			uint8_t vrb;
		} egis_readf;
		struct {
			uint8_t len[2];
			uint8_t val[3];
		} egis_readfp;
		struct {
			uint8_t val[5];
		} sige_misc;
		uint8_t padding[0x40-6]; /* Ensure size of 0x40 */
	};
} __attribute__((packed));


/* Structure to keep information between asynchronous functions. */
struct etes603_dev {
	uint8_t regs[256];
	struct egis_msg *req;
	size_t req_len;
	struct egis_msg *ans;
	size_t ans_len;

	uint8_t *fp;
	uint16_t fp_height;

	uint8_t tunedc_min;
	uint8_t tunedc_max;

	/* Device parameters */
	uint8_t gain;
	uint8_t dcoffset;
	uint8_t vrt;
	uint8_t vrb;

	unsigned int is_active;
};

static void m_start_fingerdetect(struct fp_img_dev *idev);
/*
 * Prepare the header of the message to be sent to the device.
 */
static void msg_header_prepare(struct egis_msg *msg)
{
	msg->magic[0] = 'E';
	msg->magic[1] = 'G';
	msg->magic[2] = 'I';
	msg->magic[3] = 'S';
	msg->magic[4] = 0x09;
}

/*
 * Check that the header of the received message is correct.
 */
static int msg_header_check(struct egis_msg *msg)
{
	if (msg->magic[0] == 'S' && msg->magic[1] == 'I'
	    && msg->magic[2] == 'G' && msg->magic[3] == 'E'
	    && msg->magic[4] == 0x0A)
		return 0;
	return -1;
}

/*
 * Prepare message to ask for a frame.
 */
static void msg_get_frame(struct etes603_dev *dev,
	uint8_t use_gvv, uint8_t gain, uint8_t vrt, uint8_t vrb)
{
	struct egis_msg *msg = dev->req;
	msg_header_prepare(msg);
	msg->cmd = CMD_READ_FRAME;
	msg->egis_readf.length_factor = 0x01;
	/* length should be 0xC0 */
	msg->egis_readf.length = FRAME_WIDTH;
	msg->egis_readf.use_gvv = use_gvv;
	/* if use_gvv is set, gain/vrt/vrb are used */
	msg->egis_readf.gain = gain;
	msg->egis_readf.vrt = vrt;
	msg->egis_readf.vrb = vrb;

	dev->req_len = MSG_HDR_SIZE + 6;
	dev->ans_len = FRAME_SIZE;
}

/*
 * Prepare message to ask for a fingerprint frame.
 */
static void msg_get_fp(struct etes603_dev *dev, uint8_t len0, uint8_t len1,
	uint8_t v2, uint8_t v3, uint8_t v4)
{
	struct egis_msg *msg = dev->req;
	msg_header_prepare(msg);
	msg->cmd = CMD_READ_FE;
	/* Unknown values and always same on captured frames.
	 * 1st 2nd bytes is unsigned short for height, but only on value range
	 * 0x01 0xF4 (500), 0x02 0x00 (512), 0x02 0xF4 (756) are ok
	 */
	msg->egis_readfp.len[0] = len0;
	msg->egis_readfp.len[1] = len1;
	/* 3rd byte : ?? but changes frame size
	 * 4th byte : 0x00 -> can change width
	 * 5th byte : motion sensibility?
	 */
	msg->egis_readfp.val[0] = v2;
	msg->egis_readfp.val[1] = v3;
	msg->egis_readfp.val[2] = v4;

	dev->req_len = MSG_HDR_SIZE + 5;
	dev->ans_len = FE_SIZE;
}

/*
 * Prepare message to read registers from the sensor.
 * Variadic argument pattern: int reg, ...
 */
static void msg_get_regs(struct etes603_dev *dev, int n_args, ... )
{
	struct egis_msg *msg = dev->req;
	va_list ap;
	int i;

	assert(n_args > 0 && n_args <= REG_MAX);

	msg_header_prepare(msg);
	msg->cmd = CMD_READ_REG;
	msg->egis_readreg.nb = n_args;
	va_start(ap, n_args);
	for (i = 0; i < n_args; i++) {
		msg->egis_readreg.regs[i] = va_arg(ap, int);
	}
	va_end(ap);

	dev->req_len = MSG_HDR_SIZE + 1 + n_args;
	dev->ans_len = MSG_HDR_SIZE + 1 + n_args;
}

/*
 * Parse the result of read register command.
 */
static int msg_parse_regs(struct etes603_dev *dev)
{
	size_t i, n_args;
	struct egis_msg *msg_req = dev->req;
	struct egis_msg *msg_ans = dev->ans;
	n_args = dev->ans_len - MSG_HDR_SIZE;

	if (msg_header_check(msg_ans)) {
		return -1;
	}
	if (msg_ans->cmd != CMD_OK) {
		return -2;
	}

	for (i = 0; i < n_args; i++) {
		int reg = msg_req->egis_readreg.regs[i];
		dev->regs[reg] = msg_ans->sige_readreg.regs[i];
	}
	return 0;
}

/*
 * Prepare message to write sensor's registers.
 * Variadic arguments are: int reg, int val, ...
 */
static void msg_set_regs(struct etes603_dev *dev, int n_args, ...)
{
	struct egis_msg *msg = dev->req;
	va_list ap;
	int i;

	assert(n_args != 0 && n_args % 2 == 0 && n_args <= REG_MAX * 2);

	msg_header_prepare(msg);
	msg->cmd = CMD_WRITE_REG;
	msg->egis_writereg.nb = n_args / 2;

	va_start(ap, n_args);
	for (i = 0; i < n_args / 2; i++) {
		msg->egis_writereg.regs[i].reg = va_arg(ap, int);
		msg->egis_writereg.regs[i].val = va_arg(ap, int);
	}
	va_end(ap);

	dev->req_len = MSG_HDR_SIZE + 1 + n_args;
	dev->ans_len = MSG_HDR_SIZE + 1;
}

static int msg_check_ok(struct etes603_dev *dev)
{
	struct egis_msg *msg = dev->ans;
	if (msg_header_check(msg)) {
		goto err;
	}
	if (msg->cmd != CMD_OK) {
		goto err;
	}
	return 0;
err:
	return -1;
}

/*
 * Check the model of the sensor.
 */
static int check_info(struct etes603_dev *dev)
{
	if (dev->regs[0x70] == 0x4A && dev->regs[0x71] == 0x44
	    && dev->regs[0x72] == 0x49 && dev->regs[0x73] == 0x31)
		return 0;
	fp_err("unknown device parameters (REG_70:%02X REG_71:%02X "
		"REG_FIRMWARE:%02X REG_VERSION:%02X)",
		dev->regs[0x70], dev->regs[0x71], dev->regs[0x72],
		dev->regs[0x73]);
	return -1;
}

static void msg_get_cmd20(struct etes603_dev *dev)
{
	struct egis_msg *msg = dev->req;
	msg_header_prepare(msg);
	msg->cmd = CMD_20;
	dev->req_len = MSG_HDR_SIZE;
}

static int msg_check_cmd20(struct etes603_dev *dev)
{
	struct egis_msg *msg = dev->ans;
	if (msg_header_check(msg)) {
		fp_err("msg_header_check failed");
		return -1;
	}
	/* status or flashtype/flashinfo or ? */
	if (msg->cmd != 0x05
	    || msg->sige_misc.val[0] != 0x00
	    || msg->sige_misc.val[1] != 0x00) {
		fp_warn("unexpected answer CMD_20 from device(%02X %02X %02X)",
			msg->cmd, msg->sige_misc.val[0], msg->sige_misc.val[1]);
	}

	return 0;
}

static void msg_get_cmd25(struct etes603_dev *dev)
{
	struct egis_msg *msg = dev->req;
	msg_header_prepare(msg);
	msg->cmd = CMD_25;
	dev->req_len = MSG_HDR_SIZE;
}

static int msg_check_cmd25(struct etes603_dev *dev)
{
	struct egis_msg *msg = dev->ans;
	if (msg_header_check(msg)) {
		fp_err("msg_header_check failed");
		goto err;
	}
	if (msg->cmd != CMD_OK) {
		fp_err("CMD_OK failed");
		goto err;
	}
	/* flashtype or status or ? */
	if (msg->sige_misc.val[0] != 0x00) {
		fp_warn("unexpected answer for CMD_25 (%02X)",
			msg->sige_misc.val[0]);
	}
	return 0;
err:
	return -1;
}

static void msg_set_mode_control(struct etes603_dev *dev, uint8_t mode)
{
	msg_set_regs(dev, 2, REG_MODE_CONTROL, mode);
}


/* Processing functions */

/*
 * Return the brightness of a 4bpp frame
 */
static unsigned int process_get_brightness(uint8_t *f, size_t s)
{
	unsigned int i, sum = 0;
	for (i = 0; i < s; i++) {
		sum += f[i] >> 4;
		sum += f[i] & 0x0F;
	}
	return sum;
}

/*
 * Return the histogram of a 4bpp frame
 */
static void process_hist(uint8_t *f, size_t s, float stat[5])
{
	float hist[16];
	float black_mean, white_mean;
	int i;
	/* Clean histogram */
	for (i = 0; i < 16; i++)
		hist[i] = 0.0;
	for (i = 0; i < s; i++) {
		hist[f[i] >> 4]++;
		hist[f[i] & 0x0F]++;
	}
	/* histogram average */
	for (i = 0; i < 16; i++) {
		hist[i] = hist[i] / (s * 2);
	}
	/* Average black/white pixels (full black and full white pixels
	 * are excluded). */
	black_mean = white_mean = 0.0;
	for (i = 1; i < 8; i++)
		black_mean += hist[i];
	for (i = 8; i < 15; i++)
		white_mean += hist[i];
	stat[0] = hist[0];
	stat[1] = black_mean;
	stat[2] = black_mean+white_mean;
	stat[3] = white_mean;
	stat[4] = hist[15];
	fp_dbg("fullb=%6f black=%6f grey=%6f white=%6f fullw=%6f",
		hist[0], black_mean, black_mean+white_mean, white_mean,
		hist[15]);
}

/*
 * Return true if the frame is almost empty.
 */
static int process_frame_empty(uint8_t *frame, size_t size)
{
	unsigned int sum = process_get_brightness(frame, size);
	/* Allow an average of 'threshold' luminosity per pixel */
	if (sum < size)
		return 1;
	return 0;
}

/* Transform 4 bits image to 8 bits image */
static void process_4to8_bpp(uint8_t *input, unsigned int input_size,
	uint8_t *output)
{
	unsigned int i, j = 0;
	for (i = 0; i < input_size; i++, j += 2) {
		/* 16 gray levels transform to 256 levels using << 4 */
		output[j] = input[i] & 0xF0;
		output[j+1] = input[i] << 4;
	}
}

/*
 * Remove duplicated lines at the end of a fingerprint.
 */
static void process_remove_fp_end(struct etes603_dev *dev)
{
	unsigned int i;
	/* 2 last lines with Fly-Estimation are the empty pattern. */
	uint8_t *pattern = dev->fp + (dev->fp_height - 2) * FE_WIDTH / 2;
	for (i = 2; i < dev->fp_height; i+= 2) {
		if (memcmp(pattern, pattern - (i * FE_WIDTH / 2), FE_WIDTH))
			break;
	}
	dev->fp_height -= i;
	fp_dbg("Removing %d empty lines from image", i - 2);
}

static void reset_param(struct etes603_dev *dev)
{
	dev->dcoffset = 0;
	dev->vrt = 0;
	dev->vrb = 0;
	dev->gain = 0;
}


/* Asynchronous stuff */

enum {
	INIT_CHECK_INFO_REQ,
	INIT_CHECK_INFO_ANS,
	INIT_CMD20_REQ,
	INIT_CMD20_ANS,
	INIT_CMD25_REQ,
	INIT_CMD25_ANS,
	INIT_SENSOR_REQ,
	INIT_SENSOR_ANS,
	INIT_ENC_REQ,
	INIT_ENC_ANS,
	INIT_REGS_REQ,
	INIT_REGS_ANS,
	INIT_NUM_STATES
};

enum {
	TUNEDC_INIT,
	TUNEDC_SET_DCOFFSET_REQ,
	TUNEDC_SET_DCOFFSET_ANS,
	TUNEDC_GET_FRAME_REQ,
	TUNEDC_GET_FRAME_ANS,
	TUNEDC_FINAL_SET_REG2122_REQ,
	TUNEDC_FINAL_SET_REG2122_ANS,
	TUNEDC_FINAL_SET_GAIN_REQ,
	TUNEDC_FINAL_SET_GAIN_ANS,
	TUNEDC_FINAL_SET_DCOFFSET_REQ,
	TUNEDC_FINAL_SET_DCOFFSET_ANS,
	TUNEDC_NUM_STATES
};

enum {
	TUNEVRB_INIT,
	TUNEVRB_GET_GAIN_REQ,
	TUNEVRB_GET_GAIN_ANS,
	TUNEVRB_GET_DCOFFSET_REQ,
	TUNEVRB_GET_DCOFFSET_ANS,
	TUNEVRB_SET_DCOFFSET_REQ,
	TUNEVRB_SET_DCOFFSET_ANS,
	TUNEVRB_FRAME_REQ,
	TUNEVRB_FRAME_ANS,
	TUNEVRB_FINAL_SET_DCOFFSET_REQ,
	TUNEVRB_FINAL_SET_DCOFFSET_ANS,
	TUNEVRB_FINAL_SET_REG2627_REQ,
	TUNEVRB_FINAL_SET_REG2627_ANS,
	TUNEVRB_FINAL_SET_GAINVRTVRB_REQ,
	TUNEVRB_FINAL_SET_GAINVRTVRB_ANS,
	TUNEVRB_FINAL_SET_MODE_SLEEP_REQ,
	TUNEVRB_FINAL_SET_MODE_SLEEP_ANS,
	TUNEVRB_NUM_STATES
};

enum {
	FGR_FPA_INIT_SET_MODE_SLEEP_REQ,
	FGR_FPA_INIT_SET_MODE_SLEEP_ANS,
	FGR_FPA_INIT_SET_DCOFFSET_REQ,
	FGR_FPA_INIT_SET_DCOFFSET_ANS,
	FGR_FPA_INIT_SET_GAINVRTVRB_REQ,
	FGR_FPA_INIT_SET_GAINVRTVRB_ANS,
	FGR_FPA_INIT_SET_VCO_CONTROL_RT_REQ,
	FGR_FPA_INIT_SET_VCO_CONTROL_RT_ANS,
	FGR_FPA_INIT_SET_REG04_REQ,
	FGR_FPA_INIT_SET_REG04_ANS,
	FGR_FPA_INIT_SET_MODE_SENSOR_REQ,
	FGR_FPA_INIT_SET_MODE_SENSOR_ANS,
	FGR_FPA_GET_FRAME_REQ,
	FGR_FPA_GET_FRAME_ANS,
	FGR_NUM_STATES
};

enum {
	CAP_FP_INIT_SET_REG10_REQ,
	CAP_FP_INIT_SET_REG10_ANS,
	CAP_FP_INIT_SET_MODE_FP_REQ,
	CAP_FP_INIT_SET_MODE_FP_ANS,
	CAP_FP_GET_FP_REQ,
	CAP_FP_GET_FP_ANS,
	CAP_NUM_STATES
};

enum {
	EXIT_SET_REGS_REQ,
	EXIT_SET_REGS_ANS,
	EXIT_NUM_STATES
};

static int async_tx(struct fp_img_dev *idev, unsigned int ep, void *cb,
	void *cb_arg)
{
	struct etes603_dev *dev = idev->priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *buffer;
	int length;

	if (!transfer)
		return -ENOMEM;

	if (ep == EP_OUT) {
		buffer = (unsigned char *)dev->req;
		length = dev->req_len;
	} else if (ep == EP_IN) {
		buffer = (unsigned char *)dev->ans;
		length = dev->ans_len;
	} else {
		return -EIO;
	}
	libusb_fill_bulk_transfer(transfer, idev->udev, ep, buffer, length,
				  cb, cb_arg, BULK_TIMEOUT);

	if (libusb_submit_transfer(transfer)) {
		libusb_free_transfer(transfer);
		return -EIO;
	}
	return 0;
}


static void async_tx_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;
	struct fp_img_dev *idev = ssm->priv;
	struct etes603_dev *dev = idev->priv;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fp_warn("transfer is not completed (status=%d)",
			transfer->status);
		fpi_ssm_mark_aborted(ssm, -EIO);
		libusb_free_transfer(transfer);
	} else {
		unsigned char endpoint = transfer->endpoint;
		int actual_length = transfer->actual_length;
		int length = transfer->length;
		/* Freeing now transfer since fpi_ssm_* functions are not
		 * returning directly. */
		libusb_free_transfer(transfer);
		if (endpoint == EP_OUT) {
			if (length != actual_length)
				fp_warn("length %d != actual_length %d",
					length, actual_length);
			/* Chained with the answer */
			if (async_tx(idev, EP_IN, async_tx_cb, ssm))
				fpi_ssm_mark_aborted(ssm, -EIO);
		} else if (endpoint == EP_IN) {
			dev->ans_len = actual_length;
			fpi_ssm_next_state(ssm);
		}
	}
}

static void m_exit_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct etes603_dev *dev = idev->priv;

	switch (ssm->cur_state) {
	case EXIT_SET_REGS_REQ:
		msg_set_regs(dev, 4, REG_VCO_CONTROL, REG_VCO_IDLE,
			     REG_MODE_CONTROL, REG_MODE_SLEEP);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case EXIT_SET_REGS_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_mark_completed(ssm);
		break;
	default:
		fp_err("Unknown state %d", ssm->cur_state);
		goto err;
		break;
	}

	return;
err:
	fpi_ssm_mark_aborted(ssm, -EIO);
}

static void m_exit_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;

	if (ssm->error) {
		fp_err("Error switching the device to idle state");
	} else {
		fp_dbg("The device is now in idle state");
	}
	fpi_imgdev_deactivate_complete(idev);
	fpi_ssm_free(ssm);
}

static void m_exit_start(struct fp_img_dev *idev)
{
	struct fpi_ssm *ssm = fpi_ssm_new(idev->dev, m_exit_state,
					  EXIT_NUM_STATES);
	fp_dbg("Switching device to idle mode");
	ssm->priv = idev;
	fpi_ssm_start(ssm, m_exit_complete);
}

static void m_capture_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct etes603_dev *dev = idev->priv;

	if (dev->is_active == FALSE) {
		fpi_ssm_mark_completed(ssm);
		return;
	}

	switch (ssm->cur_state) {
	case CAP_FP_INIT_SET_REG10_REQ:
		/* Reset fingerprint */
		fp_dbg("Capturing a fingerprint...");
		memset(dev->fp, 0, FE_SIZE * 2);
		dev->fp_height = 0;
		msg_set_regs(dev, 2, REG_10, 0x92);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case CAP_FP_INIT_SET_REG10_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case CAP_FP_INIT_SET_MODE_FP_REQ:
		msg_set_mode_control(dev, REG_MODE_FP);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case CAP_FP_INIT_SET_MODE_FP_ANS:
		if (msg_check_ok(dev))
			goto err;
		fp_dbg("Capturing a 1st frame...");
		fpi_ssm_next_state(ssm);
		break;
	case CAP_FP_GET_FP_REQ:
		msg_get_fp(dev, 0x01, 0xF4, 0x02, 0x01, 0x64);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case CAP_FP_GET_FP_ANS:
		memcpy(dev->fp + dev->fp_height * FE_WIDTH / 2, dev->ans,
			FE_SIZE);
		dev->fp_height += FE_HEIGHT;
		if (dev->fp_height <= FE_HEIGHT) {
			/* 2 lines are at least removed each time */
			dev->fp_height -= 2;
			fp_dbg("Capturing a 2nd frame...");
			fpi_ssm_jump_to_state(ssm, CAP_FP_GET_FP_REQ);
		} else {
			struct fp_img *img;
			unsigned int img_size;
			/* Remove empty parts 2 times for the 2 frames */
			process_remove_fp_end(dev);
			process_remove_fp_end(dev);
			img_size = dev->fp_height * FE_WIDTH;
			img = fpi_img_new(img_size);
			/* Images received are white on black, so invert it. */
			/* TODO detect sweep direction */
			img->flags = FP_IMG_COLORS_INVERTED | FP_IMG_V_FLIPPED;
			img->height = dev->fp_height;
			process_4to8_bpp(dev->fp, img_size / 2, img->data);
			fp_dbg("Sending the raw fingerprint image (%dx%d)",
				img->width, img->height);
			fpi_imgdev_image_captured(idev, img);
			fpi_imgdev_report_finger_status(idev, FALSE);
			fpi_ssm_mark_completed(ssm);
		}
		break;
	default:
		fp_err("Unknown state %d", ssm->cur_state);
		goto err;
		break;
	}

	return;
err:
	fpi_ssm_mark_aborted(ssm, -EIO);
}

static void m_capture_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct etes603_dev *dev = idev->priv;

	if (ssm->error) {
		if (idev->action_state != IMG_ACQUIRE_STATE_DEACTIVATING) {
			fp_err("Error while capturing fingerprint "
				"(ssm->error=%d)", ssm->error);
			fpi_imgdev_session_error(idev, ssm->error);
		}
	}
	fpi_ssm_free(ssm);

	if (dev->is_active == TRUE) {
		fp_dbg("Device is still active, restarting finger detection");
		m_start_fingerdetect(idev);
	} else {
		fp_dbg("And it's over.");
	}
}

static void m_finger_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct etes603_dev *dev = idev->priv;

	if (dev->is_active == FALSE) {
		fpi_ssm_mark_completed(ssm);
		return;
	}

	switch (ssm->cur_state) {
	case FGR_FPA_INIT_SET_MODE_SLEEP_REQ:
		msg_set_mode_control(dev, REG_MODE_SLEEP);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case FGR_FPA_INIT_SET_MODE_SLEEP_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case FGR_FPA_INIT_SET_DCOFFSET_REQ:
		msg_set_regs(dev, 2, REG_DCOFFSET, dev->dcoffset);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case FGR_FPA_INIT_SET_DCOFFSET_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case FGR_FPA_INIT_SET_GAINVRTVRB_REQ:
		msg_set_regs(dev, 6, REG_GAIN, dev->gain, REG_VRT, dev->vrt,
			REG_VRB, dev->vrb);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case FGR_FPA_INIT_SET_GAINVRTVRB_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case FGR_FPA_INIT_SET_VCO_CONTROL_RT_REQ:
		msg_set_regs(dev, 2, REG_VCO_CONTROL, REG_VCO_RT);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case FGR_FPA_INIT_SET_VCO_CONTROL_RT_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case FGR_FPA_INIT_SET_REG04_REQ:
		msg_set_regs(dev, 2, REG_04, 0x00);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case FGR_FPA_INIT_SET_REG04_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case FGR_FPA_INIT_SET_MODE_SENSOR_REQ:
		msg_set_mode_control(dev, REG_MODE_SENSOR);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case FGR_FPA_INIT_SET_MODE_SENSOR_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case FGR_FPA_GET_FRAME_REQ:
		msg_get_frame(dev, 0x00, 0x00, 0x00, 0x00);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case FGR_FPA_GET_FRAME_ANS:
		if (process_frame_empty((uint8_t *)dev->ans, FRAME_SIZE)) {
			fpi_ssm_jump_to_state(ssm, FGR_FPA_GET_FRAME_REQ);
		} else {
			fpi_imgdev_report_finger_status(idev, TRUE);
			fpi_ssm_mark_completed(ssm);
		}
		break;
	default:
		fp_err("Unknown state %d", ssm->cur_state);
		goto err;
		break;
	}

	return;
err:
	fpi_ssm_mark_aborted(ssm, -EIO);
}

static void m_finger_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct etes603_dev *dev = idev->priv;

	if (!ssm->error) {
		struct fpi_ssm *ssm_cap;
		ssm_cap = fpi_ssm_new(idev->dev, m_capture_state,
				CAP_NUM_STATES);
		ssm_cap->priv = idev;
		fpi_ssm_start(ssm_cap, m_capture_complete);
	} else {
		if (idev->action_state != IMG_ACQUIRE_STATE_DEACTIVATING) {
			fp_err("Error while capturing fingerprint "
				"(ssm->error=%d)", ssm->error);
			fpi_imgdev_session_error(idev, -4);
		}
		dev->is_active = FALSE;
	}

	fpi_ssm_free(ssm);
}

static void m_start_fingerdetect(struct fp_img_dev *idev)
{
	struct fpi_ssm *ssmf;
	ssmf = fpi_ssm_new(idev->dev, m_finger_state, FGR_NUM_STATES);
	ssmf->priv = idev;
	fpi_ssm_start(ssmf, m_finger_complete);
}

/*
 * Tune value of VRT and VRB for contrast and brightness.
 */
static void m_tunevrb_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct etes603_dev *dev = idev->priv;
	float hist[5];

	if (dev->is_active == FALSE) {
		fpi_ssm_mark_completed(ssm);
		return;
	}

	switch (ssm->cur_state) {
	case TUNEVRB_INIT:
		fp_dbg("Tuning of VRT/VRB");
		assert(dev->dcoffset);
		/* VRT(reg E1)=0x0A and VRB(reg E2)=0x10 are starting values */
		dev->vrt = 0x0A;
		dev->vrb = 0x10;
		fpi_ssm_next_state(ssm);
		break;
	case TUNEVRB_GET_GAIN_REQ:
		msg_get_regs(dev, 1, REG_GAIN);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEVRB_GET_GAIN_ANS:
		if (msg_parse_regs(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case TUNEVRB_GET_DCOFFSET_REQ:
		msg_get_regs(dev, 1, REG_DCOFFSET);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEVRB_GET_DCOFFSET_ANS:
		if (msg_parse_regs(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case TUNEVRB_SET_DCOFFSET_REQ:
		/* Reduce DCoffset by 1 to allow tuning */
		msg_set_regs(dev, 2, REG_DCOFFSET, dev->dcoffset - 1);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEVRB_SET_DCOFFSET_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case TUNEVRB_FRAME_REQ:
		fp_dbg("Testing VRT=0x%02X VRB=0x%02X", dev->vrt, dev->vrb);
		msg_get_frame(dev, 0x01, dev->gain, dev->vrt, dev->vrb);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEVRB_FRAME_ANS:
		process_hist((uint8_t *)dev->ans, FRAME_SIZE, hist);
		/* Note that this tuning could probably be improved */
		if (hist[0] + hist[1] > 0.95) {
			if (dev->vrt <= 0 || dev->vrb <= 0) {
				fp_dbg("Image is too dark, reducing DCOffset");
				dev->dcoffset--;
				fpi_ssm_jump_to_state(ssm, TUNEVRB_INIT);
			} else {
				dev->vrt--;
				dev->vrb--;
				fpi_ssm_jump_to_state(ssm, TUNEVRB_FRAME_REQ);
			}
			break;
		}
		if (hist[4] > 0.95) {
			fp_dbg("Image is too bright, increasing DCOffset");
			dev->dcoffset++;
			fpi_ssm_jump_to_state(ssm, TUNEVRB_INIT);
			break;
		}
		if (hist[4] + hist[3] > 0.4) {
			if (dev->vrt >= 2 * dev->vrb - 0x0a) {
				dev->vrt++; dev->vrb++;
			} else {
				dev->vrt++;
			}
			/* Check maximum for vrt/vrb */
			/* TODO if maximum is reached, leave with an error? */
			if (dev->vrt > VRT_MAX)
				dev->vrt = VRT_MAX;
			if (dev->vrb > VRB_MAX)
				dev->vrb = VRB_MAX;
			fpi_ssm_jump_to_state(ssm, TUNEVRB_FRAME_REQ);
			break;
		}
		fpi_ssm_next_state(ssm);
		break;
	case TUNEVRB_FINAL_SET_DCOFFSET_REQ:
		fp_dbg("-> VRT=0x%02X VRB=0x%02X", dev->vrt, dev->vrb);
		/* Reset the DCOffset */
		msg_set_regs(dev, 2, REG_DCOFFSET, dev->dcoffset);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEVRB_FINAL_SET_DCOFFSET_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case TUNEVRB_FINAL_SET_REG2627_REQ:
		/* In traces, REG_26/REG_27 are set. purpose? values? */
		msg_set_regs(dev, 4, REG_26, 0x11, REG_27, 0x00);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEVRB_FINAL_SET_REG2627_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case TUNEVRB_FINAL_SET_GAINVRTVRB_REQ:
		/* Set Gain/VRT/VRB values found */
		msg_set_regs(dev, 6, REG_GAIN, dev->gain, REG_VRT, dev->vrt,
			     REG_VRB, dev->vrb);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEVRB_FINAL_SET_GAINVRTVRB_ANS:
		if (msg_check_ok(dev))
			goto err;
		/* In traces, Gain/VRT/VRB are read again. */
		fpi_ssm_next_state(ssm);
		break;
	case TUNEVRB_FINAL_SET_MODE_SLEEP_REQ:
		msg_set_mode_control(dev, REG_MODE_SLEEP);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEVRB_FINAL_SET_MODE_SLEEP_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_mark_completed(ssm);
		break;
	default:
		fp_err("Unknown state %d", ssm->cur_state);
		goto err;
		break;
	}

	return;
err:
	fpi_ssm_mark_aborted(ssm, -EIO);
}

static void m_tunevrb_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;

	fpi_imgdev_activate_complete(idev, ssm->error != 0);
	if (!ssm->error) {
		fp_dbg("Tuning is done. Starting finger detection.");
		m_start_fingerdetect(idev);
	} else {
		struct etes603_dev *dev = idev->priv;
		fp_err("Error while tuning VRT");
		dev->is_active = FALSE;
		reset_param(dev);
		fpi_imgdev_session_error(idev, -3);
	}
	fpi_ssm_free(ssm);
}

/*
 * This function tunes the DCoffset value and adjusts the gain value if
 * required.
 */
static void m_tunedc_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct etes603_dev *dev = idev->priv;

	if (dev->is_active == FALSE) {
		fpi_ssm_mark_completed(ssm);
		return;
	}

	/* TODO To get better results, tuning could be done 3 times as in
	 * captured traffic to make sure that the value is correct. */
	/* The default gain should work but it may reach a DCOffset limit so in
	 * this case we decrease the gain. */
	switch (ssm->cur_state) {
	case TUNEDC_INIT:
		/* reg_e0 = 0x23 is sensor normal/small gain */
		dev->gain = GAIN_SMALL_INIT;
		dev->tunedc_min = DCOFFSET_MIN;
		dev->tunedc_max = DCOFFSET_MAX;
		fp_dbg("Tuning DCoffset");
		fpi_ssm_next_state(ssm);
		break;
	case TUNEDC_SET_DCOFFSET_REQ:
		/* Dichotomic search to find at which value the frame becomes
		 * almost black. */
		dev->dcoffset = (dev->tunedc_max + dev->tunedc_min) / 2;
		fp_dbg("Testing DCoffset=0x%02X Gain=0x%02X", dev->dcoffset,
			dev->gain);
		msg_set_regs(dev, 2, REG_DCOFFSET, dev->dcoffset);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEDC_SET_DCOFFSET_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case TUNEDC_GET_FRAME_REQ:
		/* vrt:0x15 vrb:0x10 are constant in all tuning frames. */
		msg_get_frame(dev, 0x01, dev->gain, 0x15, 0x10);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEDC_GET_FRAME_ANS:
		if (process_frame_empty((uint8_t *)dev->ans, FRAME_WIDTH))
			dev->tunedc_max = dev->dcoffset;
		else
			dev->tunedc_min = dev->dcoffset;
		if (dev->tunedc_min + 1 < dev->tunedc_max) {
			fpi_ssm_jump_to_state(ssm, TUNEDC_SET_DCOFFSET_REQ);
		} else if (dev->tunedc_max < DCOFFSET_MAX) {
			dev->dcoffset = dev->tunedc_max + 1;
			fpi_ssm_next_state(ssm);
		} else {
			dev->gain--;
			fpi_ssm_jump_to_state(ssm, TUNEDC_SET_DCOFFSET_REQ);
		}
		break;
	case TUNEDC_FINAL_SET_REG2122_REQ:
		fp_dbg("-> DCoffset=0x%02X Gain=0x%02X", dev->dcoffset,
			dev->gain);
		/* ??? how reg21 / reg22 are calculated */
		msg_set_regs(dev, 4, REG_21, 0x23, REG_22, 0x21);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEDC_FINAL_SET_REG2122_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case TUNEDC_FINAL_SET_GAIN_REQ:
		msg_set_regs(dev, 2, REG_GAIN, dev->gain);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEDC_FINAL_SET_GAIN_ANS:
		fpi_ssm_next_state(ssm);
		break;
	case TUNEDC_FINAL_SET_DCOFFSET_REQ:
		msg_set_regs(dev, 2, REG_DCOFFSET, dev->dcoffset);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case TUNEDC_FINAL_SET_DCOFFSET_ANS:
		/* In captured traffic, read GAIN, VRT, and VRB registers. */
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_mark_completed(ssm);
		break;
	default:
		fp_err("Unknown state %d", ssm->cur_state);
		goto err;
		break;
	}

	return;
err:
	fpi_ssm_mark_aborted(ssm, -EIO);

}

static void m_tunedc_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	if (!ssm->error) {
		struct fpi_ssm *ssm_tune;
		ssm_tune = fpi_ssm_new(idev->dev, m_tunevrb_state,
					TUNEVRB_NUM_STATES);
		ssm_tune->priv = idev;
		fpi_ssm_start(ssm_tune, m_tunevrb_complete);
	} else {
		struct etes603_dev *dev = idev->priv;
		fp_err("Error while tuning DCOFFSET");
		dev->is_active = FALSE;
		reset_param(dev);
		fpi_imgdev_session_error(idev, -2);
	}
	fpi_ssm_free(ssm);
}

static void m_init_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	struct etes603_dev *dev = idev->priv;

	if (dev->is_active == FALSE) {
		fpi_ssm_mark_completed(ssm);
		return;
	}

	switch (ssm->cur_state) {
	case INIT_CHECK_INFO_REQ:
		msg_get_regs(dev, 4, REG_INFO0, REG_INFO1, REG_INFO2,
			     REG_INFO3);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case INIT_CHECK_INFO_ANS:
		if (msg_parse_regs(dev))
			goto err;
		if (check_info(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case INIT_CMD20_REQ:
		msg_get_cmd20(dev);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case INIT_CMD20_ANS:
		if (msg_check_cmd20(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case INIT_CMD25_REQ:
		msg_get_cmd25(dev);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case INIT_CMD25_ANS:
		if (msg_check_cmd25(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case INIT_SENSOR_REQ:
		/* In captured traffic, those are splitted. */
		msg_set_regs(dev, 18, REG_MODE_CONTROL, REG_MODE_SLEEP,
			REG_50, 0x0F, REG_GAIN, 0x04, REG_VRT, 0x08,
			REG_VRB, 0x0D, REG_VCO_CONTROL, REG_VCO_RT,
			REG_DCOFFSET, 0x36, REG_F0, 0x00, REG_F2, 0x00);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case INIT_SENSOR_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case INIT_ENC_REQ:
		/* Initialize encryption registers without encryption. */
		/* Set registers from 0x41 to 0x48 (0x8 regs) */
		msg_set_regs(dev, 16, REG_ENC1, 0x12, REG_ENC2, 0x34,
		     REG_ENC3, 0x56, REG_ENC4, 0x78, REG_ENC5, 0x90,
		     REG_ENC6, 0xAB, REG_ENC7, 0xCD, REG_ENC8, 0xEF);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case INIT_ENC_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_next_state(ssm);
		break;
	case INIT_REGS_REQ:
		/* Set register from 0x20 to 0x37 (0x18 regs) */
		msg_set_regs(dev, 48,
		     REG_20, 0x00, REG_21, 0x23, REG_22, 0x21, REG_23, 0x20,
		     REG_24, 0x14, REG_25, 0x6A, REG_26, 0x00, REG_27, 0x00,
		     REG_28, 0x00, REG_29, 0xC0, REG_2A, 0x50, REG_2B, 0x50,
		     REG_2C, 0x4D, REG_2D, 0x03, REG_2E, 0x06, REG_2F, 0x06,
		     REG_30, 0x10, REG_31, 0x02, REG_32, 0x14, REG_33, 0x34,
		     REG_34, 0x01, REG_35, 0x08, REG_36, 0x03, REG_37, 0x21);
		if (async_tx(idev, EP_OUT, async_tx_cb, ssm))
			goto err;
		break;
	case INIT_REGS_ANS:
		if (msg_check_ok(dev))
			goto err;
		fpi_ssm_mark_completed(ssm);
		break;
	default:
		fp_err("Unknown state %d", ssm->cur_state);
		goto err;
		break;
	}

	return;
err:
	fpi_ssm_mark_aborted(ssm, -EIO);

}

static void m_init_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *idev = ssm->priv;
	if (!ssm->error) {
		struct fpi_ssm *ssm_tune;
		ssm_tune = fpi_ssm_new(idev->dev, m_tunedc_state,
					TUNEDC_NUM_STATES);
		ssm_tune->priv = idev;
		fpi_ssm_start(ssm_tune, m_tunedc_complete);
	} else {
		struct etes603_dev *dev = idev->priv;
		fp_err("Error initializing the device");
		dev->is_active = FALSE;
		reset_param(dev);
		fpi_imgdev_session_error(idev, -1);
	}
	fpi_ssm_free(ssm);
}

static int dev_activate(struct fp_img_dev *idev, enum fp_imgdev_state state)
{
	struct etes603_dev *dev = idev->priv;
	struct fpi_ssm *ssm;

	assert(dev);

	if (state != IMGDEV_STATE_AWAIT_FINGER_ON) {
		fp_err("The driver is in an unexpected state: %d.", state);
		fpi_imgdev_activate_complete(idev, 1);
		return -1;
	}

	/* Reset info and data */
	dev->is_active = TRUE;

	if (dev->dcoffset == 0) {
		fp_dbg("Tuning device...");
		ssm = fpi_ssm_new(idev->dev, m_init_state, INIT_NUM_STATES);
		ssm->priv = idev;
		fpi_ssm_start(ssm, m_init_complete);
	} else {
		fp_dbg("Using previous tuning (DCOFFSET=0x%02X,VRT=0x%02X,"
			"VRB=0x%02X,GAIN=0x%02X).", dev->dcoffset, dev->vrt,
			dev->vrb, dev->gain);
		fpi_imgdev_activate_complete(idev, 0);
		ssm = fpi_ssm_new(idev->dev, m_finger_state, FGR_NUM_STATES);
		ssm->priv = idev;
		fpi_ssm_start(ssm, m_finger_complete);
	}
	return 0;
}

static void dev_deactivate(struct fp_img_dev *idev)
{
	struct etes603_dev *dev = idev->priv;

	fp_dbg("deactivating");

	/* this can be called even if still activated. */
	if (dev->is_active == TRUE) {
		dev->is_active = FALSE;
		m_exit_start(idev);
	}
}

static int dev_open(struct fp_img_dev *idev, unsigned long driver_data)
{
	int ret;
	struct etes603_dev *dev;

	dev = g_malloc0(sizeof(struct etes603_dev));
	idev->priv = dev;

	dev->req = g_malloc(sizeof(struct egis_msg));
	dev->ans = g_malloc(FE_SIZE);
	dev->fp = g_malloc(FE_SIZE * 4);

	ret = libusb_claim_interface(idev->udev, 0);
	if (ret != LIBUSB_SUCCESS) {
		fp_err("libusb_claim_interface failed on interface 0: %s", libusb_error_name(ret));
		return ret;
	}

	fpi_imgdev_open_complete(idev, 0);
	return 0;
}

static void dev_close(struct fp_img_dev *idev)
{
	struct etes603_dev *dev = idev->priv;

	g_free(dev->req);
	g_free(dev->ans);
	g_free(dev->fp);
	g_free(dev);

	libusb_release_interface(idev->udev, 0);
	fpi_imgdev_close_complete(idev);
}

static const struct usb_id id_table[] = {
	/* EgisTec (aka Lightuning) ES603 */
	{ .vendor = 0x1c7a, .product = 0x0603},
	{ 0, 0, 0, },
};

struct fp_img_driver etes603_driver = {
	.driver = {
		.id = ETES603_ID,
		.name = FP_COMPONENT,
		.full_name = "EgisTec ES603",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_SWIPE,
	},
	.flags = 0,
	.img_height = -1,
	.img_width = 256,

	.open = dev_open,
	.close = dev_close,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};

