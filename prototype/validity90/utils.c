#include <string.h>
#include <stdio.h>

#include "utils.h"

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
    if (!stream) {
        return BSTREAM_ERR_INVALID_STREAM;
    }
    return stream->data_size - stream->pos;
}

guint bstream_read_uint8(bstream *stream, guint8 *res) {
    if (!stream) {
        return BSTREAM_ERR_INVALID_STREAM;
    }
    if (stream->pos + 1 < stream->data_size) {
        *res = stream[stream->pos++];
        return BSTREAM_NO_ERROR;
    }
    return BSTREAM_ERR_NO_BYTES_AVAILABLE;
}

guint bstream_read_uint16(bstream *stream, guint16 *res) {
    if (!stream) {
        return BSTREAM_ERR_INVALID_STREAM;
    }
    if (stream->pos + 2 < stream->data_size) {
        *res = stream[stream->pos] | (stream[stream->pos] << 8);
        stream->pos += 2;
        return BSTREAM_NO_ERROR;
    }
    return BSTREAM_ERR_NO_BYTES_AVAILABLE;
}

guint bstream_read_bytes(bstream *stream, gsize size, gpointer *res) {
    if (!stream) {
        return BSTREAM_ERR_INVALID_STREAM;
    }
    if (stream->pos + size < stream->data_size) {
        *res = g_malloc(size);
        memcpy(*res, stream->data + stream->pos, size);
        stream->pos += size;
        return BSTREAM_NO_ERROR;
    }
    return BSTREAM_ERR_NO_BYTES_AVAILABLE;
}
