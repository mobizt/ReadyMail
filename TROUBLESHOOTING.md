# 🧯 TROUBLESHOOTING — ReadyMail

This document outlines known issues across supported platforms and provides solutions or workarounds to help developers resolve common problems when using ReadyMail.

---

## 📚 Table of Contents

- [ESP8266 Issues](#⚠️-esp8266-issues)
- [ESP32 Issues](#⚠️-esp32-issues)
- [ESP_SSLClient Issues](#⚠️-esp_sslclient-issues)
- [IMAP Response Parsing](#⚠️-imap-response-parsing)

---

## ⚠️ ESP8266 Issues

### 🔧 Buffer Configuration

ESP8266's `WiFiClientSecure` requires buffer tuning to avoid crashes during SSL connections.

```cpp
ssl_client.setBufferSizes(1024, 1024);
```

This reduces memory usage and enables SSL fragmentation.

### 🧠 Heap Configuration

Ensure proper MMU settings:

- Arduino IDE:  
  `Tools > MMU: 16KB cache + 48 KB IRAM and 2nd Heap (shared)`
- PlatformIO:  
  ```ini
  build_flags = -D PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48_SECHEAP_SHARED
  ```

### ⚡ Power Supply

Use a stable, low-noise power source with short, low-impedance cables to prevent brownouts during SSL handshake.

---

## ⚠️ ESP32 Issues

### 🐞 Plain Mode Hang (v3.x)

Calling `setPlainStart()` on `WiFiClientSecure` in ESP32 v3.x may cause `connected()` to hang for 30 seconds due to lwIP select bug.

**Workaround:** Avoid using plain mode or wait for upstream fix.

### 🔥 SSL/Plain Conflict

If `setPlainStart()` is called while still connected in SSL mode, data corruption or unexpected errors may occur.

**Recommendation:** Use SSL mode only unless protocol upgrade is explicitly required.

---

## ⚠️ ESP_SSLClient Issues

### 🧠 Memory Allocation Failures

On devices like Renesas (UNO R4 WiFi) or SAMD (MKR WiFi 1010), TLS handshake may fail due to limited RAM.

**Solution:**

```cpp
ssl_client.setBufferSizes(1024, 1024);
```

Also ensure:

```cpp
ssl_client.setClient(&basic_client);
ssl_client.setInsecure();
```

### 🐢 Compilation Slowness

Due to BearSSL's large C codebase, compilation may be slow in Arduino IDE. Consider disabling antivirus interference or using PlatformIO.

---

## ⚠️ IMAP Response Parsing

### 🧩 Chunked Header Limitations

Some IMAP servers report incorrect decoded sizes for `text/plain` and `text/html`, causing `fileSize = 0`.

**Impact:** Progress tracking and chunk indexing may be unreliable.

**Recommendation:** Use `fileChunk().isComplete` to detect final chunk and avoid relying on `fileSize`.

### 🧠 Memory Constraints

Parsing large multi-line IMAP responses may fail on low-RAM devices. Consider using `WiFiSSLClient` or limiting fetch size.

---

## 📄 License

MIT License © 2025 Suwatchai K.  
See [LICENSE](LICENSE) for details.
