/*
 * Imaging utility functions for libfprint
 * Copyright (C) 2007-2008 Daniel Drake <dsd@gentoo.org>
 * Copyright (C) 2013 Vasily Khoruzhick <anarsoul@gmail.com>
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

#include <pixman.h>
#include <string.h>

#include "fp_internal.h"

struct fp_img *fpi_im_resize(struct fp_img *img, unsigned int w_factor, unsigned int h_factor)
{
	int new_width = img->width * w_factor;
	int new_height = img->height * h_factor;
	pixman_image_t *orig, *resized;
	pixman_transform_t transform;
	struct fp_img *newimg;

	orig = pixman_image_create_bits(PIXMAN_a8, img->width, img->height, (uint32_t *)img->data, img->width);
	resized = pixman_image_create_bits(PIXMAN_a8, new_width, new_height, NULL, new_width);

	pixman_transform_init_identity(&transform);
	pixman_transform_scale(NULL, &transform, pixman_int_to_fixed(w_factor), pixman_int_to_fixed(h_factor));
	pixman_image_set_transform(orig, &transform);
	pixman_image_set_filter(orig, PIXMAN_FILTER_BILINEAR, NULL, 0);
	pixman_image_composite32(PIXMAN_OP_SRC,
		orig, /* src */
		NULL, /* mask */
		resized, /* dst */
		0, 0, /* src x y */
		0, 0, /* mask x y */
		0, 0, /* dst x y */
		new_width, new_height /* width height */
		);

	newimg = fpi_img_new(new_width * new_height);
	newimg->width = new_width;
	newimg->height = new_height;
	newimg->flags = img->flags;

	memcpy(newimg->data, pixman_image_get_data(resized), new_width * new_height);

	pixman_image_unref(orig);
	pixman_image_unref(resized);

	return newimg;
}

