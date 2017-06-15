#ifndef VALIDITY90_H
#define VALIDITY90_H

#include <stdint.h>

typedef unsigned char byte;

typedef struct byte_array {
    uint32_t size;
    byte data[];
} byte_array;

byte_array* byte_array_create_from_data(byte* data, uint32_t len);
byte_array* byte_array_create(uint32_t len);
void byte_array_free(byte_array* array);

typedef struct validity90 validity90;

validity90* validity90_create();

void validity90_free(validity90 * ctx);

int validity90_parse_rsp6(validity90* ctx, byte_array *data);

int validity90_get_ceritficate(validity90* ctx, byte_array ** cert);

int validity90_get_driver_ecdsa_private_key(validity90* ctx, byte_array ** key);

int validity90_get_device_ecdh_public_key(validity90* ctx, byte_array ** key);


#endif // VALIDITY90_H
