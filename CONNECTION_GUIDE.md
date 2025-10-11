# 🌐 CONNECTION GUIDE — ReadyMail

This guide explains how to select the correct ports and SSL/network clients for SMTP and IMAP connections. It covers plain text, SSL, and STARTTLS modes across various Arduino platforms.

---

## 📚 Table of Contents

- [Overview](#📖-overview)
- [Plain Text Connection](#🔓-plain-text-connection)
- [SSL Connection](#🔐-ssl-connection)
- [STARTTLS (TLS Upgrade)](#🔄-starttls-tls-upgrade)
- [Client Compatibility Matrix](#📊-client-compatibility-matrix)
- [Dynamic Port Switching](#🔀-dynamic-port-switching)

---

## 📖 Overview

Different email servers require different connection modes:

| Mode       | Security | Common Ports | Notes |
|------------|----------|--------------|-------|
| Plain Text | ❌ None   | 25 (SMTP), 143 (IMAP) | Often blocked by ISPs |
| SSL        | ✅ SSL    | 465 (SMTP), 993 (IMAP) | Most reliable |
| STARTTLS   | ✅ TLS upgrade | 587 (SMTP), 143 (IMAP) | Requires upgrade-capable client |

Not all SSL clients support STARTTLS. Choose carefully based on your board and library.

---

## 🔓 Plain Text Connection

Use basic `WiFiClient`, `EthernetClient`, or `GSMClient`. Avoid for production use.

### SMTP (Port 25)

```cpp
#include <WiFiClient.h>
WiFiClient basic_client;
SMTPClient smtp(basic_client);
smtp.connect("smtp.example.com", 25, statusCallback, false /* non-secure */);
```

### IMAP (Port 143)

```cpp
#include <WiFiClient.h>
WiFiClient basic_client;
IMAPClient imap(basic_client);
imap.connect("imap.example.com", 143, statusCallback, false /* non-secure */);
```

---

## 🔐 SSL Connection

Use `WiFiClientSecure`, `WiFiSSLClient`, or `ESP_SSLClient`. Most servers support this mode.

### SMTP (Port 465)

```cpp
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);
ssl_client.setInsecure();
smtp.connect("smtp.example.com", 465, statusCallback);
```

### IMAP (Port 993)

```cpp
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);
ssl_client.setInsecure();
imap.connect("imap.example.com", 993, statusCallback);
```

---

## 🔄 STARTTLS (TLS Upgrade)

Requires SSL client that supports protocol upgrade:

- ✅ ESP32 v3.x `WiFiClientSecure`
- ✅ [`ESP_SSLClient`](https://github.com/mobizt/ESP_SSLClient) (cross-platform)

### SMTP (Port 587) — ESP_SSLClient

```cpp
#include <WiFiClient.h>
#include <ESP_SSLClient.h>

WiFiClient basic_client;
ESP_SSLClient ssl_client;
ssl_client.setClient(&basic_client, false); // Start in plain mode
ssl_client.setInsecure();

auto startTLSCallback = [](bool &success) {
  success = ssl_client.connectSSL();
};

SMTPClient smtp(ssl_client, startTLSCallback, true /* startTLS */);
smtp.connect("smtp.example.com", 587, statusCallback);
```

### IMAP (Port 143) — ESP_SSLClient

```cpp
IMAPClient imap(ssl_client, startTLSCallback, true /* startTLS */);
imap.connect("imap.example.com", 143, statusCallback);
```

### SMTP (Port 587) — ESP32 v3 WiFiClientSecure

```cpp
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
ssl_client.setInsecure();
ssl_client.setPlainStart();

auto startTLSCallback = [](bool &success) {
  success = ssl_client.startTLS();
};

SMTPClient smtp(ssl_client, startTLSCallback, true /* startTLS */);
smtp.connect("smtp.example.com", 587, statusCallback);
```

---

## 📊 Client Compatibility Matrix

| Client              | SSL | STARTTLS | Plain | Notes |
|---------------------|-----|----------|-------|-------|
| WiFiClientSecure (ESP8266) | ✅ | ❌ | ❌ | SSL only |
| WiFiClientSecure (ESP32 v3) | ✅ | ✅ | ✅ | Use `setPlainStart()` |
| [`ESP_SSLClient`](https://github.com/mobizt/ESP_SSLClient)       | ✅ | ✅ | ✅ | Cross-platform, supports PSRAM |
| WiFiSSLClient       | ✅ | ❌ | ❌ | Lightweight, SSL only |
| WiFiClient          | ❌ | ❌ | ✅ | Plain only |

---

## 🔀 Dynamic Port Switching

ReadyMail supports runtime port switching via:

- [`AutoPort.ino`](/examples/Network/AutoPort/AutoPort.ino) — manual switching
- [`AutoClient.ino`](/examples/Network/AutoClient/AutoClient.ino) — uses `ReadyClient` wrapper

Example:

```cpp
ReadyClient client;
client.setProtocol(readymail_protocol_smtp);
client.setPort(587);
client.setStartTLS(true);
client.setClient(&basic_client);
client.setInsecure();

SMTPClient smtp(client);
smtp.connect("smtp.example.com", 587, statusCallback);
```

---

## 📄 License

MIT License © 2025 Suwatchai K.  
See [LICENSE](LICENSE) for details.
