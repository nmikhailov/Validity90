/*
 * Validity90 Packet operations
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

#ifndef VALIDITY90_H
#define VALIDITY90_H

#include <stdint.h>
#include <glib.h>
#include <gcrypt.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define VALIDITY90_RSP6_ERROR validity90_rsp6_error_quark()

GQuark validity90_rsp6_error_quark(void);

typedef struct rsp6_info {
    GByteArray *tls_cert_raw;
    gcry_sexp_t *tls_server_pubkey;
    gcry_sexp_t *tls_client_privkey;
} rsp6_info, *rsp6_info_ptr;

enum validity_error_codes {
    RSP6_ERR_INVALID_LENGTH = 0,
    RSP6_ERR_HASH_MISSMATCH = 1,
};

gboolean validity90_parse_rsp6(const guint8 *data, gsize data_len, const guint8 *serial, gsize serial_len, rsp6_info_ptr *info_out, GError **err);

/*

typedef struct validity90 validity90;

validity90* validity90_create();

void validity90_free(validity90 * ctx);
*/

/*int validity90_parse_rsp6(validity90* ctx, byte_array *data);

int validity90_get_ceritficate(validity90* ctx, byte_array ** cert);

int validity90_get_driver_ecdsa_private_key(validity90* ctx, byte_array ** key);

int validity90_get_device_ecdh_public_key(validity90* ctx, byte_array ** key);
*/

#if defined (__cplusplus)
}
#endif

#endif // VALIDITY90_H
