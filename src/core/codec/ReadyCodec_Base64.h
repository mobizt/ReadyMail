#ifndef READY_CODEC_BASE64_H
#define READY_CODEC_BASE64_H

#include "ReadyStream.h"
#include "ReadyCodec_Utils.h"

static const unsigned char rd_b64_map[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Lookup table for decoding (used in decoder, optional)
static inline unsigned char rd_b64_lookup(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 71;
    if (c >= '0' && c <= '9') return c + 4;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return 255;
}

// Internal encoder: raw → base64 string
static inline char *rd_b64_enc(const unsigned char *raw, int len) {
    uint8_t count = 0;
    char buffer[3];
    char *encoded = rd_mem<char *>(len * 4 / 3 + 4);
    int c = 0;

    for (int i = 0; i < len; i++) {
        buffer[count++] = raw[i];
        if (count == 3) {
            encoded[c++] = rd_b64_map[buffer[0] >> 2];
            encoded[c++] = rd_b64_map[((buffer[0] & 0x03) << 4) + (buffer[1] >> 4)];
            encoded[c++] = rd_b64_map[((buffer[1] & 0x0F) << 2) + (buffer[2] >> 6)];
            encoded[c++] = rd_b64_map[buffer[2] & 0x3F];
            count = 0;
        }
    }

    if (count > 0) {
        encoded[c++] = rd_b64_map[buffer[0] >> 2];
        if (count == 1) {
            encoded[c++] = rd_b64_map[(buffer[0] & 0x03) << 4];
            encoded[c++] = '=';
        } else if (count == 2) {
            encoded[c++] = rd_b64_map[((buffer[0] & 0x03) << 4) + (buffer[1] >> 4)];
            encoded[c++] = rd_b64_map[(buffer[1] & 0x0F) << 2];
        }
        encoded[c++] = '=';
    }

    encoded[c] = '\0';
    return encoded;
}

// Chunked base64 encoder: reads 57 bytes, returns 76-char line
static inline String rd_b64_encode_chunk(src_data_ctx &ctx, int &index) {
    String raw;
    ctx.seek(index);

    while (ctx.available() && raw.length() < 57) {
        raw += ctx.read();
        index++;
    }

    char *enc = rd_b64_enc(reinterpret_cast<const unsigned char *>(raw.c_str()), raw.length());
    String out = enc;
    rd_free(&enc);

    // Diagnostics
    ctx.lines_encoded++;
    if ((int)out.length() > ctx.max_line_length)
        ctx.max_line_length = out.length();

    return out;
}


#endif // READY_CODEC_BASE64_H