/*
    Warning: extremly ugly code
*/

/*
 * Validity90 Prototype
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

#include <stdio.h>
#include <stdbool.h>
#include <libusb.h>
#include <stdlib.h>
#include <string.h>
#include <nss.h>
#include <keyhi.h>
#include <keythi.h>
#include <secoid.h>
#include <secmodt.h>
#include <sechash.h>
#include <pk11pub.h>
#include <err.h>
#include <errno.h>

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/aes.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <openssl/ecdh.h>
#include <png.h>

#include "constants.h"
#include "validity90/validity90.h"
#include "validity90/utils.h"


#define xstr(a) str(a)
#define str(a) #a

#define max(a,b) (a > b ? a : b)
#define min(a,b) (a > b ? b : a)

#define err(x) res_err(x, xstr(x))
#define errb(x) res_errb(x, xstr(x))
#define errssl(x) openssl_res_err(x, xstr(x))
#define byte guint8

typedef struct DeviceInfo {
    guint16 vid;
    guint16 pid;

    guint hasLed;
    guint hasBios;
    guint requiresReset;
    guint hasRawOutput;
    gboolean unsupported;

    gchar *description;
} DeviceInfo;

DeviceInfo all_devices[] = {
    { .vid = 0x138a, .pid = 0x0090, .hasLed = 1, .hasBios = 1, .requiresReset = 0, .hasRawOutput = 1 },
    { .vid = 0x138a, .pid = 0x0097, .hasLed = 1, .hasBios = 1, .requiresReset = 0, .hasRawOutput = 0 },
    { .vid = 0x138a, .pid = 0x0094, .hasLed = 0, .hasBios = 0, .requiresReset = 1, .hasRawOutput = 1, .unsupported = 1, .description = "Support would be available soon" },
    { .vid = 0x06cb, .pid = 0x0081, .hasLed = -1, .hasBios = -1, .requiresReset = 1, .hasRawOutput = -1, .unsupported = 1, .description = "Support would be available soon" },
    { .vid = 0x06cb, .pid = 0x009a, .hasLed = 1, .hasBios = -1, .requiresReset = 0, .hasRawOutput = -1 },
    { .vid = 0x138a, .pid = 0x0091, .unsupported = 1, .description = "Won't be supported, check README" },
};

static libusb_device_handle * dev;

int idProduct = 0;

static const byte client_random[] = {
  0x95, 0x6c, 0x41, 0xa9, 0x12, 0x86, 0x8a, 0xda,
  0x9b, 0xb2, 0x5b, 0xb4, 0xbb, 0xd6, 0x1d, 0xde,
  0x4f, 0xda, 0x23, 0x2a, 0x74, 0x7b, 0x2a, 0x93,
  0xf8, 0xac, 0xc6, 0x69, 0x24, 0x70, 0xc4, 0x2a
};

byte server_random[0x40];

void print_hex_gn(byte* data, int len, int sz) {
    for (int i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) {
                printf("\n");
            }
            printf("%04x ", i);
        } else if ((i % 8) == 0) {
            printf(" ");
        }
        printf("%02x ", data[i * sz]);
    }
    puts("");
}

void print_hex_string(byte* data, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    puts("");
}

void print_hex(byte* data, int len) {
    print_hex_gn(data, len, 1);
}

void print_hex_dw(dword* data, int len) {
    print_hex_gn(data, len, 4);
}

bool compare(byte * data1, int data_len, dword * expected, int exp_len) {
    bool fail = false;

    if (data_len != exp_len) {
        printf("Expected len: %d, but got %d\n", exp_len, data_len);
        fail = true;
    } else {
        for (int i = 0; i < data_len; i++) {
            if (data1[i] != expected[i] && !(expected[i] & MASK_VARIABLE)) {
                printf("Expected at char %03x\n", i);
                fail = true;
                break;
            }
        }
    }

    if (fail) {
        /*puts("Expected:");
        print_hex_dw(expected, exp_len);
        puts("Got:");
        print_hex(data1, data_len);*/
    }

    return data_len == exp_len;
}

void openssl_res_err(int result, char* where) {
    if (result != 0) {
        printf("Failed '%s': %d - %s\n", where, result, ERR_reason_error_string(ERR_get_error()));
        exit(0);
    }
}

void res_err(int result, char* where) {
    if (result != 0) {
        printf("Failed '%s': %d - %s\n", where, result, libusb_error_name(result));
        exit(0);
    }
}

void res_errb(int result, char* where) {
    if (result != 1) {
        printf("Failed '%s': %d - %s\n", where, result, libusb_error_name(result));
        ERR_print_errors_fp(stderr);
        exit(0);
    }
}

void qwrite(byte * data, int len) {
    int send;
    err(libusb_bulk_transfer(dev, 0x01, data, len, &send, 10000));

    puts("usb write:");
    print_hex(data, len);
}

void qread(byte * data, int len, int *out_len) {
    err(libusb_bulk_transfer(dev, 0x81, data, len, out_len, 10000));

    puts("usb read:");
    print_hex(data, *out_len);
}

static byte pubkey1[0x40];
static byte ecdsa_private_key[0x60];

static char masterkey_aes[0x20];

byte system_serial[1024];
int system_serial_len;

void loadBiosData() {
    char name[1024], serial[1024];
    FILE *nameFile, *serialFile;
    if (!(nameFile = fopen("/sys/class/dmi/id/product_name", "r"))) {
        perror("Can't open /sys/class/dmi/id/product_name");
        exit(EXIT_FAILURE);
    }
    if (!(serialFile = fopen("/sys/class/dmi/id/product_serial", "r"))) {
        perror("Can't open /sys/class/dmi/id/product_serial");
        exit(EXIT_FAILURE);
    }

    fscanf(nameFile, "%s", name);
    fscanf(serialFile, "%s", serial);

    int len1 = strlen(name), len2 = strlen(serial);
    memcpy(system_serial, name, len1 + 1);
    memcpy(system_serial + len1 + 1, serial, len2 + 1);
    system_serial_len = len1 + len2 + 2;

    fclose(nameFile);
    fclose(serialFile);
}

bool do_step(byte *data, int data_len, byte *buff, int *buf_len, byte *expected, int expected_len)
{
    qwrite(data, data_len);
    qread(buff, 1024 * 1024, buf_len);

    return compare(buff, *buf_len, expected, expected_len);
}


#define STEP(a,b) do_step(a, sizeof(a) / sizeof(byte), buff, &len, b, sizeof(b) / sizeof(dword));

void init_keys(const byte *buff, int len) {
    byte vbox_serial[] = "VirtualBox\0" "0";

    rsp6_info_ptr info = NULL;
    GError *error = NULL;
    if (!validity90_parse_rsp6(buff, len, vbox_serial, G_N_ELEMENTS(vbox_serial), &info, NULL) &&
            !validity90_parse_rsp6(buff, len, system_serial, system_serial_len, &info, &error)) {
        printf("Failed to handle RSP6: %s\n", error->message);
        exit(-1);
    }

    memcpy(ecdsa_private_key, info->tls_client_privkey->data, info->tls_client_privkey->len);
    memcpy(tls_certificate + 21, info->tls_cert_raw->data, info->tls_cert_raw->len);
    memcpy(pubkey1, info->tls_server_pubkey->data, info->tls_server_pubkey->len);

    g_byte_array_free_to_bytes(info->tls_cert_raw);
    g_byte_array_free_to_bytes(info->tls_client_privkey);
    g_byte_array_free_to_bytes(info->tls_server_pubkey);
    g_free(info);

    fflush(stdout);
}

void init_steps(byte *buff, int len, int *out_len)
{
    puts("step 2");STEP(init_sequence_msg2, init_sequence_rsp2);
    puts("step 3");STEP(init_sequence_msg3, init_sequence_rsp3);
    puts("step 4");STEP(init_sequence_msg4, init_sequence_rsp4);
    puts("step 5");STEP(init_sequence_msg5, init_sequence_rsp5);
    puts("step 6");STEP(init_sequence_msg6, init_sequence_rsp6);
    *out_len = len;
}

void setup() {
    int len;
    byte buff[1024 * 1024];
    byte *keys_buff;

    static const byte setup_sequence_msg13[] = { 0x1a };
    static const dword setup_sequence_rsp13[] = { 0x00, 0x00 };

    puts("step 2");STEP(init_sequence_msg2, init_sequence_rsp2);
    print_hex(buff, len);

    puts("step 4");STEP(init_sequence_msg4, init_sequence_rsp4);
    print_hex(buff, len);

    puts("step 5");STEP(init_sequence_msg5, setup_sequence_rsp5);
    print_hex(buff, len);

    puts("step 6");STEP(setup_sequence_msg6, setup_sequence_rsp6);
    print_hex(buff, len);

    puts("step 7");STEP(setup_sequence_msg7, setup_sequence_rsp7);
    print_hex(buff, len);

    puts("step 8");STEP(setup_sequence_msg8, setup_sequence_rsp8);
    print_hex(buff, len);

    puts("step 9");STEP(setup_sequence_config_data, setup_sequence_config_data_rsp);
    print_hex(buff, len);

    puts("step 5");STEP(init_sequence_msg5, setup_sequence_rsp5);
    print_hex(buff, len);

    puts("step 10");STEP(setup_sequence_msg10, setup_sequence_rsp10);
    print_hex(buff, len);

    puts("step 11");STEP(setup_sequence_msg11, setup_sequence_rsp11);
    print_hex(buff, len);

    puts("step 12");STEP(setup_sequence_msg12, setup_sequence_rsp12);
    print_hex(buff, len);

    puts("step 13");STEP(setup_sequence_msg13, setup_sequence_rsp13);
    print_hex(buff, len);

    // We need to find a way to retrieve ecdsa_private_key, once we've that
    // we're all set and we can start a TLS session...
    puts("Incomplete reverse engineering");
    exit(EXIT_FAILURE);
}

void init() {
    int len;
    byte buff[1024 * 1024];

    puts("step 1");STEP(init_sequence_msg1, init_sequence_rsp1);

    if (getenv("FORCE_RESET") != NULL) {
        puts("Sending reset commands");
        STEP(setup_sequence_config_data, setup_sequence_config_data_rsp);
        const byte reset_cmd[] = "\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                                 "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                                 "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                                 "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                                 "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                                 "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
                                 "\x00\x00";
        qwrite(reset_cmd, sizeof(reset_cmd));
        qread(buff, sizeof(buff), &len);
        print_hex(buff, len);

        puts("ACK");
        STEP(setup_sequence_completed, setup_sequence_completed_rsp);
        print_hex(buff, len);
        exit(EXIT_SUCCESS);
    }

    if (buff[len-1] != 0x07 || getenv("FORCE_SETUP") != NULL) {
        printf("Sensor not initialized, init byte is 0x%x (expected 0x02)\n",
               buff[len-1]);

        setup();
        exit(EXIT_FAILURE);
    } else {
        init_steps(buff, len, &len);
    }

    init_keys(buff, len);
}

#undef STEP

PK11Context* hmac_make_context(byte *key_bytes, int key_len) {
    CK_MECHANISM_TYPE hmacMech = CKM_SHA256_HMAC;
    PK11SlotInfo *slot = PK11_GetBestSlot(hmacMech, NULL);

    SECItem key;

    key.data = key_bytes;
    key.len = key_len;

    PK11SymKey *pkKey = PK11_ImportSymKey(slot, hmacMech, PK11_OriginUnwrap, CKA_SIGN, &key, NULL);

    SECItem param = { .type = siBuffer, .data = NULL, .len = 0 };

    PK11Context* context = PK11_CreateContextBySymKey(hmacMech, CKA_SIGN, pkKey, &param);
    PK11_DigestBegin(context);

    return context;
}

byte* hmac_compute(byte *key, int key_len, byte* data, int data_len) {
    PK11Context* context = hmac_make_context(key, key_len);
    PK11_DigestOp(context, data, data_len);

    byte *res = malloc(0x20);
    int len = 0x20;
    PK11_DigestFinal(context, res, &len, 0x20);
    PK11_DestroyContext(context, PR_TRUE);

    return res;
}

//10
static const byte privkey1[] = {
    0x1d, 0xd8, 0x36, 0x68, 0xe9, 0xb0, 0x7b, 0x93, 0x12, 0x38, 0x31, 0x23, 0x90, 0xc8, 0x87, 0xca,
    0xdb, 0x82, 0x27, 0x39, 0xde, 0x7b, 0x43, 0xd2, 0x23, 0xd7, 0xcd, 0xd1, 0x3c, 0x77, 0x0e, 0xd2,

    0xd1, 0x93, 0x70, 0x02, 0xaf, 0x3b, 0x18, 0x47, 0xc5, 0x30, 0x4c, 0x33, 0x60, 0xcf, 0xbf, 0xc5,
    0x9b, 0x3c, 0x67, 0xd9, 0x45, 0x06, 0x38, 0xda, 0x92, 0xbe, 0x65, 0xbf, 0x81, 0x8c, 0xaa, 0x7e,

    0x20, 0x14, 0x3b, 0x7b, 0x62, 0x64, 0x90, 0x07, 0x54, 0x4e, 0x7a, 0x98, 0xf9, 0x81, 0xbe, 0xc1,
    0xf2, 0x1f, 0x9a, 0x29, 0x65, 0xb6, 0xcc, 0x29, 0x0c, 0x45, 0xd3, 0x87, 0xae, 0xbf, 0xa4, 0xd9
};
//11
/*
static const byte privkey1[] = {
    0x65, 0xd7, 0xf7, 0x6f, 0xf2, 0x94, 0xe4, 0xe9,  0xd8, 0xae, 0xc5, 0x79, 0x8d, 0x77, 0x3b, 0xb1,
    0xad, 0xd4, 0xe7, 0xf2, 0xbd, 0x09, 0x64, 0xa7,  0xd9, 0x9c, 0xeb, 0x50, 0x33, 0x56, 0xbb, 0x3e,

    0xcb, 0x1c, 0x62, 0xfc, 0x40, 0x60, 0xbf, 0xd2,  0xd8, 0x7b, 0xc9, 0x3f, 0xdc, 0x4c, 0xc7, 0xab,
    0xb3, 0xfe, 0x3a, 0x25, 0x8c, 0x35, 0xa1, 0x2f,  0x8e, 0x67, 0xe3, 0x89, 0xc7, 0x6a, 0x32, 0xf4,

    0xfd, 0x01, 0x93, 0x3c, 0xd8, 0x18, 0x9d, 0x65,  0x9c, 0x41, 0xd3, 0xbe, 0x6e, 0xcb, 0x8b, 0x08,
    0x58, 0x0a, 0xae, 0x80, 0xb4, 0x2d, 0xd0, 0xb5,  0x54, 0x81, 0x89, 0x91, 0xd0, 0x68, 0xb0, 0x26
};*/

EC_KEY * load_key(byte *data, bool is_private) {
    BIGNUM *x = BN_bin2bn(data, 0x20, NULL);
    BIGNUM *y = BN_bin2bn(data + 0x20, 0x20, NULL);
    BIGNUM *d = NULL;
    EC_KEY *key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

    if (!EC_KEY_set_public_key_affine_coordinates(key, x, y)) {
        goto err;
    }

    if (is_private) {
        d = BN_bin2bn(data + 0x40, 0x20, NULL);
        if (!EC_KEY_set_private_key(key, d)) {
            goto err;
        }
    }
    errssl(EC_KEY_check_key(key) - 1);

    goto clean;

err:
    if (key) EC_KEY_free(key);
    key = NULL;
    ERR_print_errors_fp(stderr);

clean:
    if (x) BN_free(x);
    if (y) BN_free(y);
    if (d) BN_free(d);

    return key;
}

EVP_PKEY * load_pkey(byte * data, bool is_private) {
    EC_KEY *key = load_key(data, is_private);

    EVP_PKEY *priv = EVP_PKEY_new();
    if (is_private) {
        EVP_PKEY_set1_EC_KEY(priv, key);
    } else {
        EVP_PKEY_set1_EC_KEY(priv, key);
    }
    return priv;
}

byte* P_Hash(byte * secret, int key_len, byte * seed, int seed_len) {
    byte *a1 = hmac_compute(secret, key_len, seed, seed_len);
    byte *a2 = hmac_compute(secret, key_len, a1, 0x20);

    byte buff1[seed_len + 0x20];
    byte buff2[seed_len + 0x20];

    memcpy(buff1, a1, 0x20);
    memcpy(buff1 + 0x20, seed, seed_len);

    memcpy(buff2, a2, 0x20);
    memcpy(buff2 + 0x20, seed, seed_len);

    byte * val1 = hmac_compute(secret, key_len, buff1, seed_len + 0x20);
    byte * val2 = hmac_compute(secret, key_len, buff2, seed_len + 0x20);

    byte * data = malloc(0x30);
    memcpy(data, val1, 0x20);
    memcpy(data + 0x20, val2, 0x10);

    return data;
}

void print_hex_C(FILE *f, byte* data, int len) {
    fprintf(f, " = {\n");
    for (int i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) {
                fprintf(f, "\n");
            }
            fprintf(f, "    ");
        }
        fprintf(f, "0x%02x, ", data[i]);
    }
    fprintf(f, "\n};\n");
}

byte all_messages[1024 * 1024]; int all_messages_index = 0;
void HUpdate(HASHContext *context, const unsigned char *src, unsigned int len) {
    HASH_Update(context, src, len);
    memcpy(all_messages + all_messages_index, src, len);
    all_messages_index += len;
    // puts("HASHING>>>");
    // print_hex(src, len);
    // puts("HASHING<<");
}

guint8 key_block[0x120];

void mac_then_encrypt(byte type, byte * data, int data_len, byte **res, int *res_len) {
    byte iv[0x10] = {0x4b, 0x77, 0x62, 0xff, 0xa9, 0x03, 0xc1, 0x1e, 0x6f, 0xd8, 0x35, 0x93, 0x17, 0x2d, 0x54, 0xef};

    int prefix_len = 5;
    if (type == 0xFF) {
        prefix_len = 0;
    }
    // header for hmac + data + hmac
    byte all_data[prefix_len + data_len + 0x20];
    all_data[0] = type; all_data[1] = all_data[2] = 0x03; all_data[3] = (data_len >> 8) & 0xFF; all_data[4] = data_len & 0xFF;
    memcpy(all_data + prefix_len, data, data_len);

    memcpy(all_data + prefix_len + data_len, hmac_compute(key_block + 0x00, 0x20, all_data, prefix_len + data_len), 0x20);


    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    EVP_EncryptInit(context, EVP_aes_256_cbc(), key_block + 0x40, iv);
    EVP_CIPHER_CTX_set_padding(context, 0);

    *res_len = ((data_len + 16) / 16) * 16 + 0x30;
    *res = malloc(*res_len);
    memcpy(*res, iv, 0x10);
    int written = 0, wr2, wr3 = 0;

//    puts("To encrypt & mac:");
//    print_hex(data, data_len);

    EVP_EncryptUpdate(context, *res + 0x10, &written, all_data + prefix_len, data_len + 0x20);
//    printf("enc written: %02x\n", written);

    int pad_len = *res_len - (0x30 + data_len);
    if (pad_len == 0) {
        pad_len = 16;
    }
    byte pad[pad_len];
    memset(pad, pad_len - 1, pad_len);

    EVP_EncryptUpdate(context, *res + 0x10 + written, &wr3, pad, pad_len);

    EVP_EncryptFinal(context, *res + 0x10 + written + wr3, &wr2);
//    printf("enc written: %02x\n", wr2);
    *res_len = written + wr2 + wr3 + 0x10;

//    print_hex(all_data + prefix_len, data_len + 0x20);

//    puts("Encrypted& hmac");
//    print_hex(*res, *res_len);

    EVP_CIPHER_CTX_free(context);
}

byte *sign(EVP_PKEY* key, byte *data, int data_len) {
//    ECDSA_do_sign()
    int len5;
    int status;
    byte * res = NULL;
    do {
        free(res);
        puts("signing...");
        EVP_PKEY_CTX *sign_ctx = EVP_PKEY_CTX_new(key, NULL);
        status = EVP_PKEY_sign_init(sign_ctx);
        status = EVP_PKEY_CTX_set_signature_md(sign_ctx, EVP_sha256());

        status = EVP_PKEY_sign(sign_ctx, NULL, &len5, data, data_len);
        res = malloc(len5);
        status = EVP_PKEY_sign(sign_ctx, res, &len5, data, data_len);

        EVP_PKEY_CTX_free(sign_ctx);
    } while(len5 != 0x48);
    return res;
}

byte *sign2(EC_KEY* key, byte *data, int data_len) {
    int len = 0;
    byte * res;
    do {
        ECDSA_SIG *sig = ECDSA_do_sign(data, data_len, key);
        len = i2d_ECDSA_SIG(sig, NULL);

        res = malloc(len);
        byte *f = res;
        i2d_ECDSA_SIG(sig, &f);
    } while (len != 0x48);

/*
    // test check
    char packet_bytes[] = {
      0x30, 0x46, 0x02, 0x21, 0x00, 0xa3, 0xad, 0xaa, 0x61,
      0x00, 0xe6, 0x9d, 0xbd, 0xcf, 0x48, 0x73, 0xb7,
      0xa6, 0xed, 0xe3, 0x62, 0x0a, 0x79, 0xe4, 0xf8,
      0x14, 0x27, 0x4d, 0xeb, 0x73, 0x91, 0x01, 0x0c,
      0xae, 0x08, 0xb9, 0x43, 0x02, 0x21, 0x00, 0xd3,
      0x28, 0xa4, 0x86, 0xcf, 0x8b, 0xaf, 0x35, 0xc9,
      0x04, 0xf7, 0x1f, 0xe2, 0x56, 0x22, 0xf7, 0x5d,
      0xdf, 0x53, 0x13, 0x4f, 0xc6, 0xdb, 0x6b, 0xc0,
      0x0d, 0x57, 0x90, 0xc4, 0x23, 0xfe, 0x06
    };
    char *test = malloc(sizeof(packet_bytes));
    memcpy(test, packet_bytes, sizeof(packet_bytes));
    char* pp = packet_bytes;

    ECDSA_SIG *sig2 = d2i_ECDSA_SIG(NULL, &pp, 0x48);
*/
//    int status = ECDSA_do_verify(data, data_len, sig2, load_key(ecdsa_private_key, false));
//    printf("Verified: %d", status);
//    if (status == 1) {
//    return test;
//    } else {
//        exit(-1);
//    }

    return res;
}


void handshake() {
    int len;
    byte *client_hello = malloc(len = sizeof(tls_client_hello) / sizeof(byte));
    byte buff[1024 * 1024];

    HASHContext *tls_hash_context = HASH_Create(HASH_AlgSHA256);
    HASHContext *tls_hash_context2 = HASH_Create(HASH_AlgSHA256);
    HASH_Begin(tls_hash_context);
    HASH_Begin(tls_hash_context2);

    // Send ClientHello
    memcpy(client_hello, tls_client_hello, len);
    memcpy(client_hello + 0xf, client_random, 0x20);
    HUpdate(tls_hash_context, client_hello + 0x09, 0x43);
    HUpdate(tls_hash_context2, client_hello + 0x09, 0x43);
    qwrite(client_hello, sizeof(tls_client_hello));

    // Receive ServerHello
    qread(buff, 1024 * 1024, &len);

    // SET DEBUG
    /*
     * 10
     * */
/*
    memcpy(buff + 0xb, server_random, 0x20);
    char packet_bytes[] = {
      0x54, 0x4c, 0x53, 0x90, 0x0c, 0xb8, 0x01
    };
    memcpy(buff + 0x2c, packet_bytes, sizeof(packet_bytes));
*/
    /*
     *11
     */
/*
     char rnd_stat[] = {
  0x00, 0x00, 0x67, 0x28, 0x90, 0xb8, 0xb0, 0x9a,
  0x24, 0x98, 0x2e, 0x09, 0x7b, 0x8f, 0x03, 0xa8,
  0x27, 0x53, 0x79, 0xb2, 0x1f, 0xf3, 0x19, 0xaf,
  0x2e, 0xa8, 0xff, 0xea, 0x53, 0x02, 0xa7, 0x38
};
memcpy(buff + 0xb, rnd_stat, 0x20);
char packet_bytes[] = {
  0x54, 0x4c, 0x53, 0x90, 0xb8, 0xb0, 0x9a
};

memcpy(buff + 0x2c, packet_bytes, sizeof(packet_bytes));
*/
//
// SET DEBUG END


    memcpy(server_random, buff + 0xb, 0x20);
    puts("Server tls Random:");
    print_hex(server_random, 0x20);
    printf("%d", len);
    HUpdate(tls_hash_context, buff + 0x05, 0x3d);
    HUpdate(tls_hash_context2, buff + 0x05, 0x3d);


    // Send cert
    EC_KEY *priv_key = load_key(privkey1, true);
    EC_KEY *pub_key = load_key(pubkey1, false);
    int status;

    if (!priv_key || !pub_key) {
        puts("failed to load");
        return;
    }

    EVP_PKEY *priv = EVP_PKEY_new(), *pub = EVP_PKEY_new();
    status = EVP_PKEY_set1_EC_KEY(priv, priv_key);
    status = EVP_PKEY_set1_EC_KEY(pub, pub_key);

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(priv, NULL);

    status = EVP_PKEY_derive_init(ctx);
    status = EVP_PKEY_derive_set_peer(ctx, pub);

    size_t len2 = 0;
    status = EVP_PKEY_derive(ctx, NULL, &len2);

    byte pre_master_secret[len2];

    ECDH_compute_key(pre_master_secret, 0x20, EC_KEY_get0_public_key(pub_key), priv_key, NULL);

    char seed[0x40], expansion_seed[0x40];
    memcpy(seed, client_random, 0x20);
    memcpy(seed + 0x20, server_random, 0x20);

    memcpy(expansion_seed + 0x20, client_random, 0x20);
    memcpy(expansion_seed, server_random, 0x20);

    guint8 master_secret[0x30];
    validity90_tls_prf(pre_master_secret, 0x20, "master secret", seed, 0x40, 0x30, master_secret, NULL);

    puts("master secret");
    print_hex(master_secret, 0x30);

    validity90_tls_prf(master_secret, 0x30, "key expansion", seed, 0x40, 0x120, key_block, NULL);
    puts("keyblock");
    print_hex(key_block, 0x120);


    // copy client_random to cert
//    memcpy(tls_certificate + 0x13, client_random + 0x04, 0x02);
//    memcpy(tls_certificate + 0x13, client_random + 0x05, 0x02);
    // copy ecdhe pub to key exchange
    memcpy(tls_certificate + 0xce + 4, privkey1, 0x40);

    HUpdate(tls_hash_context, tls_certificate + 0x09, 0x109);
    HUpdate(tls_hash_context2, tls_certificate + 0x09, 0x109);

    byte test[0x20];int test_len;
    HASH_End(tls_hash_context, test, &test_len, 0x20);
    puts("Hash");
    print_hex(test, 0x20);



    // cert verify
//    byte* cert_verify_signature = sign(load_pkey(ecdsa_private_key, true), all_messages, all_messages_index);
    byte* cert_verify_signature = sign2(load_key(ecdsa_private_key, true), test, 0x20);
//    byte* cert_verify_signature = sign(load_pkey(ecdsa_private_key, true), test, 0x20);

    printf("\nCert signed: \n");
    print_hex(cert_verify_signature, 0x48);
    memcpy(tls_certificate + 0x09 + 0x109 + 0x04, cert_verify_signature, 0x48);

//    printf("\nWhat signed: \n");
//    print_hex(all_messages, all_messages_index);

    // encrypted finished
    byte handshake_messages[0x20]; int len3 = 0x20;
    HUpdate(tls_hash_context2, tls_certificate + 0x09 + 0x109, 0x4c);
    HASH_End(tls_hash_context2, handshake_messages, &len3, 0x20);

    puts("hash of handshake messages");
    print_hex(handshake_messages, 0x20); // ok

    byte *finished_message = malloc(0x10);
    finished_message[0] = 0x14;
    finished_message[1] = finished_message[2] = 0x00;
    finished_message[3] = 0x0c;

    validity90_tls_prf(master_secret, 0x30, "client finished", handshake_messages, 0x20, 0x0c, finished_message + 0x04, NULL);
    // copy handshake protocol

    puts("client finished");
    print_hex(finished_message, 0x10);

    byte * final;
    int final_size;
    mac_then_encrypt(0x16, finished_message, 0x10, &final, &final_size);
    memcpy(tls_certificate + 0x169, final, final_size);

    puts("final");
    print_hex(final, final_size);

    qwrite(tls_certificate, sizeof(tls_certificate));
    qread(buff, 1024 * 1024, &len);
}

void tls_write(byte * data, int data_len) {
    byte *res;
    int res_len;
    mac_then_encrypt(0x17, data, data_len, &res, &res_len);
    byte *wr = malloc(res_len + 5);
    memcpy(wr + 5, res, res_len);
    wr[0] = 0x17; wr[1] = wr[2] = 0x03; wr[3] = res_len >> 8; wr[4] = res_len & 0xFF;
    qwrite(wr, res_len + 5);

    free(res);
    free(wr);
}

void tls_read(byte *output_buffer, int *output_len) {
    byte *raw_buff = malloc(1024 * 1024);
    int raw_buff_len;

    qread(raw_buff, 1024 * 1024, &raw_buff_len);

    int buff_len = raw_buff_len - 5;
    byte *buff = raw_buff + 5;

    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    errb(EVP_DecryptInit(context, EVP_aes_256_cbc(), key_block + 0x60, buff));
    EVP_CIPHER_CTX_set_padding(context, 0);

    int res_len = buff_len - 0x10;
    int tlen1 = 0, tlen2;
    byte *res = malloc(res_len);
    errb(EVP_DecryptUpdate(context, res, &tlen1, buff + 0x10, res_len));

    errb(EVP_DecryptFinal(context, res + tlen1, &tlen2));
    EVP_CIPHER_CTX_free(context);

    *output_len = tlen1 + tlen2 - 0x20 - (res[res_len - 1] + 1);
    memcpy(output_buffer, res, *output_len);

    free(raw_buff);
}

int writeImage(char* filename, int width, int height, float *buffer) {
    int code = 0;
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;

    // Open file for writing (binary mode)
    fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file %s for writing\n", filename);
        code = 1;
        goto finalise;
    }

    // Initialize write structure
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        fprintf(stderr, "Could not allocate write struct\n");
        code = 1;
        goto finalise;
    }

    // Initialize info structure
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fprintf(stderr, "Could not allocate info struct\n");
        code = 1;
        goto finalise;
    }

    // Setup Exception handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "Error during png creation\n");
        code = 1;
        goto finalise;
    }

    png_init_io(png_ptr, fp);

    // Write header (8 bit colour depth)
    png_set_IHDR(png_ptr, info_ptr, width, height,
            8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);


    png_write_info(png_ptr, info_ptr);

    // Write image data
    int x, y;
    for (y=0 ; y<height ; y++) {
       png_write_row(png_ptr, buffer + 36 * y);
    }

    // End write
    png_write_end(png_ptr, NULL);

    finalise:
    if (fp != NULL) fclose(fp);
    if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

    return code;
}

void fingerprint() {
    byte data1[] = {
        0x08, 0x5c, 0x20, 0x00, 0x80, 0x07, 0x00, 0x00, 0x00, 0x04
    };
    byte data2[] = {
        0x07, 0x80, 0x20, 0x00, 0x80, 0x04
    };
    byte data345[] = {
        0x75
    };
    byte data67[] = {
        0x43, 0x02
    };

    byte data10[] = {
        0x51, 0x00, 0x20, 0x00, 0x00 // read data - return buffer
    };
    byte data11[] = {
        0x51, 0x00, 0x20, 0x00, 0x00
    };

    byte response[1024 * 1024];
    int response_len = 0;

    tls_write(led_green_on, sizeof(led_green_on));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    tls_write(data1, sizeof(data1)); // not required
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    tls_write(data2, sizeof(data2)); // not required
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    for (int i =0; i < 3; i++ ){ // not required
        tls_write(data345, sizeof(data345));
        tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);
    }

    for (int i =0; i < 2; i++ ){ // not required
        tls_write(data67, sizeof(data67));
        tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);
    }


    /*tls_write(scan_matrix2, sizeof(scan_matrix2));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);*/

    // Check DB
    tls_write(scan_matrix1, sizeof(scan_matrix1));tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);
    //tls_write(v97_scan_matrix2, sizeof(v97_scan_matrix2)); tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    byte interrupt[0x100]; int interrupt_len;

    const byte waiting_finger[] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
    const byte finger_down[] = { 0x02, 0x00, 0x40, 0x10, 0x00 };
    const byte finger_down2[] = { 0x02, 0x00, 0x40, 0x06, 0x06 };
    const byte scanning_prints[] = { 0x03, 0x40, 0x01, 0x00, 0x00 };
    const byte scan_completed[] = { 0x03, 0x41, 0x03, 0x00, 0x40 };

    const byte desired_interrupt[] = { 0x03, 0x43, 0x04, 0x00, 0x41 };
    const byte desired_interrupt_v97[] = { 0x03, 0x42, 0x04, 0x00, 0x40 };
    const byte low_quality_scan_interrupt[] = { 0x03, 0x42, 0x04, 0x00, 0x40 };
    const byte scan_failed_too_short_interrupt[] = { 0x03, 0x60, 0x07, 0x00, 0x40 };
    const byte scan_failed_too_short2_interrupt[] = { 0x03, 0x61, 0x07, 0x00, 0x41 };
    const byte scan_failed_too_fast_interrupt[] = { 0x03, 0x20, 0x07, 0x00, 0x00 };

    puts("Awaiting fingerprint:");
    while (true) {
        int status = libusb_interrupt_transfer(dev, 0x83, interrupt, 0x100, &interrupt_len, 5 * 1000);
        if (status == 0) {
            puts("interrupt:");
            print_hex(interrupt, interrupt_len);
            fflush(stdout);

            if (sizeof(waiting_finger) == interrupt_len &&
                memcmp(waiting_finger, interrupt, interrupt_len) == 0) {
                puts("Waiting for finger...");
            }
            if ((sizeof(finger_down) == interrupt_len &&
                 memcmp(finger_down, interrupt, interrupt_len) == 0) ||
                (sizeof(finger_down2) == interrupt_len &&
                 memcmp(finger_down2, interrupt, interrupt_len) == 0)) {
                puts("Finger is on the sensor...");
            }
            if (sizeof(scanning_prints) == interrupt_len &&
                memcmp(scanning_prints, interrupt, interrupt_len) == 0) {
                puts("Scan in progress...");
            }
            if (sizeof(scan_completed) == interrupt_len &&
                memcmp(scan_completed, interrupt, interrupt_len) == 0) {
                puts("Fingerprint scan completed...");
            }
            if (sizeof(scan_failed_too_short_interrupt) == interrupt_len &&
                memcmp(scan_failed_too_short_interrupt, interrupt, interrupt_len) == 0) {
                puts("Impossible to read fingerprint, keep it in the sensor");
                return;
            }
            if (sizeof(scan_failed_too_short2_interrupt) == interrupt_len &&
                memcmp(scan_failed_too_short2_interrupt, interrupt, interrupt_len) == 0) {
                puts("Impossible to read fingerprint, keep it in the sensor (2)");
                return;
            }
            if (sizeof(scan_failed_too_fast_interrupt) == interrupt_len &&
                memcmp(scan_failed_too_fast_interrupt, interrupt, interrupt_len) == 0) {
                puts("Impossible to read fingerprint, movement was too fast");
                return;
            }
            if (sizeof(desired_interrupt) == interrupt_len &&
                memcmp(desired_interrupt, interrupt, interrupt_len) == 0) {
                puts("Scan succeeded!");
                break;
            }
            if (sizeof(desired_interrupt_v97) == interrupt_len &&
                memcmp(desired_interrupt_v97, interrupt, interrupt_len) == 0) {
                puts("Scan succeeded! (v97)");
                break;
            }
            if (sizeof(low_quality_scan_interrupt) == interrupt_len &&
                memcmp(low_quality_scan_interrupt, interrupt, interrupt_len) == 0) {
                puts("Scan succeeded! Low quality.");
                break;
            }
        }
    }

    byte image[144 * 144];
    int image_len = 0;

    tls_write(data10, sizeof(data10));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);
    memcpy(image, response + 0x12, response_len - 0x12);
    image_len += response_len - 0x12;

    tls_write(data10, sizeof(data10));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);
    memcpy(image + image_len, response + 0x06, response_len - 0x06);
    image_len += response_len - 0x06;

    tls_write(data10, sizeof(data10));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);
    memcpy(image + image_len, response + 0x06, response_len - 0x06);
    image_len += response_len - 0x06;




    //char packet4[] = { 0x4b, 0x00, 0x00, 0x0b, 0x00, 0x53, 0x74, 0x67, 0x57, 0x69, 0x6e, 0x64, 0x73, 0x6f, 0x72, 0x00 };
    //tls_write(packet4, sizeof(packet4));
    //tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    // Check against db packet
    char packet1[] = { 0x5e, 0x02, 0xff, 0x03, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 };
    tls_write(packet1, sizeof(packet1));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    int validated_finger_id = -1;
    while (true) {
        int status = libusb_interrupt_transfer(dev, 0x83, interrupt, 0x100, &interrupt_len, 5 * 1000);
        if (status == 0) {
            puts("interrupt:");
            print_hex(interrupt, interrupt_len);
            fflush(stdout);
            validated_finger_id = interrupt[0] == 0x03 ? interrupt[2] : 0;
            // interrupt [1] == 0
            // interrupt [2] == finger id
            //
            break;
        } else if (status == LIBUSB_ERROR_TIMEOUT) {
            puts("\nValidation check timeout - try restarting prototype\n");
            break;
        }
    }

    // Properly reset so consequtive calls work 1
    char packet2[] = { 0x60, 0x00, 0x00, 0x00, 0x00 };
    tls_write(packet2, sizeof(packet2));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    // Properly reset so consequtive calls work 2
    char packet3[] = { 0x62, 0x00, 0x00, 0x00, 0x00 };
    tls_write(packet3, sizeof(packet3));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    if (idProduct != 0x97) {
        printf("total len  %d\n", image_len);
        writeImage("img.png", 144, 144, image);
        puts("Image written - img.png, img.raw");

        FILE *f = fopen("img.raw", "wb");
        fwrite(image, 144, 144, f);
        fclose(f);
    }

    puts("Done");

    if (validated_finger_id != -1) {
        if (validated_finger_id > 0) {
            tls_write(led_green_blink, sizeof(led_green_blink));tls_read(response, &response_len);
            printf("\n\nFingerprint MATCHES DB Finger id: %d!\n", validated_finger_id);
        } else {
            tls_write(led_red_blink, sizeof(led_red_blink));tls_read(response, &response_len);
            printf("\n\nFingerprint UNKNOWN!\n");
        }
    } else {
        tls_write(led_red_blink, sizeof(led_red_blink));tls_read(response, &response_len);
        puts("Fingerprint check procedure didn't worked");
    }
}

void led_test() {
    byte response[1024 * 1024];
    int response_len = 0;

    puts("Green on");
    tls_write(led_green_on, sizeof(led_green_on));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    sleep(2);

    puts("Red blink x3 then off");
    tls_write(led_red_blink, sizeof(led_red_blink));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    sleep(2);

    puts("Green blink");
    tls_write(led_green_blink, sizeof(led_green_blink));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    char led_script[] = {
        0x39, // packet type?

        0xff, 0x10, 0x00, 0x00, // Script len

        0xff, 0x03, // part len
        0x00, 0x00, 0x01, 0xff, 0x00, 0x20, 0x00,
        0x00, 0x00,
        0x00,

        0xff, // V green
        0xff, // V2 green ?
        0x0, 0x0,
        0xff, // V red
        0xff, // V2 red ?
        0x00, 0x00,

        0xff, 0x03,
        0x00, 0x00, 0x01, 0xff, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,

        0xff, 0x03,
        0x00, 0x00, 0x01, 0xff, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

        0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

        0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

        0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    sleep(2);
    puts("Custom script");
    tls_write(led_script, sizeof(led_script));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);
}

int main(int argc, char *argv[]) {
    puts("Prototype version 15");
    libusb_init(NULL);
    libusb_set_debug(NULL, 3);

    struct libusb_config_descriptor descr;
    libusb_device ** dev_list;

    int dev_cnt = libusb_get_device_list(NULL, &dev_list);
    for (int i = 0; i < dev_cnt; i++) {
        struct libusb_device_descriptor descriptor;
        libusb_get_device_descriptor(dev_list[i], &descriptor);

        for (int j = 0; j < sizeof(all_devices) / sizeof(DeviceInfo); j++) {
            if (all_devices[j].vid == descriptor.idVendor && all_devices[j].pid == descriptor.idProduct) {
                printf("Found device %04x:%04x\n", descriptor.idVendor, descriptor.idProduct);

                if (all_devices[j].description != NULL) {
                    puts(all_devices[j].description);
                }

                if (all_devices[j].unsupported) {
                    return -1;
                }

                idProduct = descriptor.idProduct;

                err(libusb_get_device_descriptor(dev_list[i], &descr));
                err(libusb_open(dev_list[i], &dev));
                break;
            }
        }

    }
    if (dev == NULL) {
        puts("No devices found");
        return -1;
    }

    err(libusb_reset_device(dev));
    err(libusb_set_configuration(dev, 1));
    err(libusb_claim_interface(dev, 0));

    loadBiosData();

    puts("");

    SECStatus status = NSS_NoDB_Init(".");
    if (status != SECSuccess) {
        puts("failed to init nss");
        return -1;
    }

    OpenSSL_add_all_algorithms(); ERR_load_crypto_strings();

    init();
    handshake();

    printf("IN: "); print_hex_string(key_block + 0x60, 0x20);
    printf("OUT: "); print_hex_string(key_block + 0x40, 0x20);

    fflush(stdout);

    while(true) {
        puts("");
        puts("1 - Scan fingerprint");
        puts("2 - Test leds");
        puts("0 - Exit");

        char x[1024];
        scanf("%s", x);

        if (x[0] == '1') {
            fingerprint();
        } else if (x[0] == '2') {
            led_test();
        } else if (x[0] == '0') {
            exit(EXIT_SUCCESS);
        }
    }

    return 0;
}
