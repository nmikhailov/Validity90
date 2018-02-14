/*
 * Digital Persona U.are.U 4000/4000B/4500 driver for libfprint
 * Copyright (C) 2007-2008 Daniel Drake <dsd@gentoo.org>
 * Copyright (C) 2012 Timo Teräs <timo.teras@iki.fi>
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

#define FP_COMPONENT "uru4000"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <nss.h>
#include <pk11pub.h>
#include <libusb.h>

#include <fp_internal.h>

#include "driver_ids.h"

#define EP_INTR			(1 | LIBUSB_ENDPOINT_IN)
#define EP_DATA			(2 | LIBUSB_ENDPOINT_IN)
#define USB_RQ			0x04
#define CTRL_IN			(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)
#define CTRL_OUT		(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)
#define CTRL_TIMEOUT		5000
#define BULK_TIMEOUT		5000
#define IRQ_LENGTH		64
#define CR_LENGTH		16

#define IMAGE_HEIGHT		290
#define IMAGE_WIDTH		384

#define ENC_THRESHOLD		5000

enum {
	IRQDATA_SCANPWR_ON = 0x56aa,
	IRQDATA_FINGER_ON = 0x0101,
	IRQDATA_FINGER_OFF = 0x0200,
	IRQDATA_DEATH = 0x0800,
};

enum {
	REG_HWSTAT = 0x07,
	REG_SCRAMBLE_DATA_INDEX = 0x33,
	REG_SCRAMBLE_DATA_KEY = 0x34,
	REG_MODE = 0x4e,
	REG_DEVICE_INFO = 0xf0,
	/* firmware starts at 0x100 */
	REG_RESPONSE = 0x2000,
	REG_CHALLENGE = 0x2010,
};

enum {
	MODE_INIT = 0x00,
	MODE_AWAIT_FINGER_ON = 0x10,
	MODE_AWAIT_FINGER_OFF = 0x12,
	MODE_CAPTURE = 0x20,
	MODE_CAPTURE_AUX = 0x30,
	MODE_OFF = 0x70,
	MODE_READY = 0x80,
};

enum {
	MS_KBD,
	MS_INTELLIMOUSE,
	MS_STANDALONE,
	MS_STANDALONE_V2,
	DP_URU4000,
	DP_URU4000B,
};

static const struct uru4k_dev_profile {
	const char *name;
	gboolean auth_cr;
	gboolean encryption;
} uru4k_dev_info[] = {
	[MS_KBD] = {
		.name = "Microsoft Keyboard with Fingerprint Reader",
		.auth_cr = FALSE,
	},
	[MS_INTELLIMOUSE] = {
		.name = "Microsoft Wireless IntelliMouse with Fingerprint Reader",
		.auth_cr = FALSE,
	},
	[MS_STANDALONE] = {
		.name = "Microsoft Fingerprint Reader",
		.auth_cr = FALSE,
	},
	[MS_STANDALONE_V2] = {
		.name = "Microsoft Fingerprint Reader v2",
		.auth_cr = TRUE,
	},
	[DP_URU4000] = {
		.name = "Digital Persona U.are.U 4000",
		.auth_cr = FALSE,
	},
	[DP_URU4000B] = {
		.name = "Digital Persona U.are.U 4000B",
		.auth_cr = FALSE,
		.encryption = TRUE,
	},
};

typedef void (*irq_cb_fn)(struct fp_img_dev *dev, int status, uint16_t type,
	void *user_data);
typedef void (*irqs_stopped_cb_fn)(struct fp_img_dev *dev);

struct uru4k_dev {
	const struct uru4k_dev_profile *profile;
	uint8_t interface;
	enum fp_imgdev_state activate_state;
	unsigned char last_reg_rd[16];
	unsigned char last_hwstat;

	struct libusb_transfer *irq_transfer;
	struct libusb_transfer *img_transfer;
	void *img_data;
	uint16_t img_lines_done, img_block;
	uint32_t img_enc_seed;

	irq_cb_fn irq_cb;
	void *irq_cb_data;
	irqs_stopped_cb_fn irqs_stopped_cb;

	int rebootpwr_ctr;
	int powerup_ctr;
	unsigned char powerup_hwstat;

	int scanpwr_irq_timeouts;
	struct fpi_timeout *scanpwr_irq_timeout;

	int fwfixer_offset;
	unsigned char fwfixer_value;

	CK_MECHANISM_TYPE cipher;
	PK11SlotInfo *slot;
	PK11SymKey *symkey;
	SECItem *param;
};

/* For 2nd generation MS devices */
static const unsigned char crkey[] = {
	0x79, 0xac, 0x91, 0x79, 0x5c, 0xa1, 0x47, 0x8e,
	0x98, 0xe0, 0x0f, 0x3c, 0x59, 0x8f, 0x5f, 0x4b,
};

/***** REGISTER I/O *****/

typedef void (*write_regs_cb_fn)(struct fp_img_dev *dev, int status,
	void *user_data);

struct write_regs_data {
	struct fp_img_dev *dev;
	write_regs_cb_fn callback;
	void *user_data;
};

static void write_regs_cb(struct libusb_transfer *transfer)
{
	struct write_regs_data *wrdata = transfer->user_data;
	struct libusb_control_setup *setup =
		libusb_control_transfer_get_setup(transfer);
	int r = 0;
	
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		r = -EIO;
	else if (transfer->actual_length != setup->wLength)
		r = -EPROTO;

	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
	wrdata->callback(wrdata->dev, r, wrdata->user_data);
	g_free(wrdata);
}

static int write_regs(struct fp_img_dev *dev, uint16_t first_reg,
	uint16_t num_regs, unsigned char *values, write_regs_cb_fn callback,
	void *user_data)
{
	struct write_regs_data *wrdata;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *data;
	int r;

	if (!transfer)
		return -ENOMEM;

	wrdata = g_malloc(sizeof(*wrdata));
	wrdata->dev = dev;
	wrdata->callback = callback;
	wrdata->user_data = user_data;

	data = g_malloc(LIBUSB_CONTROL_SETUP_SIZE + num_regs);
	memcpy(data + LIBUSB_CONTROL_SETUP_SIZE, values, num_regs);
	libusb_fill_control_setup(data, CTRL_OUT, USB_RQ, first_reg, 0, num_regs);
	libusb_fill_control_transfer(transfer, dev->udev, data, write_regs_cb,
		wrdata, CTRL_TIMEOUT);

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		g_free(wrdata);
		g_free(data);
		libusb_free_transfer(transfer);
	}
	return r;
}

static int write_reg(struct fp_img_dev *dev, uint16_t reg,
	unsigned char value, write_regs_cb_fn callback, void *user_data)
{
	return write_regs(dev, reg, 1, &value, callback, user_data);
}

typedef void (*read_regs_cb_fn)(struct fp_img_dev *dev, int status,
	uint16_t num_regs, unsigned char *data, void *user_data);

struct read_regs_data {
	struct fp_img_dev *dev;
	read_regs_cb_fn callback;
	void *user_data;
};

static void read_regs_cb(struct libusb_transfer *transfer)
{
	struct read_regs_data *rrdata = transfer->user_data;
	struct libusb_control_setup *setup =
		libusb_control_transfer_get_setup(transfer);
	unsigned char *data = NULL;
	int r = 0;
	
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		r = -EIO;
	else if (transfer->actual_length != setup->wLength)
		r = -EPROTO;
	else
		data = libusb_control_transfer_get_data(transfer);

	rrdata->callback(rrdata->dev, r, transfer->actual_length, data, rrdata->user_data);
	g_free(rrdata);
	g_free(transfer->buffer);
	libusb_free_transfer(transfer);
}

static int read_regs(struct fp_img_dev *dev, uint16_t first_reg,
	uint16_t num_regs, read_regs_cb_fn callback, void *user_data)
{
	struct read_regs_data *rrdata;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *data;
	int r;

	if (!transfer)
		return -ENOMEM;

	rrdata = g_malloc(sizeof(*rrdata));
	rrdata->dev = dev;
	rrdata->callback = callback;
	rrdata->user_data = user_data;

	data = g_malloc(LIBUSB_CONTROL_SETUP_SIZE + num_regs);
	libusb_fill_control_setup(data, CTRL_IN, USB_RQ, first_reg, 0, num_regs);
	libusb_fill_control_transfer(transfer, dev->udev, data, read_regs_cb,
		rrdata, CTRL_TIMEOUT);

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		g_free(rrdata);
		g_free(data);
		libusb_free_transfer(transfer);
	}
	return r;
}

/*
 * HWSTAT
 *
 * This register has caused me a lot of headaches. It pretty much defines
 * code flow, and if you don't get it right, the pretty lights don't come on.
 * I think the situation is somewhat complicated by the fact that writing it
 * doesn't affect the read results in the way you'd expect -- but then again
 * it does have some obvious effects. Here's what we know
 *
 * BIT 7: LOW POWER MODE
 * When this bit is set, the device is partially turned off or something. Some
 * things, like firmware upload, need to be done in this state. But generally
 * we want to clear this bit during late initialization, which can sometimes
 * be tricky.
 *
 * BIT 2: SOMETHING WENT WRONG
 * Not sure about this, but see the init function, as when we detect it,
 * we reboot the device. Well, we mess with hwstat until this evil bit gets
 * cleared.
 *
 * BIT 1: IRQ PENDING
 * Just had a brainwave. This bit is set when the device is trying to deliver
 * and interrupt to the host. Maybe?
 */

static void response_cb(struct fp_img_dev *dev, int status, void *user_data)
{
	struct fpi_ssm *ssm = user_data;
	if (status == 0)
		fpi_ssm_next_state(ssm);
	else
		fpi_ssm_mark_aborted(ssm, status);
}

static void challenge_cb(struct fp_img_dev *dev, int status,
	uint16_t num_regs, unsigned char *data, void *user_data)
{
	struct fpi_ssm *ssm = user_data;
	struct uru4k_dev *urudev = dev->priv;
	unsigned char *respdata;
	PK11Context *ctx;
	int r, outlen;

	r = status;
	if (status != 0) {
		fpi_ssm_mark_aborted(ssm, status);
		return;
	}

	/* submit response */
	/* produce response from challenge */
	respdata = g_malloc(CR_LENGTH);
	ctx = PK11_CreateContextBySymKey(urudev->cipher, CKA_ENCRYPT,
					 urudev->symkey, urudev->param);
	if (PK11_CipherOp(ctx, respdata, &outlen, CR_LENGTH, data, CR_LENGTH) != SECSuccess
	    || PK11_Finalize(ctx) != SECSuccess) {
		fp_err("Failed to encrypt challenge data");
		r = -ECONNABORTED;
		g_free(respdata);
	}
	PK11_DestroyContext(ctx, PR_TRUE);

	if (r >= 0) {
		r = write_regs(dev, REG_RESPONSE, CR_LENGTH, respdata, response_cb, ssm);
		g_free(respdata);
	}
	if (r < 0)
		fpi_ssm_mark_aborted(ssm, r);
}

/*
 * 2nd generation MS devices added an AES-based challenge/response
 * authentication scheme, where the device challenges the authenticity of the
 * driver.
 */
static void sm_do_challenge_response(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	int r;

	fp_dbg("");
	r = read_regs(dev, REG_CHALLENGE, CR_LENGTH, challenge_cb, ssm);
	if (r < 0)
		fpi_ssm_mark_aborted(ssm, r);
}

/***** INTERRUPT HANDLING *****/

#define IRQ_HANDLER_IS_RUNNING(urudev) ((urudev)->irq_transfer)

static int start_irq_handler(struct fp_img_dev *dev);

static void irq_handler(struct libusb_transfer *transfer)
{
	struct fp_img_dev *dev = transfer->user_data;
	struct uru4k_dev *urudev = dev->priv;
	unsigned char *data = transfer->buffer;
	uint16_t type;
	int r = 0;

	if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
		fp_dbg("cancelled");
		if (urudev->irqs_stopped_cb)
			urudev->irqs_stopped_cb(dev);
		urudev->irqs_stopped_cb = NULL;
		goto out;
	} else if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		r = -EIO;
		goto err;
	} else if (transfer->actual_length != transfer->length) {
		fp_err("short interrupt read? %d", transfer->actual_length);
		r = -EPROTO;
		goto err;
	}

	type = GUINT16_FROM_BE(*((uint16_t *) data));
	fp_dbg("recv irq type %04x", type);
	g_free(data);
	libusb_free_transfer(transfer);

	/* The 0800 interrupt seems to indicate imminent failure (0 bytes transfer)
	 * of the next scan. It still appears on occasion. */
	if (type == IRQDATA_DEATH)
		fp_warn("oh no! got the interrupt OF DEATH! expect things to go bad");

	if (urudev->irq_cb)
		urudev->irq_cb(dev, 0, type, urudev->irq_cb_data);
	else
		fp_dbg("ignoring interrupt");

	r = start_irq_handler(dev);
	if (r == 0)
		return;

	transfer = NULL;
	data = NULL;
err:
	if (urudev->irq_cb)
		urudev->irq_cb(dev, r, 0, urudev->irq_cb_data);
out:
	g_free(data);
	libusb_free_transfer(transfer);
	urudev->irq_transfer = NULL;
}

static int start_irq_handler(struct fp_img_dev *dev)
{
	struct uru4k_dev *urudev = dev->priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char *data;
	int r;

	if (!transfer)
		return -ENOMEM;
	
	data = g_malloc(IRQ_LENGTH);
	libusb_fill_bulk_transfer(transfer, dev->udev, EP_INTR, data, IRQ_LENGTH,
		irq_handler, dev, 0);

	urudev->irq_transfer = transfer;
	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		g_free(data);
		libusb_free_transfer(transfer);
		urudev->irq_transfer = NULL;
	}
	return r;
}

static void stop_irq_handler(struct fp_img_dev *dev, irqs_stopped_cb_fn cb)
{
	struct uru4k_dev *urudev = dev->priv;
	struct libusb_transfer *transfer = urudev->irq_transfer;
	if (transfer) {
		libusb_cancel_transfer(transfer);
		urudev->irqs_stopped_cb = cb;
	}
}

/***** STATE CHANGING *****/

static int execute_state_change(struct fp_img_dev *dev);

static void finger_presence_irq_cb(struct fp_img_dev *dev, int status,
	uint16_t type, void *user_data)
{
	if (status)
		fpi_imgdev_session_error(dev, status);
	else if (type == IRQDATA_FINGER_ON)
		fpi_imgdev_report_finger_status(dev, TRUE);
	else if (type == IRQDATA_FINGER_OFF)
		fpi_imgdev_report_finger_status(dev, FALSE);
	else
		fp_warn("ignoring unexpected interrupt %04x", type);
}

static void change_state_write_reg_cb(struct fp_img_dev *dev, int status,
	void *user_data)
{
	if (status)
		fpi_imgdev_session_error(dev, status);
}

static int dev_change_state(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct uru4k_dev *urudev = dev->priv;

	switch (state) {
	case IMGDEV_STATE_INACTIVE:
	case IMGDEV_STATE_AWAIT_FINGER_ON:
	case IMGDEV_STATE_AWAIT_FINGER_OFF:
	case IMGDEV_STATE_CAPTURE:
		break;
	default:
		fp_err("unrecognised state %d", state);
		return -EINVAL;
	}

	urudev->activate_state = state;
	if (urudev->img_transfer != NULL)
		return 0;

	return execute_state_change(dev);
}

/***** GENERIC STATE MACHINE HELPER FUNCTIONS *****/

static void sm_write_reg_cb(struct fp_img_dev *dev, int result, void *user_data)
{
	struct fpi_ssm *ssm = user_data;

	if (result)
		fpi_ssm_mark_aborted(ssm, result);
	else
		fpi_ssm_next_state(ssm);
}

static void sm_write_regs(struct fpi_ssm *ssm, uint16_t first_reg, uint16_t num_regs,
	void *data)
{
	struct fp_img_dev *dev = ssm->priv;
	int r = write_regs(dev, first_reg, num_regs, data, sm_write_reg_cb, ssm);
	if (r < 0)
		fpi_ssm_mark_aborted(ssm, r);
}

static void sm_write_reg(struct fpi_ssm *ssm, uint16_t reg,
	unsigned char value)
{
	sm_write_regs(ssm, reg, 1, &value);
}

static void sm_read_reg_cb(struct fp_img_dev *dev, int result,
	uint16_t num_regs, unsigned char *data, void *user_data)
{
	struct fpi_ssm *ssm = user_data;
	struct uru4k_dev *urudev = dev->priv;

	if (result) {
		fpi_ssm_mark_aborted(ssm, result);
	} else {
		memcpy(urudev->last_reg_rd, data, num_regs);
		fp_dbg("reg value %x", urudev->last_reg_rd[0]);
		fpi_ssm_next_state(ssm);
	}
}

static void sm_read_regs(struct fpi_ssm *ssm, uint16_t reg, uint16_t num_regs)
{
	struct fp_img_dev *dev = ssm->priv;
	struct uru4k_dev *urudev = dev->priv;
	int r;

	if (num_regs > sizeof(urudev->last_reg_rd)) {
		fpi_ssm_mark_aborted(ssm, -EIO);
		return;
	}

	fp_dbg("read %d regs at %x", num_regs, reg);
	r = read_regs(dev, reg, num_regs, sm_read_reg_cb, ssm);
	if (r < 0)
		fpi_ssm_mark_aborted(ssm, r);
}

static void sm_read_reg(struct fpi_ssm *ssm, uint16_t reg)
{
	sm_read_regs(ssm, reg, 1);
}

static void sm_set_hwstat(struct fpi_ssm *ssm, unsigned char value)
{
	fp_dbg("set %02x", value);
	sm_write_reg(ssm, REG_HWSTAT, value);
}

/***** IMAGING LOOP *****/

enum imaging_states {
	IMAGING_CAPTURE,
	IMAGING_SEND_INDEX,
	IMAGING_READ_KEY,
	IMAGING_DECODE,
	IMAGING_REPORT_IMAGE,
	IMAGING_NUM_STATES
};

static void image_transfer_cb(struct libusb_transfer *transfer)
{
	struct fpi_ssm *ssm = transfer->user_data;

	if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
		fp_dbg("cancelled");
		fpi_ssm_mark_aborted(ssm, -ECANCELED);
	} else if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fp_dbg("error");
		fpi_ssm_mark_aborted(ssm, -EIO);
	} else {
		fpi_ssm_next_state(ssm);
	}
}

enum {
	BLOCKF_CHANGE_KEY	= 0x80,
	BLOCKF_NO_KEY_UPDATE	= 0x04,
	BLOCKF_ENCRYPTED		= 0x02,
	BLOCKF_NOT_PRESENT	= 0x01,
};

struct uru4k_image {
	uint8_t		unknown_00[4];
	uint16_t	num_lines;
	uint8_t		key_number;
	uint8_t		unknown_07[9];
	struct {
		uint8_t	flags;
		uint8_t	num_lines;
	} block_info[15];
	uint8_t		unknown_2E[18];
	uint8_t		data[IMAGE_HEIGHT][IMAGE_WIDTH];
};

static uint32_t update_key(uint32_t key)
{
	/* linear feedback shift register
	 * taps at bit positions 1 3 4 7 11 13 20 23 26 29 32 */
	uint32_t bit = key & 0x9248144d;
	bit ^= bit << 16;
	bit ^= bit << 8;
	bit ^= bit << 4;
	bit ^= bit << 2;
	bit ^= bit << 1;
	return (bit & 0x80000000) | (key >> 1);
}

static uint32_t do_decode(uint8_t *data, int num_bytes, uint32_t key)
{
	uint8_t xorbyte;
	int i;

	for (i = 0; i < num_bytes - 1; i++) {
		/* calculate xor byte and update key */
		xorbyte  = ((key >>  4) & 1) << 0;
		xorbyte |= ((key >>  8) & 1) << 1;
		xorbyte |= ((key >> 11) & 1) << 2;
		xorbyte |= ((key >> 14) & 1) << 3;
		xorbyte |= ((key >> 18) & 1) << 4;
		xorbyte |= ((key >> 21) & 1) << 5;
		xorbyte |= ((key >> 24) & 1) << 6;
		xorbyte |= ((key >> 29) & 1) << 7;
		key = update_key(key);

		/* decrypt data */
		data[i] = data[i+1] ^ xorbyte;
	}

	/* the final byte is implictly zero */
	data[i] = 0;
	return update_key(key);
}

static int calc_dev2(struct uru4k_image *img)
{
	uint8_t *b[2] = { NULL, NULL };
	int res = 0, mean = 0, i, r, j, idx;

	for (i = r = idx = 0; i < array_n_elements(img->block_info) && idx < 2; i++) {
		if (img->block_info[i].flags & BLOCKF_NOT_PRESENT)
			continue;
		for (j = 0; j < img->block_info[i].num_lines && idx < 2; j++)
			b[idx++] = img->data[r++];
	}
	if (!b[0] || !b[1]) {
		fp_dbg("NULL! %p %p", b[0], b[1]);
		return 0;
	}
	for (i = 0; i < IMAGE_WIDTH; i++)
		mean += (int)b[0][i] + (int)b[1][i];

	mean /= IMAGE_WIDTH;

	for (i = 0; i < IMAGE_WIDTH; i++) {
		int dev = (int)b[0][i] + (int)b[1][i] - mean;
		res += dev * dev;
	}

	return res / IMAGE_WIDTH;
}

static void imaging_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct uru4k_dev *urudev = dev->priv;
	struct uru4k_image *img = urudev->img_data;
	struct fp_img *fpimg;
	uint32_t key;
	uint8_t flags, num_lines;
	int i, r, to, dev2;
	char buf[5];

	switch (ssm->cur_state) {
	case IMAGING_CAPTURE:
		urudev->img_lines_done = 0;
		urudev->img_block = 0;
		libusb_fill_bulk_transfer(urudev->img_transfer, dev->udev, EP_DATA,
			urudev->img_data, sizeof(struct uru4k_image), image_transfer_cb, ssm, 0);
		r = libusb_submit_transfer(urudev->img_transfer);
		if (r < 0)
			fpi_ssm_mark_aborted(ssm, -EIO);
		break;
	case IMAGING_SEND_INDEX:
		fp_dbg("hw header lines %d", img->num_lines);

		if (img->num_lines >= IMAGE_HEIGHT ||
		    urudev->img_transfer->actual_length < img->num_lines * IMAGE_WIDTH + 64) {
			fp_err("bad captured image (%d lines) or size mismatch %d < %d",
				img->num_lines,
				urudev->img_transfer->actual_length,
				img->num_lines * IMAGE_WIDTH + 64);
			fpi_ssm_jump_to_state(ssm, IMAGING_CAPTURE);
			return;
		}
		if (!urudev->profile->encryption) {
			dev2 = calc_dev2(img);
			fp_dbg("dev2: %d", dev2);
			if (dev2 < ENC_THRESHOLD) {
				fpi_ssm_jump_to_state(ssm, IMAGING_REPORT_IMAGE);
				return;
			}
			fp_info("image seems to be encrypted");
		}
		buf[0] = img->key_number;
		buf[1] = urudev->img_enc_seed;
		buf[2] = urudev->img_enc_seed >> 8;
		buf[3] = urudev->img_enc_seed >> 16;
		buf[4] = urudev->img_enc_seed >> 24;
		sm_write_regs(ssm, REG_SCRAMBLE_DATA_INDEX, 5, buf);
		break;
	case IMAGING_READ_KEY:
		sm_read_regs(ssm, REG_SCRAMBLE_DATA_KEY, 4);
		break;
	case IMAGING_DECODE:
		key  = urudev->last_reg_rd[0];
		key |= urudev->last_reg_rd[1] << 8;
		key |= urudev->last_reg_rd[2] << 16;
		key |= urudev->last_reg_rd[3] << 24;
		key ^= urudev->img_enc_seed;

		fp_dbg("encryption id %02x -> key %08x", img->key_number, key);
		while (urudev->img_block < array_n_elements(img->block_info) &&
				urudev->img_lines_done < img->num_lines) {
			flags = img->block_info[urudev->img_block].flags;
			num_lines = img->block_info[urudev->img_block].num_lines;
			if (num_lines == 0)
				break;

			fp_dbg("%d %02x %d", urudev->img_block, flags, num_lines);
			if (flags & BLOCKF_CHANGE_KEY) {
				fp_dbg("changing encryption keys.\n");
				img->block_info[urudev->img_block].flags &= ~BLOCKF_CHANGE_KEY;
				img->key_number++;
				urudev->img_enc_seed = rand();
				fpi_ssm_jump_to_state(ssm, IMAGING_SEND_INDEX);
				return;
			}
			switch (flags & (BLOCKF_NO_KEY_UPDATE | BLOCKF_ENCRYPTED)) {
			case BLOCKF_ENCRYPTED:
				fp_dbg("decoding %d lines", num_lines);
				key = do_decode(&img->data[urudev->img_lines_done][0],
						IMAGE_WIDTH*num_lines, key);
				break;
			case 0:
				fp_dbg("skipping %d lines", num_lines);
				for (r = 0; r < IMAGE_WIDTH*num_lines; r++)
					key = update_key(key);
				break;
			}
			if ((flags & BLOCKF_NOT_PRESENT) == 0)
				urudev->img_lines_done += num_lines;
			urudev->img_block++;
		}
		fpi_ssm_next_state(ssm);
		break;
	case IMAGING_REPORT_IMAGE:
		fpimg = fpi_img_new_for_imgdev(dev);

		to = r = 0;
		for (i = 0; i < array_n_elements(img->block_info) && r < img->num_lines; i++) {
			flags = img->block_info[i].flags;
			num_lines = img->block_info[i].num_lines;
			if (num_lines == 0)
				break;
			memcpy(&fpimg->data[to], &img->data[r][0],
				num_lines * IMAGE_WIDTH);
			if (!(flags & BLOCKF_NOT_PRESENT))
				r += num_lines;
			to += num_lines * IMAGE_WIDTH;
		}

		fpimg->flags = FP_IMG_COLORS_INVERTED;
		if (!urudev->profile->encryption)
			fpimg->flags |= FP_IMG_V_FLIPPED | FP_IMG_H_FLIPPED;
		fpi_imgdev_image_captured(dev, fpimg);

		if (urudev->activate_state == IMGDEV_STATE_CAPTURE)
			fpi_ssm_jump_to_state(ssm, IMAGING_CAPTURE);
		else
			fpi_ssm_mark_completed(ssm);
		break;
	}
}

static void imaging_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct uru4k_dev *urudev = dev->priv;
	int r = ssm->error;
	fpi_ssm_free(ssm);

	/* Report error before exiting imaging loop - the error handler
	 * can request state change, which needs to be postponed to end of
	 * this function. */
	if (r)
		fpi_imgdev_session_error(dev, r);

	g_free(urudev->img_data);
	urudev->img_data = NULL;

	libusb_free_transfer(urudev->img_transfer);
	urudev->img_transfer = NULL;

	r = execute_state_change(dev);
	if (r)
		fpi_imgdev_session_error(dev, r);
}

/***** INITIALIZATION *****/

/* After closing an app and setting hwstat to 0x80, my ms keyboard gets in a
 * confused state and returns hwstat 0x85. On next app run, we don't get the
 * 56aa interrupt. This is the best way I've found to fix it: mess around
 * with hwstat until it starts returning more recognisable values. This
 * doesn't happen on my other devices: uru4000, uru4000b, ms fp rdr v2 
 *
 * The windows driver copes with this OK, but then again it uploads firmware
 * right after reading the 0x85 hwstat, allowing some time to pass before it
 * attempts to tweak hwstat again...
 *
 * This is implemented with a reboot power state machine. the ssm runs during
 * initialization if bits 2 and 7 are set in hwstat. it masks off the 4 high
 * hwstat bits then checks that bit 1 is set. if not, it pauses before reading
 * hwstat again. machine completes when reading hwstat shows bit 1 is set,
 * and fails after 100 tries. */

enum rebootpwr_states {
	REBOOTPWR_SET_HWSTAT = 0,
	REBOOTPWR_GET_HWSTAT,
	REBOOTPWR_CHECK_HWSTAT,
	REBOOTPWR_PAUSE,
	REBOOTPWR_NUM_STATES,
};

static void rebootpwr_pause_cb(void *data)
{
	struct fpi_ssm *ssm = data;
	struct fp_img_dev *dev = ssm->priv;
	struct uru4k_dev *urudev = dev->priv;

	if (!--urudev->rebootpwr_ctr) {
		fp_err("could not reboot device power");
		fpi_ssm_mark_aborted(ssm, -EIO);
	} else {
		fpi_ssm_jump_to_state(ssm, REBOOTPWR_GET_HWSTAT);
	}
}

static void rebootpwr_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct uru4k_dev *urudev = dev->priv;

	switch (ssm->cur_state) {
	case REBOOTPWR_SET_HWSTAT:
		urudev->rebootpwr_ctr = 100;
		sm_set_hwstat(ssm, urudev->last_hwstat & 0xf);
		break;
	case REBOOTPWR_GET_HWSTAT:
		sm_read_reg(ssm, REG_HWSTAT);
		break;
	case REBOOTPWR_CHECK_HWSTAT:
		urudev->last_hwstat = urudev->last_reg_rd[0];
		if (urudev->last_hwstat & 0x1)
			fpi_ssm_mark_completed(ssm);
		else
			fpi_ssm_next_state(ssm);
		break;
	case REBOOTPWR_PAUSE:
		if (fpi_timeout_add(10, rebootpwr_pause_cb, ssm) == NULL)
			fpi_ssm_mark_aborted(ssm, -ETIME);
		break;
	}
}

/* After messing with the device firmware in it's low-power state, we have to
 * power it back up and wait for interrupt notification. It's not quite as easy
 * as that: the combination of both modifying firmware *and* doing C-R auth on
 * my ms fp v2 device causes us not to get to get the 56aa interrupt and
 * for the hwstat write not to take effect. We have to loop a few times,
 * authenticating each time, until the device wakes up.
 *
 * This is implemented as the powerup state machine below. Pseudo-code:

	status = get_hwstat();
	for (i = 0; i < 100; i++) {
		set_hwstat(status & 0xf);
		if ((get_hwstat() & 0x80) == 0)
			break;

		usleep(10000);
		if (need_auth_cr)
			auth_cr();
	}

	if (tmp & 0x80)
		error("could not power up device");

 */

enum powerup_states {
	POWERUP_INIT = 0,
	POWERUP_SET_HWSTAT,
	POWERUP_GET_HWSTAT,
	POWERUP_CHECK_HWSTAT,
	POWERUP_PAUSE,
	POWERUP_CHALLENGE_RESPONSE,
	POWERUP_CHALLENGE_RESPONSE_SUCCESS,
	POWERUP_NUM_STATES,
};

static void powerup_pause_cb(void *data)
{
	struct fpi_ssm *ssm = data;
	struct fp_img_dev *dev = ssm->priv;
	struct uru4k_dev *urudev = dev->priv;

	if (!--urudev->powerup_ctr) {
		fp_err("could not power device up");
		fpi_ssm_mark_aborted(ssm, -EIO);
	} else if (!urudev->profile->auth_cr) {
		fpi_ssm_jump_to_state(ssm, POWERUP_SET_HWSTAT);
	} else {
		fpi_ssm_next_state(ssm);
	}
}

static void powerup_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct uru4k_dev *urudev = dev->priv;

	switch (ssm->cur_state) {
	case POWERUP_INIT:
		urudev->powerup_ctr = 100;
		urudev->powerup_hwstat = urudev->last_hwstat & 0xf;
		fpi_ssm_next_state(ssm);
		break;
	case POWERUP_SET_HWSTAT:
		sm_set_hwstat(ssm, urudev->powerup_hwstat);
		break;
	case POWERUP_GET_HWSTAT:
		sm_read_reg(ssm, REG_HWSTAT);
		break;
	case POWERUP_CHECK_HWSTAT:
		urudev->last_hwstat = urudev->last_reg_rd[0];
		if ((urudev->last_reg_rd[0] & 0x80) == 0)
			fpi_ssm_mark_completed(ssm);
		else
			fpi_ssm_next_state(ssm);
		break;
	case POWERUP_PAUSE:
		if (fpi_timeout_add(10, powerup_pause_cb, ssm) == NULL)
			fpi_ssm_mark_aborted(ssm, -ETIME);
		break;
	case POWERUP_CHALLENGE_RESPONSE:
		sm_do_challenge_response(ssm);
		break;
	case POWERUP_CHALLENGE_RESPONSE_SUCCESS:
		fpi_ssm_jump_to_state(ssm, POWERUP_SET_HWSTAT);
		break;
	}
}

/*
 * This is the main initialization state machine. As pseudo-code:

	status = get_hwstat();

	// correct device power state
	if ((status & 0x84) == 0x84)
		run_reboot_sm();

	// power device down
	if ((status & 0x80) == 0)
		set_hwstat(status | 0x80);

	// power device up
	run_powerup_sm();
	await_irq(IRQDATA_SCANPWR_ON);
 */

enum init_states {
	INIT_GET_HWSTAT = 0,
	INIT_CHECK_HWSTAT_REBOOT,
	INIT_REBOOT_POWER,
	INIT_CHECK_HWSTAT_POWERDOWN,
	INIT_POWERUP,
	INIT_AWAIT_SCAN_POWER,
	INIT_DONE,
	INIT_GET_VERSION,
	INIT_REPORT_VERSION,
	INIT_NUM_STATES,
};

static void init_scanpwr_irq_cb(struct fp_img_dev *dev, int status,
	uint16_t type, void *user_data)
{
	struct fpi_ssm *ssm = user_data;
	struct uru4k_dev *urudev = dev->priv;

	if (status)
		fpi_ssm_mark_aborted(ssm, status);
	else if (type != IRQDATA_SCANPWR_ON)
		fp_dbg("ignoring interrupt");
	else if (ssm->cur_state != INIT_AWAIT_SCAN_POWER) {
		fp_dbg("early scanpwr interrupt");
		urudev->scanpwr_irq_timeouts = -1;
	} else {
		fp_dbg("late scanpwr interrupt");
		fpi_ssm_next_state(ssm);
	}
}

static void init_scanpwr_timeout(void *user_data)
{
	struct fpi_ssm *ssm = user_data;
	struct fp_img_dev *dev = ssm->priv;
	struct uru4k_dev *urudev = dev->priv;

	fp_warn("powerup timed out");
	urudev->irq_cb = NULL;
	urudev->scanpwr_irq_timeout = NULL;

	if (++urudev->scanpwr_irq_timeouts >= 3) {
		fp_err("powerup timed out 3 times, giving up");
		fpi_ssm_mark_aborted(ssm, -ETIMEDOUT);
	} else {
		fpi_ssm_jump_to_state(ssm, INIT_GET_HWSTAT);
	}
}

static void init_run_state(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct uru4k_dev *urudev = dev->priv;

	switch (ssm->cur_state) {
	case INIT_GET_HWSTAT:
		sm_read_reg(ssm, REG_HWSTAT);
		break;
	case INIT_CHECK_HWSTAT_REBOOT:
		urudev->last_hwstat = urudev->last_reg_rd[0];
		if ((urudev->last_hwstat & 0x84) == 0x84)
			fpi_ssm_next_state(ssm);
		else
			fpi_ssm_jump_to_state(ssm, INIT_CHECK_HWSTAT_POWERDOWN);
		break;
	case INIT_REBOOT_POWER: ;
		struct fpi_ssm *rebootsm = fpi_ssm_new(dev->dev, rebootpwr_run_state,
			REBOOTPWR_NUM_STATES);
		rebootsm->priv = dev;
		fpi_ssm_start_subsm(ssm, rebootsm);
		break;
	case INIT_CHECK_HWSTAT_POWERDOWN:
		if ((urudev->last_hwstat & 0x80) == 0)
			sm_set_hwstat(ssm, urudev->last_hwstat | 0x80);
		else
			fpi_ssm_next_state(ssm);
		break;
	case INIT_POWERUP: ;
		if (!IRQ_HANDLER_IS_RUNNING(urudev)) {
			fpi_ssm_mark_aborted(ssm, -EIO);
			break;
		}
		urudev->irq_cb_data = ssm;
		urudev->irq_cb = init_scanpwr_irq_cb;

		struct fpi_ssm *powerupsm = fpi_ssm_new(dev->dev, powerup_run_state,
			POWERUP_NUM_STATES);
		powerupsm->priv = dev;
		fpi_ssm_start_subsm(ssm, powerupsm);
		break;
	case INIT_AWAIT_SCAN_POWER:
		if (urudev->scanpwr_irq_timeouts < 0) {
			fpi_ssm_next_state(ssm);
			break;
		}

		/* sometimes the 56aa interrupt that we are waiting for never arrives,
		 * so we include this timeout loop to retry the whole process 3 times
		 * if we don't get an irq any time soon. */
		urudev->scanpwr_irq_timeout = fpi_timeout_add(300,
			init_scanpwr_timeout, ssm);
		if (!urudev->scanpwr_irq_timeout) {
			fpi_ssm_mark_aborted(ssm, -ETIME);
			break;
		}
		break;
	case INIT_DONE:
		if (urudev->scanpwr_irq_timeout) {
			fpi_timeout_cancel(urudev->scanpwr_irq_timeout);
			urudev->scanpwr_irq_timeout = NULL;
		}
		urudev->irq_cb_data = NULL;
		urudev->irq_cb = NULL;
		fpi_ssm_next_state(ssm);
		break;
	case INIT_GET_VERSION:
		sm_read_regs(ssm, REG_DEVICE_INFO, 16);
		break;
	case INIT_REPORT_VERSION:
		/* Likely hardware revision, and firmware version.
		 * Not sure which is which. */
		fp_info("Versions %02x%02x and %02x%02x",
			urudev->last_reg_rd[10], urudev->last_reg_rd[11],
			urudev->last_reg_rd[4],  urudev->last_reg_rd[5]);
		fpi_ssm_mark_completed(ssm);
		break;
	}
}

static void activate_initsm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	int r = ssm->error;
	fpi_ssm_free(ssm);

	if (r) {
		fpi_imgdev_activate_complete(dev, r);
		return;
	}

	r = execute_state_change(dev);
	fpi_imgdev_activate_complete(dev, r);
}

/* FIXME: having state parameter here is kinda useless, will we ever
 * see a scenario where the parameter is useful so early on in the activation
 * process? asynchronity means that it'll only be used in a later function
 * call. */
static int dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct uru4k_dev *urudev = dev->priv;
	struct fpi_ssm *ssm;
	int r;

	r = start_irq_handler(dev);
	if (r < 0)
		return r;

	urudev->scanpwr_irq_timeouts = 0;
	urudev->activate_state = state;
	ssm = fpi_ssm_new(dev->dev, init_run_state, INIT_NUM_STATES);
	ssm->priv = dev;
	fpi_ssm_start(ssm, activate_initsm_complete);
	return 0;
}

/***** DEINITIALIZATION *****/

static void deactivate_irqs_stopped(struct fp_img_dev *dev)
{
	fpi_imgdev_deactivate_complete(dev);
}

static void deactivate_write_reg_cb(struct fp_img_dev *dev, int status,
	void *user_data)
{
	stop_irq_handler(dev, deactivate_irqs_stopped);
}

static void dev_deactivate(struct fp_img_dev *dev)
{
	dev_change_state(dev, IMGDEV_STATE_INACTIVE);
}

static int execute_state_change(struct fp_img_dev *dev)
{
	struct uru4k_dev *urudev = dev->priv;
	struct fpi_ssm *ssm;

	switch (urudev->activate_state) {
	case IMGDEV_STATE_INACTIVE:
		fp_dbg("deactivating");
		urudev->irq_cb = NULL;
		urudev->irq_cb_data = NULL;
		return write_reg(dev, REG_MODE, MODE_OFF,
			deactivate_write_reg_cb, NULL);
		break;

	case IMGDEV_STATE_AWAIT_FINGER_ON:
		fp_dbg("wait finger on");
		if (!IRQ_HANDLER_IS_RUNNING(urudev))
			return -EIO;
		urudev->irq_cb = finger_presence_irq_cb;
		return write_reg(dev, REG_MODE, MODE_AWAIT_FINGER_ON,
			change_state_write_reg_cb, NULL);

	case IMGDEV_STATE_CAPTURE:
		fp_dbg("starting capture");
		urudev->irq_cb = NULL;

		urudev->img_transfer = libusb_alloc_transfer(0);
		urudev->img_data = g_malloc(sizeof(struct uru4k_image));
		urudev->img_enc_seed = rand();

		ssm = fpi_ssm_new(dev->dev, imaging_run_state, IMAGING_NUM_STATES);
		ssm->priv = dev;
		fpi_ssm_start(ssm, imaging_complete);

		return write_reg(dev, REG_MODE, MODE_CAPTURE,
			change_state_write_reg_cb, NULL);

	case IMGDEV_STATE_AWAIT_FINGER_OFF:
		fp_dbg("await finger off");
		if (!IRQ_HANDLER_IS_RUNNING(urudev))
			return -EIO;
		urudev->irq_cb = finger_presence_irq_cb;
		return write_reg(dev, REG_MODE, MODE_AWAIT_FINGER_OFF,
			change_state_write_reg_cb, NULL);
	}

	return 0;
}

/***** LIBRARY STUFF *****/

static int dev_init(struct fp_img_dev *dev, unsigned long driver_data)
{
	struct libusb_config_descriptor *config;
	const struct libusb_interface *iface = NULL;
	const struct libusb_interface_descriptor *iface_desc;
	const struct libusb_endpoint_descriptor *ep;
	struct uru4k_dev *urudev;
	SECStatus rv;
	SECItem item;
	int i;
	int r;

	/* Find fingerprint interface */
	r = libusb_get_config_descriptor(libusb_get_device(dev->udev), 0, &config);
	if (r < 0) {
		fp_err("Failed to get config descriptor");
		return r;
	}
	for (i = 0; i < config->bNumInterfaces; i++) {
		const struct libusb_interface *cur_iface = &config->interface[i];

		if (cur_iface->num_altsetting < 1)
			continue;

		iface_desc = &cur_iface->altsetting[0];
		if (iface_desc->bInterfaceClass == 255
				&& iface_desc->bInterfaceSubClass == 255 
				&& iface_desc->bInterfaceProtocol == 255) {
			iface = cur_iface;
			break;
		}
	}

	if (iface == NULL) {
		fp_err("could not find interface");
		r = -ENODEV;
		goto out;
	}

	/* Find/check endpoints */

	if (iface_desc->bNumEndpoints != 2) {
		fp_err("found %d endpoints!?", iface_desc->bNumEndpoints);
		r = -ENODEV;
		goto out;
	}

	ep = &iface_desc->endpoint[0];
	if (ep->bEndpointAddress != EP_INTR
			|| (ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) !=
				LIBUSB_TRANSFER_TYPE_INTERRUPT) {
		fp_err("unrecognised interrupt endpoint");
		r = -ENODEV;
		goto out;
	}

	ep = &iface_desc->endpoint[1];
	if (ep->bEndpointAddress != EP_DATA
			|| (ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) !=
				LIBUSB_TRANSFER_TYPE_BULK) {
		fp_err("unrecognised bulk endpoint");
		r = -ENODEV;
		goto out;
	}

	/* Device looks like a supported reader */

	r = libusb_claim_interface(dev->udev, iface_desc->bInterfaceNumber);
	if (r < 0) {
		fp_err("interface claim failed: %s", libusb_error_name(r));
		goto out;
	}

	/* Initialise NSS early */
	rv = NSS_NoDB_Init(".");
	if (rv != SECSuccess) {
		fp_err("could not initialise NSS");
		goto out;
	}

	urudev = g_malloc0(sizeof(*urudev));
	urudev->profile = &uru4k_dev_info[driver_data];
	urudev->interface = iface_desc->bInterfaceNumber;

	/* Set up encryption */
	urudev->cipher = CKM_AES_ECB;
	urudev->slot = PK11_GetBestSlot(urudev->cipher, NULL);
	if (urudev->slot == NULL) {
		fp_err("could not get encryption slot");
		goto out;
	}
	item.type = siBuffer;
	item.data = (unsigned char*) crkey;
	item.len = sizeof(crkey);
	urudev->symkey = PK11_ImportSymKey(urudev->slot,
					   urudev->cipher,
					   PK11_OriginUnwrap,
					   CKA_ENCRYPT,
					   &item, NULL);
	if (urudev->symkey == NULL) {
		fp_err("failed to import key into NSS");
		PK11_FreeSlot(urudev->slot);
		urudev->slot = NULL;
		goto out;
	}
	urudev->param = PK11_ParamFromIV(urudev->cipher, NULL);

	dev->priv = urudev;
	fpi_imgdev_open_complete(dev, 0);

out:
	libusb_free_config_descriptor(config);
	return r;
}

static void dev_deinit(struct fp_img_dev *dev)
{
	struct uru4k_dev *urudev = dev->priv;
	if (urudev->symkey)
		PK11_FreeSymKey (urudev->symkey);
	if (urudev->param)
		SECITEM_FreeItem(urudev->param, PR_TRUE);
	if (urudev->slot)
		PK11_FreeSlot(urudev->slot);
	libusb_release_interface(dev->udev, urudev->interface);
	g_free(urudev);
	fpi_imgdev_close_complete(dev);
}

static const struct usb_id id_table[] = {
	/* ms kbd with fp rdr */
	{ .vendor = 0x045e, .product = 0x00bb, .driver_data = MS_KBD },

	/* ms intellimouse with fp rdr */
	{ .vendor = 0x045e, .product = 0x00bc, .driver_data = MS_INTELLIMOUSE },

	/* ms fp rdr (standalone) */
	{ .vendor = 0x045e, .product = 0x00bd, .driver_data = MS_STANDALONE },

	/* ms fp rdr (standalone) v2 */
	{ .vendor = 0x045e, .product = 0x00ca, .driver_data = MS_STANDALONE_V2 },

	/* dp uru4000 (standalone) */
	{ .vendor = 0x05ba, .product = 0x0007, .driver_data = DP_URU4000 },

	/* dp uru4000 (keyboard) */
	{ .vendor = 0x05ba, .product = 0x0008, .driver_data = DP_URU4000 },

	/* dp uru4000b (standalone) */
	{ .vendor = 0x05ba, .product = 0x000a, .driver_data = DP_URU4000B },

	/* terminating entry */
	{ 0, 0, 0, },
};

struct fp_img_driver uru4000_driver = {
	.driver = {
		.id = URU4000_ID,
		.name = FP_COMPONENT,
		.full_name = "Digital Persona U.are.U 4000/4000B/4500",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_PRESS,
	},
	.flags = FP_IMGDRV_SUPPORTS_UNCONDITIONAL_CAPTURE,
	.img_height = IMAGE_HEIGHT,
	.img_width = IMAGE_WIDTH,

	.open = dev_init,
	.close = dev_deinit,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
	.change_state = dev_change_state,
};

