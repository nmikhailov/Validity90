/*
 * Shared functions between libfprint Authentec drivers
 * Copyright (C) 2007 Daniel Drake <dsd@gentoo.org>
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

#ifndef __AESLIB_H__
#define __AESLIB_H__

#include <fp_internal.h>

struct aes_regwrite {
	unsigned char reg;
	unsigned char value;
};

struct fpi_frame;
struct fpi_frame_asmbl_ctx;

typedef void (*aes_write_regv_cb)(struct fp_img_dev *dev, int result,
	void *user_data);

void aes_write_regv(struct fp_img_dev *dev, const struct aes_regwrite *regs,
	unsigned int num_regs, aes_write_regv_cb callback, void *user_data);

unsigned char aes_get_pixel(struct fpi_frame_asmbl_ctx *ctx,
			    struct fpi_frame *frame,
			    unsigned int x,
			    unsigned int y);

#endif

