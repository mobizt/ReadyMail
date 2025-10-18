# 🔐 Encoding Modes — ReadyMail Codec

This document outlines the supported encoding modes in the ReadyMail codec module, including RFC compliance, buffer sizing, trait integration, and usage examples. All modes are STL-free, malloc-free, and designed for embedded platforms.

---

## 📦 Mode Overview

| Mode              | RFC         | Max Line Length | CRLF Required | Traits Support | Notes                                 |
|-------------------|-------------|------------------|----------------|----------------|----------------------------------------|
| `xenc_base64`     | RFC 2045    | 76               | ✅             | ✅              | Used for attachments, binary-safe      |
| `xenc_qp`         | RFC 2047    | 76               | ✅             | ✅              | Used for text with special chars       |
| `xenc_qp_flowed`  | RFC 3676    | 76               | ✅             | ✅              | Used for format=flowed text/plain      |
| `xenc_7bit`       | RFC 5321    | ≤1000            | ✅             | ✅              | Raw ASCII with CRLF normalization      |
| `xenc_8bit`       | RFC 5321    | ≤1000            | ❌             | ✅              | Raw 8-bit passthrough                  |
| `xenc_binary`     | RFC 3030    | ∞                | ❌             | ✅              | Binary passthrough, no line breaks     |

---

## 🧩 Trait-Based Buffer Sizing

Each mode defines its required buffer size via traits:

```cpp
template <>
struct rd_chunk_traits<base64_mode> {
  static constexpr size_t required_buffer_size = 79; // 76 + \r\n + \0
};
```

Use with static_assert:

```cpp
static_assert(out_size >= rd_chunk_traits<base64_mode>::required_buffer_size, "Buffer too small");
```

---

## 🧪 Base64 Example

```cpp
char outbuf[80];
int index = 0;
while (ctx.available()) {
  int n = rd_b64_encode_chunk(ctx, index, outbuf, sizeof(outbuf));
  if (n > 0) client.write(outbuf, n);
}
```

---

## 🧪 Quoted-Printable Example

```cpp
char outbuf[80];
int index = 0;
while (ctx.available()) {
  int n = rd_qp_encode_chunk(ctx, index, outbuf, sizeof(outbuf));
  if (n > 0) client.write(outbuf, n);
}
```

---

## 🧪 Raw 7bit Example (SMTP-safe)

```cpp
char outbuf[100];
int index = 0;
while (ctx.available()) {
  int n = rd_chunk_encode_raw(ctx, index, outbuf, sizeof(outbuf));
  if (n > 0) client.write(outbuf, n);
}
```

---

## 🧪 8bit and Binary Example

```cpp
uint8_t outbuf[64];
int index = 0;
while (ctx.available()) {
  size_t n = rd_chunk_write_8bit(ctx, index, outbuf, sizeof(outbuf));
  if (n > 0) client.write(outbuf, n);
}
```

Binary mode uses the same API.

---

