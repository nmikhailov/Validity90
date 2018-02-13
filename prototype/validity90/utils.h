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

#ifndef UTILS_H
#define UTILS_H

#include <glib.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define VALIDITY90_UTILS_ERROR validity90_utils_error_quark()

GQuark validity90_utils_error_quark(void);

/*
 * Binary stream tools
 */
typedef struct bstream bstream;

bstream *bstream_create(const guint8 *data, const gsize data_size);
void bstream_free(bstream *stream);

gsize bstream_remaining(bstream *stream);

void bstream_set_pos(bstream *stream, gsize pos);
gsize bstream_get_pos(bstream *stream);

gboolean bstream_read_uint8(bstream *stream, guint8 *res);
gboolean bstream_read_uint16(bstream *stream, guint16 *res);
gboolean bstream_read_bytes(bstream *stream, gsize size, guint8 **res);

/*
 * Crypto helpers
 */
enum validity90_crypto_error_s {
    VALIDITY90_ERROR_CODE_AES_CIPHER_FAILED,
    VALIDITY90_ERROR_CODE_AES_DECRYPTION_FAILED,
    VALIDITY90_ERROR_CODE_AES_PADDING_FAILED,
};

gboolean validity90_check_aes_padding(const guint8 *data, const gsize data_len, gsize *real_len);

gboolean validity90_aes_decrypt(const guint8 *data, const gsize data_len, const guint8 *key, const gsize key_len,
                                GByteArray **out_data, GError **error);

gboolean validity90_tls_prf_raw(const guint8 *secret, const gsize secret_len, const guint8 *seed, const gsize seed_len,
                                const gsize required_len, guint8 *out_buff, GError **error);

gboolean validity90_tls_prf(const guint8 *secret, const gsize secret_len, const char *label, const guint8 *seed, const gsize seed_len,
                         const gsize required_len, guint8 *out_buff, GError **error);

/*
 * Misc.
 */
void reverse_mem(guint8* data, gsize size);

void print_array_(const guint8* data, gsize len);

#if defined (__cplusplus)
}
#endif

#endif // UTILS_H
