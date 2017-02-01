/*
 * Validity VFS0090 driver for libfprint
 * Copyright (C) 2017 Nikita Mikhailov <nikita.s.mikhailov@gmail.com>
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

#define FP_COMPONENT "vfs0090"

#include <errno.h>
#include <string.h>
#include <fp_internal.h>
#include <assembling.h>
#include "driver_ids.h"

#include "vfs0090.h"

static int dev_activate(struct fp_img_dev *idev, enum fp_imgdev_state state)
{
	return -1;
}

static void dev_deactivate(struct fp_img_dev *idev)
{
}

static int dev_open(struct fp_img_dev *idev, unsigned long driver_data)
{
	return -1;
}

static void dev_close(struct fp_img_dev *idev)
{
}

/* Usb id table of device */
static const struct usb_id id_table[] = {
	{.vendor = 0x138a,.product = 0x0090},
	{0, 0, 0,},
};

/* Device driver definition */
struct fp_img_driver vfs0090_driver = {
	/* Driver specification */
	.driver = {
	   .id = VFS0090_ID,
	   .name = FP_COMPONENT,
	   .full_name = "Validity VFS0090",
	   .id_table = id_table,
	   .scan_type = FP_SCAN_TYPE_SWIPE,
	},

	/* Image specification */
	.flags = 0,
	.img_width = 0,
	.img_height = 0,
	.bz3_threshold = 0,

	/* Routine specification */
	.open = dev_open,
	.close = dev_close,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};
