/*
    Warning: extremly ugly code
*/

#include <stdio.h>
#include <stdbool.h>
#include <libusb.h>
#include <stdlib.h>
#include <string.h>
#include <nss.h>
#include <nss/keyhi.h>
#include <nss/keythi.h>
#include <secoid.h>
#include <secmodt.h>
#include <sechash.h>
#include <pk11pub.h>

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


#define xstr(a) str(a)
#define str(a) #a

#define max(a,b) (a > b ? a : b)
#define min(a,b) (a > b ? b : a)

#define err(x) res_err(x, xstr(x))
#define errb(x) res_errb(x, xstr(x))

static libusb_device_handle * dev;


static const byte client_random[] = {
  0x95, 0x6c, 0x41, 0xa9, 0x12, 0x86, 0x8a, 0xda,
  0x9b, 0xb2, 0x5b, 0xb4, 0xbb, 0xd6, 0x1d, 0xde,
  0x4f, 0xda, 0x23, 0x2a, 0x74, 0x7b, 0x2a, 0x93,
  0xf8, 0xac, 0xc6, 0x69, 0x24, 0x70, 0xc4, 0x2a
};

byte server_random[] = {
  0x00, 0x4b, 0xc7, 0x66, 0x90, 0x0c, 0xb8, 0x01,
  0x0a, 0xd5, 0x38, 0x7b, 0x72, 0x0d, 0xe6, 0x13,
  0x08, 0x75, 0x8d, 0x94, 0x6b, 0x34, 0x94, 0x44,
  0xdb, 0x83, 0x35, 0x9e, 0x12, 0xc4, 0x03, 0x97
};


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

void compare(byte * data1, int data_len, dword * expected, int exp_len) {
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
        puts("Expected:");
        print_hex_dw(expected, exp_len);
        puts("Got:");
        print_hex(data1, data_len);
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
    err(libusb_bulk_transfer(dev, 0x01, data, len, &send, 1000));
}

void qread(byte * data, int len, int *out_len) {
    err(libusb_bulk_transfer(dev, 0x81, data, len, out_len, 1000));
}

void init() {
    int len;
    byte buff[1024 * 1024];

#define STEP(a,b) { \
    qwrite(a, sizeof(a) / sizeof(byte)); \
    qread(buff, 1024 * 1024, &len); \
    compare(buff, len, b, sizeof(b) / sizeof(dword)); \
}

    STEP(init_sequence_msg1, init_sequence_rsp1);
    STEP(init_sequence_msg2, init_sequence_rsp2);
    STEP(init_sequence_msg3, init_sequence_rsp3);
    STEP(init_sequence_msg4, init_sequence_rsp4);
    STEP(init_sequence_msg5, init_sequence_rsp5);
    STEP(init_sequence_msg6, init_sequence_rsp6);
#undef STEP
}


void test_crypto1() {
    // Gen EC p256 keypair
    SECKEYPublicKey* pub_key = NULL;
    SECKEYPrivateKey* priv_key = NULL;
//    SECItem ec_der_params;
//    memset(&ec_der_params, 0, sizeof(ec_der_params));
//    ec_der_params.data = kANSIX962CurveParams;
//    ec_der_params.len = sizeof(kANSIX962CurveParams);
//    priv_key = SECKEY_CreateECPrivateKey(&ec_der_params, &pub_key, NULL);

//    PK11_ImportPublicKey()
//    PK11_ImportPrivateKeyInfoAndReturnKey();
//    PK11_ExportDERPrivateKeyInfo();
//    PK11_ExportPrivKeyInfo()

    SECOidData* oid_data = SECOID_FindOIDByTag(SEC_OID_SECG_EC_SECP256R1);
    byte * buff = malloc(oid_data->oid.len + 2);

    SECKEYECParams ec_parameters = {
        siDEROID, buff,
        oid_data->oid.len + 2
      };

      ec_parameters.data[0] = SEC_ASN1_OBJECT_ID;
      ec_parameters.data[1] = oid_data->oid.len;
      memcpy(ec_parameters.data + 2, oid_data->oid.data, oid_data->oid.len);


    PK11SlotInfo *slot
            = PK11_GetInternalKeySlot();
    priv_key = PK11_GenerateKeyPair(slot, CKM_EC_KEY_PAIR_GEN,
                                   & ec_parameters, &pub_key,
                                    PR_FALSE, PR_FALSE, NULL);

//    SECKEYPrivateKeyInfo *derPriv = PK11_ExportEncryptedPrivKeyInfo(priv_key, NULL);
//    SECKEYPrivateKeyInfo *info = PK11_ExportPrivKeyInfo(priv_key, NULL);
//    ssl3_

//    PK11_ExportEncryptedPrivKeyInfo(slot, )

//    print_hex(derPriv->data, derPriv->len);
    puts("done");
}

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

    return res;
}

void test_crypto_hash() {
    byte p[] = {
        0x71, 0x7c, 0xd7, 0x2d, 0x09, 0x62, 0xbc, 0x4a, 0x28, 0x46, 0x13, 0x8d, 0xbb, 0x2c, 0x24, 0x19,
        0x25, 0x12, 0xa7, 0x64, 0x07, 0x06, 0x5f, 0x38, 0x38, 0x46, 0x13, 0x9d, 0x4b, 0xec, 0x20, 0x33
    };

    byte data[] = {
        0xbc, 0x41, 0x9d, 0xfc, 0x39, 0xc9, 0xba, 0x69, 0xa7, 0x4d, 0x5d, 0x60, 0x0a, 0xc3, 0x5b, 0x7b,
        0x1a, 0xfb, 0x2b, 0x52, 0xe5, 0xd2, 0x4a, 0x23, 0x04, 0x58, 0x67, 0xc8, 0x3a, 0x98, 0xaa, 0x9a,
        0x47, 0x57, 0x4b, 0x56, 0x69, 0x72, 0x74, 0x75, 0x61, 0x6c, 0x42, 0x6f, 0x78, 0x00, 0x30, 0x00
    };

    PK11Context* context = hmac_make_context(p, 0x20);
    PK11_DigestOp(context, data, 0x30);

    byte res[0x20];
    int len = 0x20;
    PK11_DigestFinal(context, res, &len, 0x20);

    print_hex(res, 0x20);
    /*
     * 0000 48 78 02 70 5e 5a c4 a9  93 1c 44 aa 4d 32 25 22
     *   0010 39 e0 bf 8f 0c 85 4d de  49 0c cc f6 87 ef ad 9c
*/
}

void test_crypto_hash_hmac() {
    HASHContext *context = HASH_Create(HASH_AlgSHA256);
    HASH_Begin(context);

    int hashLen = HASH_ResultLen(HASH_AlgSHA256);
    byte data[] = {0x00};
    byte result[hashLen];
    HASH_Update(context, data, 1);

    int res_len = 0;
    HASH_End(context, result, &res_len, hashLen);

    print_hex(result, hashLen);
}


static const byte pubkey1[] = {
    0x5f, 0x71, 0x17, 0x6f, 0x76, 0x66, 0x55, 0x74, 0xa3, 0x86, 0x53, 0x53, 0x10, 0xf6, 0x98, 0x18,
    0x6f, 0x42, 0x9b, 0xf0, 0x6e, 0xfa, 0x05, 0x9b, 0x0c, 0x3f, 0x99, 0xbc, 0xfe, 0xb5, 0xd6, 0xce,

    0x3e, 0x61, 0x55, 0x91, 0xab, 0x00, 0x99, 0xb0, 0x4f, 0x6f, 0x4b, 0x68, 0xac, 0xbd, 0x67, 0x81,
    0x65, 0xb8, 0x26, 0x75, 0x1d, 0x50, 0xe3, 0x87, 0xd0, 0xcc, 0xfd, 0x49, 0x5f, 0xf4, 0xce, 0xca
};
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
    err(EC_KEY_check_key(key) - 1);

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

byte* TLS_PRF2(byte * secret, int secret_len, char * str, byte * seed40, int seed40_len, int required_len) {
    int total_len = 0;

    int str_len = strlen(str);
    byte seed[str_len + seed40_len];
    memcpy(seed, str, str_len);
    memcpy(seed + str_len, seed40, seed40_len);
    int seed_len = str_len + seed40_len;


    byte* res = malloc(required_len);
    byte *a = hmac_compute(secret, secret_len, seed, seed_len);

    while (total_len < required_len) {
        byte buff[0x20 + seed_len];
        memcpy(buff, a, 0x20);
        memcpy(buff + 0x20, seed, seed_len);

        byte * p = hmac_compute(secret, secret_len, buff, 0x20 + seed_len);
        memcpy(res + total_len, p, min(0x20, required_len - total_len));
        free(p);

        total_len += 0x20;

        byte *t = hmac_compute(secret, secret_len, a, 0x20);
        free(a);
        a = t;
    }
    free(a);

    return res;
}

void openssl() {
    EC_KEY *priv_key = load_key(privkey1, true);
    EC_KEY *pub_key = load_key(pubkey1, false);

    if (!priv_key || !pub_key) {
        puts("failed to load");
        return;
    }
    int status;

    EC_KEY_print_fp(stdout, priv_key, 0);


    EVP_PKEY *priv = EVP_PKEY_new(), *pub = EVP_PKEY_new();
    status = EVP_PKEY_set1_EC_KEY(priv, priv_key);
    status = EVP_PKEY_set1_EC_KEY(pub, pub_key);

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(priv, NULL);


    status = EVP_PKEY_derive_init(ctx);
    status = EVP_PKEY_derive_set_peer(ctx, pub);

    size_t len = 0;
    status = EVP_PKEY_derive(ctx, NULL, &len);

    byte pre_master_secret[len];
//    status = EVP_PKEY_derive(ctx, pre_master_secret, &len);

    ECDH_compute_key(pre_master_secret, 0x20, EC_KEY_get0_public_key(pub_key), priv_key, NULL);

    print_hex(pre_master_secret, len);
    puts("\n");


    char seed[0x40], expansion_seed[0x40];
    memcpy(seed, client_random, 0x20);
    memcpy(seed + 0x20, server_random, 0x20);

    memcpy(expansion_seed + 0x20, client_random, 0x20);
    memcpy(expansion_seed, server_random, 0x20);

    puts("master secret");
    byte * master_secret = TLS_PRF2(pre_master_secret, 0x20, "master secret", seed, 0x40, 0x30);
    print_hex(master_secret, 0x30);


    puts("keyblock");
    byte * key_block = TLS_PRF2(master_secret, 0x30, "key expansion", seed, 0x40, 0x120);
    print_hex(key_block, 0x120);
//    EVP_PKEY_CTX_set_ecdh_kdf_md()

//    EVP_PKEY_derive()

    puts("ok");

    return;////////////////////////////////////////////////////////////////////////////


//CKM_TLS_MASTER_KEY_DERIVE_DH
    CK_MECHANISM_TYPE derive_mech = CKM_NSS_TLS_MASTER_KEY_DERIVE_SHA256;
    CK_MECHANISM_TYPE hash_mech = CKM_SHA256;
    CK_MECHANISM_TYPE key_mech = CKM_NSS_TLS_MASTER_KEY_DERIVE_SHA256;
    CK_VERSION      pms_version;

    PK11SlotInfo *slot = PK11_GetInternalSlot();

    //CK_SSL3_MASTER_KEY_DERIVE_PARAMS
    CK_TLS12_MASTER_KEY_DERIVE_PARAMS key_derive_params;
    key_derive_params.RandomInfo.pClientRandom = client_random;
    key_derive_params.RandomInfo.ulClientRandomLen = 0x20;
    key_derive_params.RandomInfo.pServerRandom = server_random;
    key_derive_params.RandomInfo.ulServerRandomLen = 0x20;

    key_derive_params.pVersion = &pms_version;
    key_derive_params.prfHashMechanism = CKM_SHA256;

    SECItem key = { .type = siBuffer, .data = pre_master_secret, .len = 0x20 };
    SECItem params_ = {.data = &key_derive_params, .len = sizeof(key_derive_params) };

    PK11SymKey * pms_ = PK11_ImportSymKey(slot, derive_mech, PK11_OriginUnwrap, CKA_DERIVE, &key, NULL);

    PK11SymKey *master_nss = PK11_DeriveWithFlags(pms_, derive_mech, &params_, key_mech, CKA_DERIVE, 0, CKF_SIGN | CKF_VERIFY);


    /*
     * PK11SymKey *PK11_DeriveWithFlags(PK11SymKey *baseKey,
                                 CK_MECHANISM_TYPE derive, SECItem *param, CK_MECHANISM_TYPE target,
                                 CK_ATTRIBUTE_TYPE operation, int keySize, CK_FLAGS flags);
     *
    */
    puts("NSS");


    SECItem *master_nss_data = PK11_GetKeyData(master_nss);
    print_hex(master_nss_data->data, master_nss_data->len);
}

void export_import_keys() {
    EC_KEY *priv_key = load_key(privkey1, true);
    EC_KEY *pub_key = load_key(pubkey1, false);

//    EC_KEY_set_enc_flags(priv_key, );
    int len = i2d_ECPrivateKey(priv_key, NULL);
    byte out[len];
    unsigned char * derkeyPtr = out;
    i2d_ECPrivateKey(priv_key, &derkeyPtr);

    print_hex(out, len);

//    PEM_write_bio_ECPrivateKey(
//    PEM_ASN1_write()
}

byte all_messages[1024 * 1024]; int all_messages_index = 0;
void HUpdate(HASHContext *context, const unsigned char *src, unsigned int len) {
    HASH_Update(context, src, len);
    memcpy(all_messages + all_messages_index, src, len);
    all_messages_index += len;
    puts("HASHING>>>");
    print_hex(src, len);
    puts("HASHING<<");
}

byte * key_block;

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

    byte * master_secret = TLS_PRF2(pre_master_secret, 0x20, "master secret", seed, 0x40, 0x30);
    puts("master secret");
    print_hex(master_secret, 0x30);

    key_block = TLS_PRF2(master_secret, 0x30, "key expansion", seed, 0x40, 0x120);
    puts("keyblock");
    print_hex(key_block, 0x120);


    // copy client_random to cert
    memcpy(tls_certificate + 0x13, client_random + 0x04, 0x02);
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

    memcpy(finished_message + 0x04, TLS_PRF2(master_secret, 0x30, "client finished", handshake_messages, 0x20, 0x0c), 0x0c);
    // copy handshake protocol

    puts("client finished");
    print_hex(finished_message, 0x10);

    byte * final;
    int final_size;
    mac_then_encrypt(0x16, finished_message, 0x10, &final, &final_size);
    memcpy(tls_certificate + 0x169, final, final_size); // TODO encrypt

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

    tls_write(data1, sizeof(data1));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    tls_write(data2, sizeof(data2));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    for (int i =0; i < 3; i++ ){
        tls_write(data345, sizeof(data345));
        tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);
    }

    for (int i =0; i < 2; i++ ){
        tls_write(data67, sizeof(data67));
        tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);
    }


    tls_write(scan_matrix2, sizeof(scan_matrix2));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    tls_write(scan_matrix1, sizeof(scan_matrix1));
    tls_read(response, &response_len);puts("READ:");print_hex(response, response_len);

    byte interrupt[0x100]; int interrupt_len;

    byte desired_interrupt[] = { 0x03, 0x43, 0x04, 0x00, 0x41 };
    byte scan_failed_interrupt[] = { 0x03, 0x20, 0x07, 0x00, 0x00 }; // 03 60 07 00 40

    puts("Awaiting fingerprint:");
    while (true) {
        int status = libusb_interrupt_transfer(dev, 0x83, interrupt, 0x100, &interrupt_len, 5 * 1000);
        if (status == 0) {
            puts("interrupt:");
            print_hex(interrupt, interrupt_len);
            fflush(stdout);

            if (sizeof(desired_interrupt) == interrupt_len &&
                    memcmp(desired_interrupt, interrupt, sizeof(desired_interrupt)) == 0) {
                break;
            }
            if (sizeof(scan_failed_interrupt) == interrupt_len &&
                    memcmp(scan_failed_interrupt, interrupt, sizeof(desired_interrupt)) == 0) {
                puts("scan failed");
                return;
            }
        }
    }

    byte image[144 * 144];
    int image_len = 0;

    tls_write(data10, sizeof(data10));
    tls_read(response, &response_len);
    memcpy(image, response + 0x12, response_len - 0x12);
    image_len += response_len - 0x12;

    tls_write(data10, sizeof(data10));
    tls_read(response, &response_len);
    memcpy(image + image_len, response + 0x06, response_len - 0x06);
    image_len += response_len - 0x06;

    tls_write(data10, sizeof(data10));
    tls_read(response, &response_len);
    memcpy(image + image_len, response + 0x06, response_len - 0x06);
    image_len += response_len - 0x06;

    printf("total len  %d\n", image_len);
    writeImage("img.png", 144, 144, image);
    puts("Image written - img.png, img.raw");

    FILE *f = fopen("img.raw", "wb");
    fwrite(image, 144, 144, f);
    fclose(f);
}

int main(int argc, char *argv[]) {
    libusb_init(NULL);
    libusb_set_debug(NULL, 3);

    dev = libusb_open_device_with_vid_pid(NULL, 0x138a, 0x0090);
    if (dev == NULL) {
        return -1;
    }

    err(libusb_reset_device(dev));
    err(libusb_set_configuration(dev, 1));
    err(libusb_claim_interface(dev, 0));

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
    fingerprint();

//    test_crypto_hash();
//    puts("hmac");
//    test_crypto_hash_hmac();
//    test_crypto1();

//    test_derivation();

//    ectest_curve_pkcs11();


//    openssl();
//export_import_keys();
    //test_crypto1();

    return 0;
}
