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
#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include "constants.h"


#define xstr(a) str(a)
#define str(a) #a

#define max(a,b) (a > b ? a : b)
#define min(a,b) (a > b ? b : a)

#define err(x) res_err(x, xstr(x))

static libusb_device_handle * dev;


static const byte client_random[] = {
  0x95, 0x6c, 0x41, 0xa9, 0x12, 0x86, 0x8a, 0xda,
  0x9b, 0xb2, 0x5b, 0xb4, 0xbb, 0xd6, 0x1d, 0xde,
  0x4f, 0xda, 0x23, 0x2a, 0x74, 0x7b, 0x2a, 0x93,
  0xf8, 0xac, 0xc6, 0x69, 0x24, 0x70, 0xc4, 0x2a
};

static const byte server_random[] = {
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
    }
}

void qwrite(byte * data, int len) {
    int send;
    err(libusb_bulk_transfer(dev, 0x01, data, len, &send, 100));
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


void handshake() {
    int len;
    byte *client_hello = malloc(len = sizeof(tls_client_hello) / sizeof(byte));
    byte buff[1024 * 1024];

    // Send ClientHello
    memcpy(client_hello, tls_client_hello, len);
    memcpy(client_hello + 0xf, client_random, 0x20);
    qwrite(client_hello, len);

    // Receive ServerHello
    qread(buff, 1024 * 1024, &len);
    memcpy(server_random, buff + 0xb, 0x20);
    puts("Server tls Random:");
    print_hex(server_random, 0x20);

    // Send cert

}

// This is a DER encoded, ANSI X9.62 CurveParams object which simply
// specifies P256.
static const byte kANSIX962CurveParams[] = {
    0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
};


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

static const byte privkey1[] = {
    0x1d, 0xd8, 0x36, 0x68, 0xe9, 0xb0, 0x7b, 0x93, 0x12, 0x38, 0x31, 0x23, 0x90, 0xc8, 0x87, 0xca,
    0xdb, 0x82, 0x27, 0x39, 0xde, 0x7b, 0x43, 0xd2, 0x23, 0xd7, 0xcd, 0xd1, 0x3c, 0x77, 0x0e, 0xd2,

    0xd1, 0x93, 0x70, 0x02, 0xaf, 0x3b, 0x18, 0x47, 0xc5, 0x30, 0x4c, 0x33, 0x60, 0xcf, 0xbf, 0xc5,
    0x9b, 0x3c, 0x67, 0xd9, 0x45, 0x06, 0x38, 0xda, 0x92, 0xbe, 0x65, 0xbf, 0x81, 0x8c, 0xaa, 0x7e,

    0x20, 0x14, 0x3b, 0x7b, 0x62, 0x64, 0x90, 0x07, 0x54, 0x4e, 0x7a, 0x98, 0xf9, 0x81, 0xbe, 0xc1,
    0xf2, 0x1f, 0x9a, 0x29, 0x65, 0xb6, 0xcc, 0x29, 0x0c, 0x45, 0xd3, 0x87, 0xae, 0xbf, 0xa4, 0xd9
};

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

byte* TLS_PRF(byte * secret, int secret_len, char * str, byte * seed40, int required_len) {
    int total_len = 0;

    int str_len = strlen(str);
    byte seed[str_len + 0x40];
    memcpy(seed, str, str_len);
    memcpy(seed + str_len, seed40, 0x40);
    int seed_len = str_len + 0x40;


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
    status = EVP_PKEY_derive(ctx, pre_master_secret, &len);
//EVP_PKEY_CTX_set_ecdh_kdf_type(ctx, EVP_PKEY_ECDH_KDF_X9_62)
    print_hex(pre_master_secret, len);
    puts("\n");


    char seed[0x40], expansion_seed[0x40];
    memcpy(seed, client_random, 0x20);
    memcpy(seed + 0x20, server_random, 0x20);

    memcpy(expansion_seed + 0x20, client_random, 0x20);
    memcpy(expansion_seed, server_random, 0x20);

    puts("master secret");
    byte * master_secret = TLS_PRF(pre_master_secret, 0x20, "master secret", seed, 0x30);
    print_hex(master_secret, 0x30);


    puts("keyblock");
    byte * key_block = TLS_PRF(master_secret, 0x30, "key expansion", seed, 0x120);
    print_hex(key_block, 0x120);
//    EVP_PKEY_CTX_set_ecdh_kdf_md()

//    EVP_PKEY_derive()

    puts("ok");
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


//    init();
//    handshake();
    SECStatus status = NSS_NoDB_Init(".");
    if (status != SECSuccess) {
        puts("failed to init nss");
        return -1;
    }
    test_crypto_hash();
//    puts("hmac");
//    test_crypto_hash_hmac();
//    test_crypto1();

//    test_derivation();

//    ectest_curve_pkcs11();


    openssl();

    //test_crypto1();

    return 0;
}
