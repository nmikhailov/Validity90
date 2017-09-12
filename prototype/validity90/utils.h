#ifndef UTILS_H
#define UTILS_H

#include <glib.h>

typedef struct bstream bstream;

typedef enum bstream_error {
    BSTREAM_NO_ERROR = 0,
    BSTREAM_ERR_INVALID_STREAM = -1,
    BSTREAM_ERR_NO_BYTES_AVAILABLE = -2
} bstream_error;

bstream *bstream_create(const guint8 *data, const gsize data_size);
void bstream_free(bstream *stream);

gsize bstream_remaining(bstream *stream);

bstream_error bstream_read_uint8(bstream *stream, guint8 *res);
bstream_error bstream_read_uint16(bstream *stream, guint16 *res);
bstream_error bstream_read_bytes(bstream *stream, gsize size, guint8 **res);

#endif // UTILS_H
