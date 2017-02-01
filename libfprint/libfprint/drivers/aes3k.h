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

#ifndef __AES3K_H
#define __AES3K_H

#define AES3K_FRAME_HEIGHT	16

struct aes3k_dev {
	struct libusb_transfer *img_trf;
	size_t frame_width;  /* image size = frame_width x frame_width */
	size_t frame_size;   /* 4 bits/pixel: frame_width x AES3K_FRAME_HEIGHT / 2 */
	size_t frame_number; /* number of frames */
	size_t enlarge_factor;

	size_t data_buflen;             /* buffer length of usb bulk transfer */
	struct aes_regwrite *init_reqs; /* initial values sent to device */
	size_t init_reqs_len;
};


int aes3k_dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state);
void aes3k_dev_deactivate(struct fp_img_dev *dev);

#endif
