/*
 * Image assembling routines
 * Copyright (C) 2007-2008 Daniel Drake <dsd@gentoo.org>
 * Copyright (C) 2013 Arseniy Lartsev <arseniy@chalmers.se>
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

#define FP_COMPONENT "assembling"

#include <errno.h>
#include <string.h>

#include <libusb.h>
#include <glib.h>

#include "fp_internal.h"
#include "assembling.h"

static unsigned int calc_error(struct fpi_frame_asmbl_ctx *ctx,
			       struct fpi_frame *first_frame,
			       struct fpi_frame *second_frame,
			       int dx,
			       int dy)
{
	unsigned int width, height;
	unsigned int x1, y1, x2, y2, err, i, j;

	width = ctx->frame_width - (dx > 0 ? dx : -dx);
	height = ctx->frame_height - dy;

	y1 = 0;
	y2 = dy;
	i = 0;
	err = 0;
	do {
		x1 = dx < 0 ? 0 : dx;
		x2 = dx < 0 ? -dx : 0;
		j = 0;

		do {
			unsigned char v1, v2;


			v1 = ctx->get_pixel(ctx, first_frame, x1, y1);
			v2 = ctx->get_pixel(ctx, second_frame, x2, y2);
			err += v1 > v2 ? v1 - v2 : v2 - v1;
			j++;
			x1++;
			x2++;

		} while (j < width);
		i++;
		y1++;
		y2++;
	} while (i < height);

	/* Normalize error */
	err *= (ctx->frame_height * ctx->frame_width);
	err /= (height * width);

	if (err == 0)
		return INT_MAX;

	return err;
}

/* This function is rather CPU-intensive. It's better to use hardware
 * to detect movement direction when possible.
 */
static void find_overlap(struct fpi_frame_asmbl_ctx *ctx,
			 struct fpi_frame *first_frame,
			 struct fpi_frame *second_frame,
			 unsigned int *min_error)
{
	int dx, dy;
	unsigned int err;
	*min_error = 255 * ctx->frame_height * ctx->frame_width;

	/* Seeking in horizontal and vertical dimensions,
	 * for horizontal dimension we'll check only 8 pixels
	 * in both directions. For vertical direction diff is
	 * rarely less than 2, so start with it.
	 */
	for (dy = 2; dy < ctx->frame_height; dy++) {
		for (dx = -8; dx < 8; dx++) {
			err = calc_error(ctx, first_frame, second_frame,
				dx, dy);
			if (err < *min_error) {
				*min_error = err;
				second_frame->delta_x = -dx;
				second_frame->delta_y = dy;
			}
		}
	}
}

static unsigned int do_movement_estimation(struct fpi_frame_asmbl_ctx *ctx,
			    GSList *stripes, size_t num_stripes,
			    gboolean reverse)
{
	GSList *list_entry = stripes;
	GTimer *timer;
	int frame = 1;
	struct fpi_frame *prev_stripe = list_entry->data;
	unsigned int min_error;
	/* Max error is width * height * 255, for AES2501 which has the largest
	 * sensor its 192*16*255 = 783360. So for 32bit value it's ~5482 frame before
	 * we might get int overflow. Use 64bit value here to prevent integer overflow
	 */
	unsigned long long total_error = 0;

	list_entry = g_slist_next(list_entry);

	timer = g_timer_new();
	do {
		struct fpi_frame *cur_stripe = list_entry->data;

		if (reverse) {
			find_overlap(ctx, prev_stripe, cur_stripe, &min_error);
			prev_stripe->delta_y = -prev_stripe->delta_y;
			prev_stripe->delta_x = -prev_stripe->delta_x;
		}
		else
			find_overlap(ctx, cur_stripe, prev_stripe, &min_error);
		total_error += min_error;

		frame++;
		prev_stripe = cur_stripe;
		list_entry = g_slist_next(list_entry);

	} while (frame < num_stripes);

	g_timer_stop(timer);
	fp_dbg("calc delta completed in %f secs", g_timer_elapsed(timer, NULL));
	g_timer_destroy(timer);

	return total_error / num_stripes;
}

void fpi_do_movement_estimation(struct fpi_frame_asmbl_ctx *ctx,
			    GSList *stripes, size_t num_stripes)
{
	int err, rev_err;
	err = do_movement_estimation(ctx, stripes, num_stripes, FALSE);
	rev_err = do_movement_estimation(ctx, stripes, num_stripes, TRUE);
	fp_dbg("errors: %d rev: %d", err, rev_err);
	if (err < rev_err) {
		do_movement_estimation(ctx, stripes, num_stripes, FALSE);
	}
}

static inline void aes_blit_stripe(struct fpi_frame_asmbl_ctx *ctx,
				   struct fp_img *img,
				   struct fpi_frame *stripe,
				   int x, int y)
{
	unsigned int ix, iy;
	unsigned int fx, fy;
	unsigned int width, height;

	/* Find intersection */
	if (x < 0) {
		width = ctx->frame_width + x;
		ix = 0;
		fx = -x;
	} else {
		ix = x;
		fx = 0;
		width = ctx->frame_width;
	}
	if ((ix + width) > img->width)
		width = img->width - ix;

	if (y < 0) {
		iy = 0;
		fy = -y;
		height = ctx->frame_height + y;
	} else {
		iy = y;
		fy = 0;
		height = ctx->frame_height;
	}

	if (fx > ctx->frame_width)
		return;

	if (fy > ctx->frame_height)
		return;

	if (ix > img->width)
		return;

	if (iy > img->height)
		return;

	if ((iy + height) > img->height)
		height = img->height - iy;

	for (; fy < height; fy++, iy++) {
		if (x < 0) {
			ix = 0;
			fx = -x;
		} else {
			ix = x;
			fx = 0;
		}
		for (; fx < width; fx++, ix++) {
			img->data[ix + (iy * img->width)] = ctx->get_pixel(ctx, stripe, fx, fy);
		}
	}
}

struct fp_img *fpi_assemble_frames(struct fpi_frame_asmbl_ctx *ctx,
			    GSList *stripes, size_t stripes_len)
{
	GSList *stripe;
	struct fp_img *img;
	int height = 0;
	int i, y, x;
	gboolean reverse = FALSE;
	struct fpi_frame *fpi_frame;

	BUG_ON(stripes_len == 0);
	BUG_ON(ctx->image_width < ctx->frame_width);

	/* Calculate height */
	i = 0;
	stripe = stripes;

	/* No offset for 1st image */
	fpi_frame = stripe->data;
	fpi_frame->delta_x = 0;
	fpi_frame->delta_y = 0;
	do {
		fpi_frame = stripe->data;

		height += fpi_frame->delta_y;
		i++;
		stripe = g_slist_next(stripe);
	} while (i < stripes_len);

	fp_dbg("height is %d", height);

	if (height < 0) {
		reverse = TRUE;
		height = -height;
	}

	/* For last frame */
	height += ctx->frame_height;

	/* Create buffer big enough for max image */
	img = fpi_img_new(ctx->image_width * height);
	img->flags = FP_IMG_COLORS_INVERTED;
	img->flags |= reverse ? 0 :  FP_IMG_H_FLIPPED | FP_IMG_V_FLIPPED;
	img->width = ctx->image_width;
	img->height = height;

	/* Assemble stripes */
	i = 0;
	stripe = stripes;
	y = reverse ? (height - ctx->frame_height) : 0;
	x = (ctx->image_width - ctx->frame_width) / 2;

	do {
		fpi_frame = stripe->data;

		y += fpi_frame->delta_y;
		x += fpi_frame->delta_x;

		aes_blit_stripe(ctx, img, fpi_frame, x, y);

		stripe = g_slist_next(stripe);
		i++;
	} while (i < stripes_len);

	return img;
}

static int cmpint(const void *p1, const void *p2, gpointer data)
{
	int a = *((int *)p1);
	int b = *((int *)p2);
	if (a < b)
		return -1;
	else if (a == b)
		return 0;
	else
		return 1;
}

static void median_filter(int *data, int size, int filtersize)
{
	int i;
	int *result = (int *)g_malloc0(size*sizeof(int));
	int *sortbuf = (int *)g_malloc0(filtersize*sizeof(int));
	for (i = 0; i < size; i++) {
		int i1 = i - (filtersize-1)/2;
		int i2 = i + (filtersize-1)/2;
		if (i1 < 0)
			i1 = 0;
		if (i2 >= size)
			i2 = size-1;
		g_memmove(sortbuf, data+i1, (i2-i1+1)*sizeof(int));
		g_qsort_with_data(sortbuf, i2-i1+1, sizeof(int), cmpint, NULL);
		result[i] = sortbuf[(i2-i1+1)/2];
	}
	memmove(data, result, size*sizeof(int));
	g_free(result);
	g_free(sortbuf);
}

static void interpolate_lines(struct fpi_line_asmbl_ctx *ctx,
			      GSList *line1, float y1, GSList *line2,
			      float y2, unsigned char *output, float yi, int size)
{
	int i;
	unsigned char p1, p2;

	if (!line1 || !line2)
		return;

	for (i = 0; i < size; i++) {
		p1 = ctx->get_pixel(ctx, line1, i);
		p2 = ctx->get_pixel(ctx, line2, i);
		output[i] = (float)p1
			    + (yi - y1)/(y2 - y1)*(p2 - p1);
	}
}

static int min(int a, int b) {return (a < b) ? a : b; }

/* Rescale image to account for variable swiping speed */
struct fp_img *fpi_assemble_lines(struct fpi_line_asmbl_ctx *ctx,
				  GSList *lines, size_t lines_len)
{
	/* Number of output lines per distance between two scanners */
	int i;
	GSList *row1, *row2;
	float y = 0.0;
	int line_ind = 0;
	int *offsets = (int *)g_malloc0((lines_len / 2) * sizeof(int));
	unsigned char *output = g_malloc0(ctx->line_width * ctx->max_height);
	struct fp_img *img;

	fp_dbg("%llu", g_get_real_time());

	row1 = lines;
	for (i = 0; (i < lines_len - 1) && row1; i += 2) {
		int bestmatch = i;
		int bestdiff = 0;
		int j, firstrow, lastrow;

		firstrow = i + 1;
		lastrow = min(i + ctx->max_search_offset, lines_len - 1);

		row2 = g_slist_next(row1);
		for (j = firstrow; j <= lastrow; j++) {
			int diff = ctx->get_deviation(ctx,
					row1,
					row2);
			if ((j == firstrow) || (diff < bestdiff)) {
				bestdiff = diff;
				bestmatch = j;
			}
			row2 = g_slist_next(row2);
		}
		offsets[i / 2] = bestmatch - i;
		fp_dbg("%d", offsets[i / 2]);
		row1 = g_slist_next(row1);
		if (row1)
			row1 = g_slist_next(row1);
	}

	median_filter(offsets, (lines_len / 2) - 1, ctx->median_filter_size);

	fp_dbg("offsets_filtered: %llu", g_get_real_time());
	for (i = 0; i <= (lines_len / 2) - 1; i++)
		fp_dbg("%d", offsets[i]);
	row1 = lines;
	for (i = 0; i < lines_len - 1; i++, row1 = g_slist_next(row1)) {
		int offset = offsets[i/2];
		if (offset > 0) {
			float ynext = y + (float)ctx->resolution / offset;
			while (line_ind < ynext) {
				if (line_ind > ctx->max_height - 1)
					goto out;
				interpolate_lines(ctx,
					row1, y,
					g_slist_next(row1),
					ynext,
					output + line_ind * ctx->line_width,
					line_ind,
					ctx->line_width);
				line_ind++;
			}
			y = ynext;
		}
	}
out:
	img = fpi_img_new(ctx->line_width * line_ind);
	img->height = line_ind;
	img->width = ctx->line_width;
	img->flags = FP_IMG_V_FLIPPED;
	g_memmove(img->data, output, ctx->line_width * line_ind);
	g_free(offsets);
	g_free(output);
	return img;
}
