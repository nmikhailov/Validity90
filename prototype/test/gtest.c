/*
 * Validity90 tests
 * Copyright (C) 2017-2018 Nikita Mikhailov <nikita.s.mikhailov@gmail.com>
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

#include <glib.h>
#include <gcrypt.h>
#include <locale.h>

// @TEST_INSERT_DEFINITION@

int main(int argc, char *argv[]) {
    setlocale (LC_ALL, "");

    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

    /* Version check should be the very first call because it
    makes sure that important subsystems are initialized. */
    if (!gcry_check_version (GCRYPT_VERSION)) {
        fputs ("libgcrypt version mismatch\n", stderr);
        exit (2);
    }
    /* Disable secure memory. */
    gcry_control (GCRYCTL_DISABLE_SECMEM, 0);
    /* Tell Libgcrypt that initialization has completed. */
    gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);

    // @TEST_INSERT_DECLARATION@

    return g_test_run();
}
