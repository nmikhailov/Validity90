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

void UTILS_TLS_PRF_TEST1();
void UTILS_TLS_PRF_TEST2();
void UTILS_TLS_PRF_TEST3();
void UTILS_TLS_PRF_TEST4();
void UTILS_TLS_PRF_TEST5();
void UTILS_TLS_PRF_TEST6();

void RSP6_VALID97();
void RSP6_VALID94();
void RSP6_VALID81();
void RSP6_FAIL97();

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

    // UTILS
    g_test_add_func("/utils/tls_prf/test1", UTILS_TLS_PRF_TEST1);
    g_test_add_func("/utils/tls_prf/test2", UTILS_TLS_PRF_TEST2);
    g_test_add_func("/utils/tls_prf/test3", UTILS_TLS_PRF_TEST3);
    g_test_add_func("/utils/tls_prf/test4", UTILS_TLS_PRF_TEST4);
    g_test_add_func("/utils/tls_prf/test5", UTILS_TLS_PRF_TEST5);
    g_test_add_func("/utils/tls_prf/test6", UTILS_TLS_PRF_TEST6);

    // RSP6
    g_test_add_func("/rsp6/valid97", RSP6_VALID97);
//    g_test_add_func("/rsp6/valid94", RSP6_VALID94);
//    g_test_add_func("/rsp6/valid81", RSP6_VALID81);
    g_test_add_func("/rsp6/fail_serial97", RSP6_FAIL97);

    return g_test_run();
}
