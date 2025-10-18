# 🔄 Chunk Flow — ReadyMail Codec

This document explains how data flows through the chunked encoding pipeline in ReadyMail. It covers how `src_data_ctx`, `index`, traits, and external buffers interact across encoding modes. All logic is STL-free, malloc-free, and designed for embedded and desktop platforms.

---

## 📦 Chunked Encoding Pipeline

```
[ Raw Data ] → src_data_ctx → encode_chunk(mode) → mode-specific encoder → [ Buffer ] → Sink
```

- `src_data_ctx`: Abstracts input source (RAM, PROGMEM, stream)
- `index`: Tracks read position across chunks
- `encode_chunk()`: Dispatches to mode-specific encoder
- `out`: Caller-managed buffer
- `Sink`: Output target (Serial, WiFiClient, file, etc.)

---

## 🧩 Buffer Lifecycle

```cpp
char outbuf[80];
int index = 0;

while (ctx.available()) {
  int n = encode_chunk(ctx, index, outbuf, sizeof(outbuf));
  if (n > 0) client.write(outbuf, n);
}
```

- Buffer reused across chunks
- `index` advances with each chunk
- `ctx.seek(index)` ensures correct alignment

---

## 🧪 Mode-Specific Chunk Flow

### Base64

```
[57 bytes raw] → rd_b64_enc → [76 chars] + \r\n → outbuf
```

- Uses `rd_b64_encode_chunk()`
- Requires ≥79 bytes in buffer

### Quoted-Printable

```
[raw text] → escape special chars (=XX) → wrap lines → outbuf
```

- Uses `rd_qp_encode_chunk()`
- Handles soft breaks and trailing spaces

### Raw 7bit

```
[ASCII text] → normalize \n → \r\n → outbuf
```

- Uses `rd_chunk_encode_raw()`
- Ensures SMTP compliance

### 8bit / Binary

```
[raw bytes] → direct copy → outbuf
```

- Uses `rd_chunk_write_8bit()` or `rd_chunk_write_binary()`
- No line wrapping or CRLF

---

## 📐 Chunk Size Guidelines

| Mode         | Max Input | Max Output | Buffer Required |
|--------------|-----------|------------|------------------|
| Base64       | 57 bytes  | 76 + 2     | ≥79 bytes        |
| QP           | ~60 bytes | ≤76 + 2    | ≥80 bytes        |
| Raw 7bit     | ≤997      | ≤999       | ≥1000 bytes      |
| 8bit/Binary  | Any       | Any        | Caller-defined   |

---

## 🛠 Traits Integration

```cpp
template <>
struct rd_chunk_traits<base64_mode> {
  static constexpr size_t required_buffer_size = 79;
};
```

Use with:

```cpp
static_assert(out_size >= rd_chunk_traits<base64_mode>::required_buffer_size, "Buffer too small");
```

---

## 🧪 Diagnostic Hooks

Enable chunk profiling:

```cpp
ctx.lines_encoded++;
if (len > ctx.max_line_length)
  ctx.max_line_length = len;
```

Track:
- Line count
- Max line length
- Chunk alignment

---

