
![ReadyMail Logo](resources/images/logo.svg)

# ✉️ ReadyMail

**Fast, lightweight, and asynchronous email client library for Arduino.**  
Supports both **SMTP** and **IMAP** with full RFC compliance. Designed for 32-bit MCUs including ESP8266, ESP32, Teensy, Arduino MKR, SAMD, STM32, RP2040, and more.

ReadyMail is designed to be hardware-agnostic. It supports all filesystem types (e.g. SPIFFS, LittleFS, SD) and all network client libraries, including GSMClient, WiFiClient, EthernetClient, and PPP. This ensures seamless integration across diverse hardware and connectivity environments.

![License](https://img.shields.io/badge/license-MIT-blue.svg)  
![Platform](https://img.shields.io/badge/platform-Arduino%2032--bit-green)  
![RFC](https://img.shields.io/badge/RFC-5321%2C%209051%2C%20822-important)

---

## 📚 Table of Contents

- [Features](#-features)
- [Installation](#-installation)
- [Supported Devices](#-supported-devices)
- [RFC Compliance](#-rfc-compliance)
- [Getting Started](#-getting-started)
  - [Sending Email (SMTP)](#-sending-email-smtp)
  - [SMTP Server Rejection and Spam Prevention](#-smtp-server-rejection-and-spam-prevention)
  - [Receiving Email (IMAP)](#-receiving-email-imap)
- [Ports & Clients](#-ports--clients)
- [Known Issues](#-known-issues)
- [License](#-license)
- [Advanced Usage](/resources/docs/ADVANCED.md)
- [Troubleshooting Guide](/resources/docs/TROUBLESHOOTING.md)
- [Connection Guide](/resources/docs/CONNECTION_GUIDE.md)

---

## 🚀 Features

- ✅ Simple and intuitive interface — minimal setup, full control  
- 📎 Supports inline images, attachments (file and nested RFC822 messages), and large static files (text/html).  
- 🔐 SSL/TLS and STARTTLS support  
- 📥 IMAP search, fetch, append, and idle support 
- 🧩 Lightweight and non-blocking design  
- 🧠 RFC-compliant message formatting  
- 🌐 Compatible with all network client libraries: GSM, WiFi, Ethernet, PPP  
- 📁 Supports all Arduino-compatible filesystem types: SD, SDMMC, SPIFFS, LittleFS and more  
- 🧰 Full-featured API for advanced use cases: custom commands, envelope parsing, OTA streaming

---

## 📦 Installation

### 🔧 Arduino IDE

1. Open Arduino IDE  
2. Go to **Sketch > Include Library > Manage Libraries…**  
3. Search for `ReadyMail` and click **Install**

Alternatively, install via ZIP file:

```text
Download the latest release from:
https://github.com/mobizt/ReadyMail/releases

Then go to Sketch > Include Library > Add .ZIP Library…
```

---

### ⚙️ PlatformIO

Add this to your `platformio.ini`:

```ini
lib_deps =
  mobizt/ReadyMail@^0.4.0
```

> ✅ Supports ESP32, STM32, RP2040, SAMD, Renesas and more — except AVR

---

### 📁 Manual Installation

1. Download the source from [ReadyMail GitHub](https://github.com/mobizt/ReadyMail)  
2. Extract to your Arduino `libraries/ReadyMail` folder

---

## 🧩 Supported Devices

- ESP32 / ESP8266  
- STM32 / SAMD  
- RP2040 / Renesas  
- Teensy / Arduino MKR  
> ❌ Not compatible with 8-bit AVR devices

---

## 📖 RFC Compliance

ReadyMail adheres to key email protocol standards:

| Protocol | RFC | Description |
|---------|-----|-------------|
| SMTP    | [RFC 5321](https://www.rfc-editor.org/rfc/rfc5321.html) | Simple Mail Transfer Protocol |
| IMAP    | [RFC 9051](https://datatracker.ietf.org/doc/html/rfc9051) | Internet Message Access Protocol v4rev2 |
| Message Format | [RFC 822](https://www.rfc-editor.org/rfc/rfc822.html) | ARPA Internet Text Messages |

---

## 🛠️ Getting Started

### 📤 Sending Email (SMTP)

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

void setup() {
  Serial.begin(115200);
  WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  ssl_client.setInsecure();

  auto statusCallback = [](SMTPStatus status) {
    Serial.println(status.text);
  };

  smtp.connect("smtp.example.com", 465, statusCallback);

  if (smtp.isConnected()) {
    smtp.authenticate("user@example.com", "password", readymail_auth_password);

    SMTPMessage msg;
    msg.headers.add(rfc822_from, "ReadyMail <user@example.com>");
    msg.headers.add(rfc822_to, "Recipient <recipient@example.com>");
    msg.headers.add(rfc822_subject, "Hello from ReadyMail");
    msg.text.body("This is a plain text message.");
    msg.html.body("<html><body><h1>Hello!</h1></body></html>");

    configTime(0, 0, "pool.ntp.org");
    while (time(nullptr) < 100000) delay(100);
    msg.timestamp = time(nullptr);

    smtp.send(msg);
  }
}

void loop() {}
```

---

## 🛡️ SMTP Server Rejection and Spam Prevention

To ensure successful email delivery and avoid rejection or spam filtering by SMTP servers, follow these best practices:

- Provide a valid EHLO/HELO hostname or IP
- Set the Date header using `msg.timestamp = time(nullptr);`
- Use `\r\n` (CRLF) line breaks instead of `\n` (LF)
- Sync time using `configTime()` or `WiFi.getTime()`

---

### 📥 Receiving Email (IMAP)

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_IMAP
#define ENABLE_DEBUG
#include <ReadyMail.h>

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

void setup() {
  Serial.begin(115200);
  WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  ssl_client.setInsecure();

  auto statusCallback = [](IMAPStatus status) {
    Serial.println(status.text);
  };

  auto dataCallback = [](IMAPCallbackData data) {
    if (data.event() == imap_data_event_fetch_envelope) {
      for (int i = 0; i < data.headerCount(); i++) {
        Serial.printf("%s: %s\n", data.getHeader(i).first.c_str(), data.getHeader(i).second.c_str());
      }
    }
  };

  imap.connect("imap.example.com", 993, statusCallback);

  if (imap.isConnected()) {
    imap.authenticate("user@example.com", "password", readymail_auth_password);
    imap.select("INBOX");
    imap.fetch(imap.getMailbox().msgCount, dataCallback);
  }
}

void loop() {}
```

---

## 🔌 Ports & Clients

| Protocol | Port | Security | Notes |
|---------|------|----------|-------|
| SMTP    | 465  | SSL      | Recommended |
| SMTP    | 587  | STARTTLS | Requires upgrade-capable client |
| IMAP    | 993  | SSL      | Recommended |
| IMAP    | 143  | STARTTLS | Requires upgrade-capable client |

> See [Connection Guide](/resources/docs/CONNECTION_GUIDE.md) for client selection and port compatibility.

---

## ⚠️ Known Issues

- ESP8266 requires buffer tuning via `setBufferSizes()`
- ESP32 v3.x may hang on `setPlainStart()` in plain mode
- Some devices may fail TLS handshake due to memory limits
- If you include `ReadyMail.h` from multiple `.cpp` files, you may get multiple definition linker errors for the global `ReadyMail` instance. To fix this, either:
  - Add `-DREADYMAIL_NO_GLOBAL_INSTANCE` as a compiler flag (recommended — applies to all files), or
  - Add `#define READYMAIL_NO_GLOBAL_INSTANCE` before `#include <ReadyMail.h>` in **every** `.cpp` file that includes the header.

  Then create your own `ReadyMailClass` instance where needed.

> See [Troubleshooting Guide](/resources/docs/TROUBLESHOOTING.md) for detailed solutions and platform-specific workarounds.

---

## 📄 License

MIT License © 2025 Suwatchai K (Mobizt).  
See [LICENSE](LICENSE) for details.

---

## 📘 Advanced Usage

For developers who need deeper control, debugging, or custom command support, see [Advanced Usage](/resources/docs/ADVANCED.md) for:

- SMTP and IMAP status monitoring  
- Custom command responses  
- Envelope and body parsing  
- OTA and file streaming techniques

---

## 🧯 Troubleshooting Guide

If you encounter crashes, SSL handshake failures, or unexpected behavior on ESP8266, ESP32, or Renesas boards, see the [Troubleshooting Guide](/resources/docs/TROUBLESHOOTING.md) for:

- Buffer tuning and heap configuration  
- TLS handshake issues  
- IMAP parsing limitations  
- Platform-specific workarounds

---

## 🌐 Connection Guide

To learn how to select the correct ports and SSL/network clients for your board, see the [Connection Guide](/resources/docs/CONNECTION_GUIDE.md) for:

- SSL vs STARTTLS vs Plain Text  
- Client compatibility matrix  
- Dynamic port switching examples  
- ESP_SSLClient and WiFiClientSecure usage
