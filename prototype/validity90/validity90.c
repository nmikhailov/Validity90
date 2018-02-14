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

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <gcrypt.h>
#include <gnutls/gnutls.h>

#include "utils.h"
#include "validity90.h"

GQuark validity90_rsp6_error_quark (void) {
  return g_quark_from_static_string ("validity-rsp6-error-quark");
}

typedef enum rsp6_record_type {
    RSP6_TLS_CERT = 0x0003,
    RSP6_ECDSA_PRIV_ENCRYPTED = 0x0004,
    RSP6_ECDH_PUB = 0x0006,

    RSP6_UNKNOWN_0 = 0x0000,
    RSP6_UNKNOWN_1 = 0x0001,
    RSP6_UNKNOWN_2 = 0x0002,
    RSP6_UNKNOWN_5 = 0x0005,

    RSP6_END = 0xFFFF,
} rsp6_record_type;

gboolean validity90_handle_rsp6_ecdsa_packet(const guint8 *data, gsize data_len,
                                             const guint8 *serial, gsize serial_len, GByteArray** d_mpi_component, GError **error) {
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    gboolean result = TRUE;

    if (data_len < 0x80) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, 0, "RSP6: ecdsa packet length too small: %lx", data_len);
        result = FALSE;
        goto end;
    }

    // Get AES master key
    guint8 factory_key[] = {
        0x71, 0x7c, 0xd7, 0x2d, 0x09, 0x62, 0xbc, 0x4a, 0x28, 0x46, 0x13, 0x8d, 0xbb, 0x2c, 0x24, 0x19,
        0x25, 0x12, 0xa7, 0x64, 0x07, 0x06, 0x5f, 0x38, 0x38, 0x46, 0x13, 0x9d, 0x4b, 0xec, 0x20, 0x33,
    };
    guint8 master_key_aes[0x20];

    if (!validity90_tls_prf(factory_key, G_N_ELEMENTS(factory_key), "GWK", serial, serial_len, 0x20, master_key_aes, error)) {
        result = FALSE;
        goto end;
    }

    // Decrypt
    GByteArray *ecdsa_key = NULL;

    g_assert_cmphex(data[0], ==, 0x02);
    if (data[0] != 0x02) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, 0, "RSP6: ecdsa packet invalid prefix format");
        result = FALSE;
        goto end;
    }

    if (!validity90_aes_decrypt(data + 1, 0x80, master_key_aes, 0x20, &ecdsa_key, error)) {
        result = FALSE;
        goto end;
    }

    // Reverse parts
    // X, Y components
    reverse_mem(ecdsa_key->data, 0x20);
    reverse_mem(ecdsa_key->data + 0x20, 0x20);

    // d component
    reverse_mem(ecdsa_key->data + 0x40, 0x20);

    // Assign key
    (*d_mpi_component) = g_byte_array_sized_new(0x20);
    g_byte_array_append(*d_mpi_component, ecdsa_key->data + 0x40, 0x20);

end:
    g_clear_pointer(&ecdsa_key, g_byte_array_free_to_bytes);

    return result;
}

gboolean validity90_handle_rsp6_pubkey_packet(const guint8 *data, gsize data_len, GByteArray **q_component, GError **error) {
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    gboolean result = TRUE;

    if (data_len < 0x6c) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, 0, "RSP6: pubkey packet length too small: %lx", data_len);
        result = FALSE;
        goto end;
    }

    *q_component = g_byte_array_sized_new(0x40);
    // X component
    g_byte_array_append(*q_component, data + 0x08, 0x20);
    reverse_mem((*q_component)->data, 0x20);

    // Y component
    g_byte_array_append(*q_component, data + 0x4c, 0x20);
    reverse_mem((*q_component)->data + 0x20, 0x20);

end:
    if (!result) {
        g_clear_pointer(q_component, g_byte_array_free_to_bytes);
    }

    return result;
}

gboolean validity90_parse_rsp6(const guint8 *data, gsize data_len, const guint8 *serial, gsize serial_len, rsp6_info_ptr *info_out, GError **error) {
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    gboolean result = TRUE;

    bstream *stream = bstream_create(data, data_len);

    rsp6_info *info = g_malloc(sizeof(rsp6_info));
    *info_out = info;
    GByteArray *ecdsa_d = NULL;
    GByteArray *ecdsa_q = NULL;
    GByteArray *ecdh = NULL;

    if (data_len < 8) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, RSP6_ERR_INVALID_LENGTH, "RSP6 length is <8 (%lx)", data_len);
        result = FALSE;
        goto end;
    }

    // Skip header
    bstream_set_pos(stream, 8);

    // Derive enc key
    while (bstream_remaining(stream) > 0) {
        // Header
        guint16 type;
        guint16 size;
        guint8 *hash;
        guint8 calc_hash[0x20];
        guint8 *data;

        // Read header
        if (!bstream_read_uint16(stream, &type) ||
            !bstream_read_uint16(stream, &size) ||
            !bstream_read_bytes(stream, 0x20, &hash)) {

            g_set_error(error, VALIDITY90_RSP6_ERROR, RSP6_ERR_INVALID_LENGTH, "RSP6 can't read packet header");
            result = FALSE;
            goto end;
        }

        // Stop parsing
        if (type == RSP6_END) {
            break;
        }

        if (!bstream_read_bytes(stream, size, &data)) {
            g_set_error(error, VALIDITY90_RSP6_ERROR, RSP6_ERR_INVALID_LENGTH, "RSP6 can't read packet data");
            result = FALSE;
            goto end;
        }

        // Check hash
        gcry_md_hash_buffer(GCRY_MD_SHA256, calc_hash, data, size);

        if (memcmp(calc_hash, hash, 0x20) != 0) {
            g_set_error(error, VALIDITY90_RSP6_ERROR, RSP6_ERR_HASH_MISSMATCH, "RSP6 hass missmatch for packet %x", type);
            result = FALSE;
            goto end;
        }

        switch (type) {
        case RSP6_TLS_CERT:
            if (!validity90_handle_rsp6_pubkey_packet(data, size, &ecdsa_q, error)) {
                result = FALSE;
                goto end;
            }
            info->tls_cert_raw = g_byte_array_new();
            g_byte_array_append(info->tls_cert_raw, data, size);

            break;

        case RSP6_ECDSA_PRIV_ENCRYPTED:
            if (!validity90_handle_rsp6_ecdsa_packet(data, size, serial, serial_len, &ecdsa_d, error)) {
                result = FALSE;
                goto end;
            }
            break;

        case RSP6_ECDH_PUB:
            if (!validity90_handle_rsp6_pubkey_packet(data, size, &ecdh, error)) {
                result = FALSE;
                goto end;
            }
            break;

        case RSP6_UNKNOWN_0:
        case RSP6_UNKNOWN_1:
        case RSP6_UNKNOWN_2:
        case RSP6_UNKNOWN_5:
            break;
        default:
            g_debug("RSP6: Unknown tag %x", type);
            break;
        }
    }

    if (ecdsa_d == NULL || ecdsa_q == NULL) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, RSP6_ERR_NO_ECDSA_COMPONENTS,
                    "RSP6 missing ecdsa components, priv: %d, pub: %d", ecdsa_d != NULL, ecdsa_q != NULL);
        result = FALSE;
        goto end;
    }

    if (ecdh == NULL) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, RSP6_ERR_NO_ECDH_COMPONENT, "RSP6 missing ecdh component");
        result = FALSE;
        goto end;
    }

    // Set ECDSA private key
    info->tls_client_privkey = g_byte_array_new();
    g_byte_array_append(info->tls_client_privkey, ecdsa_q->data, ecdsa_q->len);
    g_byte_array_append(info->tls_client_privkey, ecdsa_d->data, ecdsa_d->len);

    // Set ECDH pub key
    info->tls_server_pubkey = g_byte_array_new();
    g_byte_array_append(info->tls_server_pubkey, ecdh->data, ecdh->len);

end:
    if (!result) {
        g_clear_pointer(&info->tls_cert_raw, g_byte_array_free_to_bytes);
        g_clear_pointer(&info->tls_client_privkey, g_byte_array_free_to_bytes);
        g_clear_pointer(&info->tls_server_pubkey, g_byte_array_free_to_bytes);
        g_clear_pointer(&info, g_free);
    }
    bstream_free(stream);
    g_clear_pointer(&ecdsa_d, g_byte_array_free_to_bytes);
    g_clear_pointer(&ecdsa_q, g_byte_array_free_to_bytes);

    return result;
}

/*
struct validity90 {
    byte_array* tls_driver_ecdsa_private_key;
    byte_array* tls_device_ecdh_public_key;
    byte_array* tls_certificate;
};

typedef struct __attribute__((__packed__)) rsp6_record_header {
    uint16_t type;
    uint16_t length;
    byte hash[0x20];
} rsp6_record_header;

validity90* validity90_create() {
    validity90* ctx = calloc(1, sizeof(validity90));
    return ctx;
}

void validity90_free(validity90 * ctx) {
    free(ctx->tls_driver_ecdsa_private_key);
    free(ctx->tls_device_ecdh_public_key);
    free(ctx->tls_certificate);

    free(ctx);
}


int validity90_get_ceritficate(validity90* ctx, byte_array ** cert);

int validity90_get_driver_ecdsa_private_key(validity90* ctx, byte_array ** key);

int validity90_get_device_ecdh_public_key(validity90* ctx, byte_array ** key);

*/
