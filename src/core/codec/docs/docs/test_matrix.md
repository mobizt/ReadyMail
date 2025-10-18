# 🧪 Test Matrix — ReadyMail Codec

This document outlines test cases for each encoding mode supported by the ReadyMail codec module. It includes input/output expectations, edge-case coverage, and platform-specific notes. All tests are designed to run on embedded and desktop platforms without STL or dynamic memory.

---

## 📦 Test Coverage Overview

| Mode              | Function                     | RFC        | Max Line | CRLF | Edge Cases Covered |
|-------------------|------------------------------|------------|----------|------|--------------------|
| `xenc_base64`     | `rd_b64_encode_chunk`        | RFC 2045   | 76       | ✅    | 0x00, 0xFF, short tail |
| `xenc_qp`         | `rd_qp_encode_chunk`         | RFC 2047   | 76       | ✅    | =XX, trailing space, soft break |
| `xenc_qp_flowed`  | `rd_qp_encode_chunk_flowed`  | RFC 3676   | 76       | ✅    | flowed lines, space stuffing |
| `xenc_7bit`       | `rd_chunk_encode_raw`        | RFC 5321   | ≤1000    | ✅    | \n normalization, control chars |
| `xenc_8bit`       | `rd_chunk_write_8bit`        | RFC 5321   | ≤1000    | ❌    | 0x00–0xFF passthrough |
| `xenc_binary`     | `rd_chunk_write_binary`      | RFC 3030   | ∞        | ❌    | full binary range |

---

## 🧪 Base64 Tests

| Case              | Input Bytes | Expected Output         |
|------------------|-------------|--------------------------|
| Full chunk        | 57 bytes    | 76 chars + `\r\n`        |
| Short chunk       | 1–56 bytes  | padded Base64 + `\r\n`   |
| Null byte         | includes 0x00 | valid Base64            |
| Max byte          | includes 0xFF | valid Base64            |

---

## 🧪 Quoted-Printable Tests

| Case              | Input Text         | Expected Output         |
|------------------|--------------------|--------------------------|
| Space at end      | `"hello "`         | `"hello=20\r\n"`         |
| Equals sign       | `"a=b"`            | `"a=3Db\r\n"`            |
| Soft break        | long line          | wrapped with `=\r\n`     |
| Non-ASCII         | `0xC2`             | `=C2`                    |

---

## 🧪 QP Flowed Tests

| Case              | Input Text         | Expected Output         |
|------------------|--------------------|--------------------------|
| Flowed line       | `"This is a long "`| `"This is a long \r\n"`  |
| Space stuffing    | `" From here"`     | `"  From here\r\n"`      |
| Trailing space    | `"hello "`         | `"hello=20\r\n"`         |

---

## 🧪 Raw 7bit Tests

| Case              | Input              | Expected Output         |
|------------------|--------------------|--------------------------|
| LF only           | `\n`               | `\r\n`                   |
| CR only           | `\r`               | `\r\n`                   |
| Control chars     | `0x01`             | replaced with `?`       |
| Tab               | `\t`               | preserved                |

---

## 🧪 8bit Tests

| Case              | Input              | Expected Output         |
|------------------|--------------------|--------------------------|
| Null byte         | `0x00`             | `0x00`                   |
| Max byte          | `0xFF`             | `0xFF`                   |
| Mixed ASCII       | `"abc"`            | `"abc"`                  |

---

## 🧪 Binary Tests

| Case              | Input              | Expected Output         |
|------------------|--------------------|--------------------------|
| Full binary       | `0x00–0xFF`        | exact passthrough        |
| No line breaks    | long stream        | unbroken output          |

---

## 🛠 Platform Notes

| Platform | Notes |
|----------|-------|
| AVR      | Use PROGMEM-safe ctx and static buffers |
| ESP32    | Compatible with FreeRTOS and streams    |
| Desktop  | Fully portable, no STL required         |

---

## 🧰 Test Harness Integration

Use `ReadyCodec_TestHarness.ino` to validate:

- Chunk alignment
- Line wrapping
- CRLF normalization
- Round-trip encoding/decoding

Add test cases per mode:

```cpp
void test_base64() {
  src_data_ctx ctx;
  ctx.init_static((const uint8_t*)data, len);
  char outbuf[80];
  int index = 0;
  while (ctx.available()) {
    int n = rd_b64_encode_chunk(ctx, index, outbuf, sizeof(outbuf));
    Serial.write(outbuf, n);
  }
}
```
