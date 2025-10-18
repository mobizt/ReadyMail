
#ifndef READY_CODEC_BASE64_H
#define READY_CODEC_BASE64_H

#include "ReadyStream.h"
#include "ReadyCodec_Utils.h"

#if defined(__AVR__)
#include <avr/pgmspace.h>
#endif

// Default trait for Base64 encoding
struct rd_b64_traits
{
#if defined(__AVR__)
  static char read(uint8_t i)
  {
    // Base64 map stored in Flash (PROGMEM) for AVR
    static const char map[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    return pgm_read_byte(&map[i]);
  }
#else
  static char read(uint8_t i)
  {
    // Base64 map for non-AVR platforms
    static constexpr const char map[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    return map[i];
  }
#endif
};

// Base64 encoder: raw → base64 string
template <typename Traits = rd_b64_traits>
static inline int rd_b64_enc(const uint8_t *raw, int len, char *encoded, size_t encoded_size)
{
  uint8_t count = 0;
  char buffer[3];
  int c = 0;

  // Check if the provided buffer is large enough for the worst-case encoding
  size_t required_size = ((size_t)len * 4 + 2) / 3 + 1;
  if (encoded_size < required_size)
    return -1; // Buffer too small

  for (int i = 0; i < len; i++)
  {
    buffer[count++] = raw[i];
    if (count == 3)
    {
      encoded[c++] = Traits::read(buffer[0] >> 2);
      encoded[c++] = Traits::read(((buffer[0] & 0x03) << 4) + (buffer[1] >> 4));
      encoded[c++] = Traits::read(((buffer[1] & 0x0F) << 2) + (buffer[2] >> 6));
      encoded[c++] = Traits::read(buffer[2] & 0x3F);
      count = 0;
    }
  }

  if (count > 0)
  {
    encoded[c++] = Traits::read(buffer[0] >> 2);
    if (count == 1)
    {
      encoded[c++] = Traits::read((buffer[0] & 0x03) << 4);
      encoded[c++] = '=';
    }
    else // count == 2
    {
      encoded[c++] = Traits::read(((buffer[0] & 0x03) << 4) + (buffer[1] >> 4));
      encoded[c++] = Traits::read((buffer[1] & 0x0F) << 2);
    }
    encoded[c++] = '=';
  }

  encoded[c] = '\0';
  return c; // Return length of encoded data
}

// Chunked Base64 encoder: reads 57 bytes, returns 76-char line
template <typename Traits = rd_b64_traits>
static inline int rd_b64_encode_chunk(src_data_ctx &ctx, int &index, char *out, size_t out_size)
{
  // 76 chars (max data) + 2 chars (\r\n) + 1 char (\0) = 79.
  if (out_size < 79)
    return -1; // Requires at least 79 bytes: 76 data + \r\n + null terminator

  uint8_t raw[57];
  int len = 0;

  // SEEK and read up to 57 bytes of raw data
  ctx.seek(index);
  while (ctx.available() && len < 57)
  {
    raw[len++] = ctx.read();
    index++;
  }

  // If no data was read, return -1
  if (len == 0)
    return -1;

  // The max output size for 57 bytes is 76 chars. The 'out' buffer is 79.
  int encoded_len = rd_b64_enc<Traits>(raw, len, out, 77); // Use 77 for the max data space (76 + \0)
  if (encoded_len < 0)
    return -1;

  // Add SMTP mandatory hard break (\r\n) after the encoded data
  int c = encoded_len;
  out[c++] = '\r';
  out[c++] = '\n';
  out[c] = '\0'; // Re-terminate after adding \r\n

  ctx.lines_encoded++;
  if (c > ctx.max_line_length)
    ctx.max_line_length = c;

  return c;
}

static inline char *rd_b64_enc_dynamic(const uint8_t *raw, int len)
{
  if (len <= 0)
    return nullptr;

  size_t required_size = ((size_t)len * 4 + 2) / 3 + 1;

  char *encoded_ptr = rd_mem<char *>(required_size);

  if (encoded_ptr == nullptr)
    return nullptr;

  int actual_len = rd_b64_enc(raw, len, encoded_ptr, required_size);

  if (actual_len < 0)
  {
    rd_free(&encoded_ptr);
    return nullptr;
  }

  return encoded_ptr;
}

// Base64 character → 6-bit value (for decoding)
static inline uint8_t rd_b64_lookup(char c)
{
  if (c >= 'A' && c <= 'Z')
    return c - 'A'; // 0–25
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 26; // 26–51
  if (c >= '0' && c <= '9')
    return c - '0' + 52; // 52–61
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
  return 255; // invalid
}

#endif // READY_CODEC_BASE64_H
