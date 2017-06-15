#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

int validity90_parse_rsp6(validity90* ctx, byte_array * data) {
    if (!data || data->size < 8) {
        return -1;
    }

    int index = 4; // Skip first 8 bytes - unkown header

    while (true) {
        if (data->size - index < sizeof(rsp6_record_header)) {
            return -1;
        }

        rsp6_record_header * header = &data->data[index];

        switch (header->type) {
        case 0x0000:
            break;
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
}

int validity90_get_ceritficate(validity90* ctx, byte_array ** cert);

int validity90_get_driver_ecdsa_private_key(validity90* ctx, byte_array ** key);

int validity90_get_device_ecdh_public_key(validity90* ctx, byte_array ** key);
