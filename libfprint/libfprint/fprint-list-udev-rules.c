/*
 * Copyright (C) 2009 Red Hat <mjg@redhat.com> 
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2008 Timo Hoenig <thoenig@suse.de>, <thoenig@nouse.net>
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

#include <config.h>
#include <stdio.h>

#include "fp_internal.h"

static const struct usb_id whitelist_id_table[] = {
    { .vendor = 0x08ff, .product = 0x2810 },
    /* https://bugzilla.redhat.com/show_bug.cgi?id=1173367 */
    { .vendor = 0x138a, .product = 0x0017 },
    { 0, 0, 0, },
};

static const struct usb_id blacklist_id_table[] = {
    { .vendor = 0x0483, .product = 0x2016 },
    /* https://bugs.freedesktop.org/show_bug.cgi?id=66659 */
    { .vendor = 0x045e, .product = 0x00bb },
    { 0, 0, 0 },
};

struct fp_driver whitelist = {
    .id_table = whitelist_id_table,
    .full_name = "Hardcoded whitelist"
};

GHashTable *printed = NULL;

static void print_driver (struct fp_driver *driver)
{
    int i, j, blacklist, num_printed;

    num_printed = 0;

    for (i = 0; driver->id_table[i].vendor != 0; i++) {
        char *key;

	blacklist = 0;
	for (j = 0; blacklist_id_table[j].vendor != 0; j++) {
	    if (driver->id_table[i].vendor == blacklist_id_table[j].vendor &&
		driver->id_table[i].product == blacklist_id_table[j].product) {
		blacklist = 1;
		break;
	    }
	}
	if (blacklist)
	    continue;

	key = g_strdup_printf ("%04x:%04x", driver->id_table[i].vendor, driver->id_table[i].product);

	if (g_hash_table_lookup (printed, key) != NULL) {
	    g_free (key);
	    continue;
	}

	g_hash_table_insert (printed, key, GINT_TO_POINTER (1));

	if (num_printed == 0)
	    printf ("# %s\n", driver->full_name);

	printf ("SUBSYSTEM==\"usb\", ATTRS{idVendor}==\"%04x\", ATTRS{idProduct}==\"%04x\", ATTRS{dev}==\"*\", TEST==\"power/control\", ATTR{power/control}=\"auto\"\n", driver->id_table[i].vendor, driver->id_table[i].product);
	printf ("SUBSYSTEM==\"usb\", ATTRS{idVendor}==\"%04x\", ATTRS{idProduct}==\"%04x\", ENV{LIBFPRINT_DRIVER}=\"%s\"\n", driver->id_table[i].vendor, driver->id_table[i].product, driver->full_name);
	num_printed++;
    }

    if (num_printed > 0)
        printf ("\n");
}

int main (int argc, char **argv)
{
    struct fp_driver **list;
    guint i;

    list = fprint_get_drivers ();

    printed = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    for (i = 0; list[i] != NULL; i++) {
	print_driver (list[i]);
    }

    print_driver (&whitelist);

    g_hash_table_destroy (printed);

    return 0;
}
