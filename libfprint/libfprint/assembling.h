/*
 * Image assembling routines
 * Shared functions between libfprint Authentec drivers
 * Copyright (C) 2007 Daniel Drake <dsd@gentoo.org>
 * Copyright (C) 2015 Vasily Khoruzhick <anarsoul@gmail.com>
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

#ifndef __ASSEMBLING_H__
#define __ASSEMBLING_H__

#include <fp_internal.h>

struct fpi_frame {
	int delta_x;
	int delta_y;
	unsigned char data[0];
};

struct fpi_frame_asmbl_ctx {
	unsigned frame_width;
	unsigned frame_height;
	unsigned image_width;
	unsigned char (*get_pixel)(struct fpi_frame_asmbl_ctx *ctx,
				   struct fpi_frame *frame,
				   unsigned x,
				   unsigned y);
};

void fpi_do_movement_estimation(struct fpi_frame_asmbl_ctx *ctx,
			    GSList *stripes, size_t stripes_len);

struct fp_img *fpi_assemble_frames(struct fpi_frame_asmbl_ctx *ctx,
			    GSList *stripes, size_t stripes_len);

struct fpi_line_asmbl_ctx {
	unsigned line_width;
	unsigned max_height;
	unsigned resolution;
	unsigned median_filter_size;
	unsigned max_search_offset;
	int (*get_deviation)(struct fpi_line_asmbl_ctx *ctx,
			     GSList *line1, GSList *line2);
	unsigned char (*get_pixel)(struct fpi_line_asmbl_ctx *ctx,
				   GSList *line,
				   unsigned x);
};

struct fp_img *fpi_assemble_lines(struct fpi_line_asmbl_ctx *ctx,
				  GSList *lines, size_t lines_len);

#endif
