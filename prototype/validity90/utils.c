/*
 * Validity90 Utility functions
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

#include <string.h>
#include <stdio.h>

#include <gcrypt.h>

#include "utils.h"

GQuark validity90_utils_error_quark (void) {
  return g_quark_from_static_string ("validity-utils-error-quark");
}

struct bstream {
    gsize pos;
    gsize data_size;
    guint8 *data;
};

bstream *bstream_create(const guint8 *data, const gsize data_size) {
    bstream *res = g_malloc(sizeof(bstream));

    res->pos = 0;
    res->data = g_malloc(data_size);
    res->data_size = data_size;

    memcpy(res->data, data, data_size);

    return res;
}

void bstream_free(bstream *stream) {
    if (stream && stream->data) {
        g_free(stream->data);
    }
    g_free(stream);
}

gsize bstream_remaining(bstream *stream) {
    return stream->data_size - stream->pos;
}

bstream_result bstream_read_uint8(bstream *stream, guint8 *res) {
    if (stream->pos + 1 < stream->data_size) {
        *res = stream->data[stream->pos++];
        return BSTREAM_OK;
    }
    return BSTREAM_ERR_NO_BYTES_AVAILABLE;
}

bstream_result bstream_read_uint16(bstream *stream, guint16 *res) {
    if (stream->pos + 2 < stream->data_size) {
        *res = stream->data[stream->pos] | (stream->data[stream->pos] << 8);
        stream->pos += 2;
        return BSTREAM_OK;
    }
    return BSTREAM_ERR_NO_BYTES_AVAILABLE;
}

bstream_result bstream_read_bytes(bstream *stream, gsize size, guint8 **res) {
    if (stream->pos + size < stream->data_size) {
        *res = g_malloc(size);
        memcpy(*res, stream->data + stream->pos, size);
        stream->pos += size;
        return BSTREAM_OK;
    }
    return BSTREAM_ERR_NO_BYTES_AVAILABLE;
}

/*
 * TLS
 */

gboolean validity90_check_aes_padding(const guint8 *data, const gsize data_len, gsize *real_len) {
    guint8 pad_size = data[data_len - 1];

    real_len = data_len - pad_size;

    for (int i = 0; i < pad_size; i++) {
        if (data[len - 1 - i] != pad_size) {
            return FALSE;
        }
    }
    return TRUE;
}

gboolean validity90_aes_decrypt(const guint8 *data, const gsize data_len, const guint8 **out_data, guint8 *out_len, GError **error) {
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    gcry_cipher_hd_t cipher = NULL;
    gcry_error_t res = 0;
    if ((res = gcry_cipher_open(&cipher, GCRY_CIPHER_AES256, GCRY_CIPHER_MODE_CBC, 0)) != 0) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, 0, "AES Decrypt: Cipher open failed, ret: %lx", res);
        goto err;
    }
    if ((res = gcry_cipher_setkey(cipher, master_key_aes, 0x20)) != 0) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, 0, "AES Decrypt: Cipher setkey failed, ret: %lx", res);
        goto err;
    }
    if ((res = gcry_cipher_setiv(cipher, iv, 0x10)) != 0) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, 0, "AES Decrypt: Cipher setiv failed, ret: %lx", res);
        goto err;
    }
    if ((res = gcry_cipher_decrypt(cipher, NULL, 0, data, data_len)) != 0) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, 0, "AES Decrypt: Decryption failed, ret: %lx", res);
        goto err;
    }
    gcry_cipher_close(cipher);
    cipher = NULL;

    *out_len = 0;
    if (!validity90_check_aes_padding(data, data_len, out_len)) {
        g_set_error(error, VALIDITY90_RSP6_ERROR, 0, "AES Decrypt: Decryption failed, inconsistent padding");
        goto err;
    }

    out_data = g_malloc(*out_len);
    memcpy(out_data, data, *out_len);

    return TRUE;

err:
    if (cipher != NULL) {
        gcry_cipher_close(cipher);
    }

    return FALSE;
}

gboolean validity90_tls_prf_raw(const guint8 *secret, const gsize secret_len, const guint8 *seed, const gsize seed_len,
                                const gsize required_len, guint8 *out_buff, GError **error) {
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    gsize written_bytes = 0;
    guint8 iteration_buff[0x20];
    guint8 *hmac_data = gcry_xmalloc_secure(seed_len + 0x20);

    gcry_buffer_t hmac_buffs[2] = {
        {.size = secret_len, .off = 0, .len = secret_len, .data = (guint8*) secret},
        {.size = seed_len + 0x20, .off = 0x20, .len = seed_len, .data = hmac_data},
    };
    memcpy(hmac_data + 0x20, seed, seed_len);

    while (written_bytes < required_len) {
        // Gen A[i]
        gpg_error_t res = 0;
        if ((res = gcry_md_hash_buffers(GCRY_MD_SHA256, GCRY_MD_FLAG_HMAC, hmac_data, hmac_buffs, 2)) != 0) {
            g_set_error(error, VALIDITY90_UTILS_ERROR, 0, "TLS_PRF_RAW: gen A[i] hash failed, cause: 0x%x", res);
            goto err;
        }

        // Configure buff
        hmac_buffs[1].off = 0;
        hmac_buffs[1].len = seed_len + 0x20;

        if ((res = gcry_md_hash_buffers(GCRY_MD_SHA256, GCRY_MD_FLAG_HMAC, iteration_buff, hmac_buffs, 2)) != 0) {
            g_set_error(error, VALIDITY90_UTILS_ERROR, 1, "TLS_PRF_RAW: hash failed, cause: 0x%x", res);
            goto err;
        }
        memcpy(out_buff + written_bytes, iteration_buff, MIN(0x20, required_len - written_bytes));
        written_bytes += 0x20;

        // Configure A
        hmac_buffs[1].off = 0;
        hmac_buffs[1].len = 0x20;
    };

    gcry_free(hmac_data);
    return TRUE;

err:
    gcry_free(hmac_data);
    return FALSE;
}

gboolean validity90_tls_prf(const guint8 *secret, const gsize secret_len, const char *label, const guint8 *seed, const gsize seed_len,
                         const gsize required_len, guint8 *out_buff, GError **error) {
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    gsize label_len = strlen(label);
    guint8 *label_seed = gcry_xmalloc_secure(label_len + seed_len);

    memcpy(label_seed, label, label_len);
    memcpy(label_seed + label_len, seed, seed_len);

    gsize label_seed_len = label_len + seed_len;

    gboolean res = validity90_tls_prf_raw(secret, secret_len, label_seed, label_seed_len, required_len, out_buff, error);

    gcry_free(label_seed);

    return res;
}
