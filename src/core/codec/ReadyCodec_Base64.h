#ifndef READY_CODEC_BASE64_H
#define READY_CODEC_BASE64_H

#include "ReadyStream.h"
#include "ReadyCodec_Utils.h"

#if defined(__AVR__)
  #include <avr/pgmspace.h>
#endif

// Default trait for Base64 encoding
struct rd_b64_traits {
#if defined(__AVR__)
  static char read(uint8_t i) {
    static const char map[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    return pgm_read_byte(&map[i]);
  }
#else
  static char read(uint8_t i) {
    static constexpr const char map[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    return map[i];
  }
#endif

  static char* alloc(size_t size) {
    return rd_mem<char *>(size);
  }

  static void free(char** ptr) {
    rd_free(ptr);
  }
};

// Base64 encoder: raw → base64 string
template <typename Traits = rd_b64_traits>
static inline char* rd_b64_enc(const uint8_t* raw, int len) {
  uint8_t count = 0;
  char buffer[3];
  char* encoded = Traits::alloc(len * 4 / 3 + 4);
  int c = 0;

  for (int i = 0; i < len; i++) {
    buffer[count++] = raw[i];
    if (count == 3) {
      encoded[c++] = Traits::read(buffer[0] >> 2);
      encoded[c++] = Traits::read(((buffer[0] & 0x03) << 4) + (buffer[1] >> 4));
      encoded[c++] = Traits::read(((buffer[1] & 0x0F) << 2) + (buffer[2] >> 6));
      encoded[c++] = Traits::read(buffer[2] & 0x3F);
      count = 0;
    }
  }

  if (count > 0) {
    encoded[c++] = Traits::read(buffer[0] >> 2);
    if (count == 1) {
      encoded[c++] = Traits::read((buffer[0] & 0x03) << 4);
      encoded[c++] = '=';
    } else {
      encoded[c++] = Traits::read(((buffer[0] & 0x03) << 4) + (buffer[1] >> 4));
      encoded[c++] = Traits::read((buffer[1] & 0x0F) << 2);
    }
    encoded[c++] = '=';
  }

  encoded[c] = '\0';
  return encoded;
}

// Chunked Base64 encoder: reads 57 bytes, returns 76-char line
template <typename Traits = rd_b64_traits>
static inline String rd_b64_encode_chunk(src_data_ctx& ctx, int& index) {
  uint8_t raw[57];
  int len = 0;

  ctx.seek(index);
  while (ctx.available() && len < 57) {
    raw[len++] = ctx.read();
    index++;
  }

  char* enc = rd_b64_enc<Traits>(raw, len);
  String out = enc;
  Traits::free(&enc);

  ctx.lines_encoded++;
  if ((int)out.length() > ctx.max_line_length)
    ctx.max_line_length = out.length();

  return out;
}

// Base64 character → 6-bit value (for decoding)
static inline uint8_t rd_b64_lookup(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';         // 0–25
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;    // 26–51
  if (c >= '0' && c <= '9') return c - '0' + 52;    // 52–61
  if (c == '+') return 62;
  if (c == '/') return 63;
  return 255;  // invalid
}

#endif // READY_CODEC_BASE64_H
