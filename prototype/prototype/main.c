#include <stdio.h>
#include <stdbool.h>
#include <libusb.h>
#include <stdlib.h>
#include <string.h>
#include <nss.h>
#include <nss/keyhi.h>
#include <nss/keythi.h>
#include <pk11pub.h>
#include "constants.h"


#define xstr(a) str(a)
#define str(a) #a


#define err(x) res_err(x, xstr(x))

static libusb_device_handle * dev;
static byte client_random[0x20] = {
    0x44, 0x44, 0x44, 0x44,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
};

static byte server_random[20];

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

    SECStatus localNSS_NoDB_Init = NSS_NoDB_Init("/home/nsl/Repositories/validity90/nss_tmp/");
    // Gen EC p256 keypair
    SECKEYPublicKey* pub_key = NULL;
    SECKEYPrivateKey* priv_key = NULL;
//    SECItem ec_der_params;
//    memset(&ec_der_params, 0, sizeof(ec_der_params));
//    ec_der_params.data = kANSIX962CurveParams;
//    ec_der_params.len = sizeof(kANSIX962CurveParams);
//    priv_key = SECKEY_CreateECPrivateKey(&ec_der_params, &pub_key, NULL);



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

//    ssl3_
    puts("done");
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

    test_crypto1();
    return 0;
}
