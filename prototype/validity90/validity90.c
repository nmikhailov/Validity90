#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <gcrypt.h>
#include <gnutls/gnutls.h>

#include "utils.h"
#include "validity90.h"

void print_array_(byte* data, int len) {
    for (int i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) {
                printf("\n");
            }
            printf("%04x ", i);
        } else if ((i % 8) == 0) {
            printf(" ");
        }
        printf("%02x ", data[i]);
    }
    puts("");
}

byte_array* byte_array_create_from_data(byte* data, uint32_t len) {
    byte_array* res = byte_array_create(len);
    memcpy(res->data, data, len);
    return res;
}

byte_array* byte_array_create(uint32_t len) {
    return malloc(sizeof(byte_array) + len);
}

void byte_array_free(byte_array* array) {
    free(array);
}

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

guint validity90_tls_prf_raw(const guint8 *secret, const gsize secret_len, const guint8 *seed, const gsize seed_len, const gsize required_len, guint *out_buff) {
    guint res = 0;
    gsize written_bytes = 0;
    guint8 *hmac_data = gcry_xmalloc_secure(seed_len + 0x20);

    gcry_buffer_t hmac_buffs[2] = {
        {.size = secret_len, .off = 0, .len = secret_len, .data = secret},
        {.size = seed_len + 0x20, .off = 0x20, .len = seed_len, .data = hmac_data},
    };
    memcpy(hmac_data + 0x20, seed, seed_len);

    if (required_len % 0x20 != 0) {
        err = -2;
        goto err;
    }

    while (written_bytes < required_len) {
        // Gen A[i]
        if (!gcry_md_hash_buffers(GCRY_MD_SHA256, GCRY_MD_FLAG_HMAC, hmac_data, buffs, 2)) {
            res = -1;
            goto err;
        }

        // Configure buff
        hmac_buffs[1].off = 0;
        hmac_buffs[1].len = seed_len + 0x20;

        if (!gcry_md_hash_buffers(GCRY_MD_SHA256, GCRY_MD_FLAG_HMAC, out_buff + written_bytes, buffs, 2)) {
            res = -1;
            goto err;
        }
        written_bytes += 0x20;

        // Configure A
        hmac_buffs[1].off = 0;
        hmac_buffs[1].len = 0x20;
    };

err:
    gcry_free(hmac_data);

    return res;
}

guint validity90_tls_prf(const guint8 *secret, const gsize secret_len, const char *label, const guint8 *seed, const gsize seed_len, const gsize required_len, guint *out_buff) {
    gsize label_len = strlen(label);
    guint8 *label_seed = gcry_xmalloc_secure(label_len + seed_len);

    memcpy(label_seed, str, label_len);
    memcpy(label_seed + label_len, seed, seed_len);

    gsize label_seed_len = label_len + seed_len;

    guint8 res = validity90_tls_prf_raw(secret, secret_len, label_seed, label_seed_len, required_len, out_buff);

    gcry_free(label_seed);

    return res;
}

enum rsp6_error {
    ERR_RSP6_INVALID_SIZE = -1,
};

typedef struct rsp6_info {
    GByteArray *tls_cert_raw;
    gcry_sexp_t *tls_server_pubkey;
    gcry_sexp_t *tls_client_privkey;
} rsp6_data;

int v90_parse_rsp6(GByteArray *rsp6_data, GByteArray *serial_number, rsp6_info **info_out) {
    if (!rsp6_data || rsp6_data->len < 8) {
        return ERR_RSP6_INVALID_SIZE;
    }

    gcry_md_hd_t md_handle;
    gsize pos = 8; // Skip first 8 bytes - unkown header
    bstream *stream = bstream_create(rsp6_data->data, rsp6_data->len);
    rsp6_info *info = g_malloc(sizeof(rsp6_info));
    info->tls_cert_raw = g_byte_array_new();
    *info_out = info;

    // Derive enc key


    while (bstream_remaining(stream) > 0) {
        // Header
        guint16 type, size;
        guint8 hash[0x20], calc_hash[0x20];
        guint8 *data;

        // Read header
        if (!bstream_read_uint16(stream, &type) ||
            !bstream_read_uint16(stream, &size) ||
            !bstream_read_bytes(stream, 0x20, &hash)) {
            goto err;
        }

        if (size == 0xFFFF && type == 0xFFFF) { // End parsing
            break;
        }

        if (!bstream_read_bytes(stream, size, &data)) {
            goto err;
        }

        // Check hash
        if (!gcry_md_hash_buffer(GCRY_MD_SHA256, &calc_hash, data, size)) {
            goto err;
        }

        if (!memcmp(calc_hash, hash)) {
            goto err;
        }

        if (type == 0x0003) { // TLS Cert
            g_byte_array_append(info->tls_cert_raw, info->tls_cert_raw, size);
            //
        } else if (type == 0x0004) { // ECDSA private key
            //
        }

        switch (header->type) {
        case 0x0001:
            break;
        case 0x0002:
            break;
        case 0x0003:
        case 0x0004:
        case 0x0006:
            puts("");
            printf("Packet %d, size: %x\n", header->type, header->length);
            print_array_(data->data + index + sizeof(rsp6_record_header), header->length);
            puts("");
            break;
        case 0x0005:
            break;
        case 0xFFFF:
            return 0;
            break;
        default:
            break;
        }
        index += sizeof(rsp6_record_header) + header->length;
    }
    return 0;

err:
    bstream_free(stream);
    g_free(*info);
    *info = NULL;

    return -1;
}

int validity90_get_ceritficate(validity90* ctx, byte_array ** cert);

int validity90_get_driver_ecdsa_private_key(validity90* ctx, byte_array ** key);

int validity90_get_device_ecdh_public_key(validity90* ctx, byte_array ** key);

