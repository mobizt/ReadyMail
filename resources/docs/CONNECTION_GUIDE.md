# 🌐 CONNECTION GUIDE — ReadyMail

This guide explains how to select the correct ports and SSL/network clients for SMTP and IMAP connections. It covers plain text, SSL, and STARTTLS modes across various Arduino platforms.

---

## 📚 Table of Contents

- [Overview](#-overview)
- [Plain Text Connection](#-plain-text-connection)
- [SSL Connection](#-ssl-connection)
- [STARTTLS (TLS Upgrade)](#-starttls-tls-upgrade)
- [Client Compatibility Matrix](#-client-compatibility-matrix)
- [Why Use ESP_SSLClient?](#-why-use-esp_sslclient)
- [Installation of ESP_SSLClient](#-installation-of-esp_sslclient)
- [Network Clients Integration Examples](#-network-clients-integration-examples)
  - [ESP32 + TinyGsmClient + ESP_SSLClient](#-esp32--tinygsmclient--esp_sslclient)
  - [ESP32 + EThernet + ESP_SSLClient](#-esp32--ethernet--esp_sslclient)
  - [STM32 + Ethernet + ESP_SSLClient](#-stm32--ethernet--esp_sslclient)
  - [Teensy4.1 + NativeEthernet + ESP_SSLClient](#-teensy41--nativeethernet--esp_sslclient)
  - [ESP32 + ETH + NetworkClientSecure](#-esp32--eth--networkclientsecure)
  - [ESP8266 + LwipEthernet + WiFiClientSecure](#-esp8266--lwipethernet--wificlientsecure)
  - [ESP32 + PPP + NetworkClientSecure](#🧠-esp32--ppp--networkclientsecure)
- [Dynamic Port Switching](#-dynamic-port-switching)
- [License](#-license)

---

## 📖 Overview

Different email servers require different connection modes:

| Mode       | Security     | Common Ports         | Notes                              |
|------------|--------------|----------------------|-------------------------------------|
| Plain Text | ❌ None       | 25 (SMTP), 143 (IMAP)| Often blocked by ISPs              |
| SSL        | ✅ SSL        | 465 (SMTP), 993 (IMAP)| Most reliable                       |
| STARTTLS   | ✅ TLS upgrade| 587 (SMTP), 143 (IMAP)| Requires upgrade-capable client    |

> Not all SSL clients support STARTTLS. Choose carefully based on your board and library.

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

Use `WiFiClientSecure`, `WiFiSSLClient`, `ESP_SSLClient`, or `NetworkClientSecure`. Most servers support this mode.

### SMTP (Port 465)

```cpp
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);
ssl_client.setInsecure(); // For testing only — use setRootCA() in production
smtp.connect("smtp.example.com", 465, statusCallback);
```

### IMAP (Port 993)

```cpp
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);
ssl_client.setInsecure(); // For testing only — use setRootCA() in production
imap.connect("imap.example.com", 993, statusCallback);
```

---

## 🔄 STARTTLS (TLS Upgrade)

Requires SSL client that supports protocol upgrade:

✅ ESP32 v3.x `WiFiClientSecure`  
✅ `ESP_SSLClient` (cross-platform)

### SMTP (Port 587) — ESP_SSLClient

```cpp
#include <WiFiClient.h>
#include <ESP_SSLClient.h>

WiFiClient basic_client;
ESP_SSLClient ssl_client;
ssl_client.setClient(&basic_client, false); // Start in plain mode
ssl_client.setInsecure(); // For testing only — use setRootCA() in production

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
ssl_client.setInsecure(); // For testing only — use setRootCA() in production
ssl_client.setPlainStart();

auto startTLSCallback = [](bool &success) {
  success = ssl_client.startTLS();
};

SMTPClient smtp(ssl_client, startTLSCallback, true /* startTLS */);
smtp.connect("smtp.example.com", 587, statusCallback);
```

---

## 📊 Client Compatibility Matrix

| Client Type                    | SSL | STARTTLS | Plain | Network Type | Notes                                      |
|--------------------------------|-----|----------|-------|--------------|--------------------------------------------|
| WiFiClientSecure (ESP8266)     | ✅  | ❌       | ❌    | WiFi         | SSL only                                   |
| NetworkClientSecure (ESP32 v3)    | ✅  | ✅       | ✅    | WiFi         | Use `setPlainStart()`, [STARTTLS issue](https://github.com/mobizt/ReadyMail/blob/main/resources/docs/TROUBLESHOOTING.md?tab=readme-ov-file#%EF%B8%8F-esp32-issues)                      |
| [`ESP_SSLClient`](https://github.com/mobizt/ESP_SSLClient)                  | ✅  | ✅       | ✅    | Any          | `Cross-platform, simplified APIs*, supports PSRAM`             |
| OPEnSLab-OSU' s SSLClient                  | ✅  | ❌       | ❌    | Any          | complicated APIs, buggy 
| govorox's SSLClient                  | ✅  | ❌       | ❌    | Any          | ESP32 only 
| WiFiSSLClient                  | ✅  | ❌       | ❌    | WiFi         | Lightweight, SSL only                      |
| WiFiClient                     | ❌  | ❌       | ✅    | WiFi         | Plain only                                 |
| GSMClient                     | ❌  | ❌       | ✅    | GSM         | Plain only                                 |
| EthernetClient                     | ❌  | ❌       | ✅    | Ethernet         | Plain only                                 |
| WiFiClient + [`ESP_SSLClient`](https://github.com/mobizt/ESP_SSLClient)| ✅  | ✅       | ✅    | WiFi     | Suitable for all WiFiClient libraries on all WiFi-capable devices     
| GSMClient + [`ESP_SSLClient`](https://github.com/mobizt/ESP_SSLClient)      | ✅  | ✅       | ✅    | GSM          | GSM modules via wrapper                    |
| EthernetClient + [`ESP_SSLClient`](https://github.com/mobizt/ESP_SSLClient)| ✅  | ✅       | ✅    | Ethernet     | W5500 or ENC28J60 via wrapper              |
| PPP + NetworkClientSecure| ✅  | ❌       | ❌    | PPP/GSM  | ESP32 only — required for PPP modem support|


---

## 🧠 Why Use ESP_SSLClient?

[`ESP_SSLClient`](https://github.com/mobizt/ESP_SSLClient) offers several advantages over other SSL libraries:

- 🔄 **Cross-platform**  
  Works on ESP32, STM32, RP2040, SAMD, Renesas, and more

- 🔐 **Full SSL/TLS support**  
  TLS 1.2, root CA, fingerprint

- 🧩 **Universal client compatibility**  
  Supports `WiFiClient`, `GSMClient`, `EthernetClient`

- 🧠 **PSRAM-friendly**  
  Optimized for large buffers and secure streaming

- ⚙️ **Buffer tuning for low-memory devices**  
  Allows manual configuration of RX/TX buffer sizes via `setBufferSizes(rx, tx)`  
  Ideal for boards with limited RAM or when sending large data e.g., camera image

  ```cpp
  ssl_client.setBufferSizes(2048, 1024); // RX = 2KB, TX = 1KB
  ```

- 🧰 **Simple API**  
  Similar to `WiFiClientSecure` on ESP8266


---

## 📦 Installation of ESP_SSLClient

Install via PlatformIO:

```ini
lib_deps =
  mobizt/ESP_SSLClient
```

Or via Arduino IDE:

1. Download from [ESP_SSLClient GitHub](https://github.com/mobizt/ESP_SSLClient)  
2. Add to `libraries/ESP_SSLClient` folder

---

## 📡 Network Clients Integration Examples

The following examples demonstrate how to use network clients (WiFi and Ethernet) in combination with SSL clients for secure communication over SSL/TLS. For examples using STARTTLS, please refer to the [Client Compatibility Matrix](#-client-compatibility-matrix) and [Dynamic Port Switching](#-dynamic-port-switching).

---

### 🧠 ESP32 + TinyGsmClient + ESP_SSLClient

| LilyGO T-A7670 Pin | ESP32 GPIO | Signal Description |
|--------------------|------------|---------------------|
| RST                | GPIO5      | Module Reset        |
| TX                 | GPIO26     | ESP32 RX (Receive)  |
| RX                 | GPIO27     | ESP32 TX (Transmit) |


```cpp

#define TINY_GSM_MODEM_SIM7600 // SIMA7670 Compatible with SIM7600 AT instructions

// For network independent usage (disable all network features).
// #define DISABLE_NERWORKS

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
const char apn[] = "YourAPN";
const char gprsUser[] = "";
const char gprsPass[] = "";

#define UART_BAUD 115200

// LilyGO TTGO T-A7670 development board (ESP32 with SIMCom A7670)
#define SIM_MODEM_RST 5
#define SIM_MODEM_RST_LOW true // active LOW
#define SIM_MODEM_RST_DELAY 200
#define SIM_MODEM_TX 26
#define SIM_MODEM_RX 27

#include <Arduino.h>
// https://github.com/vshymanskyy/TinyGSM
#include <TinyGsmClient.h>
#include <ESP_SSLClient.h>

#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>

TinyGsm modem(SerialAT);
TinyGsmClient gsm_client(modem, 0)
ESP_SSLClient ssl_client;
SMTPClient smtp(ssl_client);

bool initModem()
{
    SerialMon.begin(115200);
    delay(10);

    // Resetting the modem
#if defined(SIM_MODEM_RST)
    pinMode(SIM_MODEM_RST, SIM_MODEM_RST_LOW ? OUTPUT_OPEN_DRAIN : OUTPUT);
    digitalWrite(SIM_MODEM_RST, SIM_MODEM_RST_LOW);
    delay(100);
    digitalWrite(SIM_MODEM_RST, !SIM_MODEM_RST_LOW);
    delay(3000);
    digitalWrite(SIM_MODEM_RST, SIM_MODEM_RST_LOW);
#endif

    DBG("Wait...");
    delay(3000);

    SerialAT.begin(UART_BAUD, SERIAL_8N1, SIM_MODEM_RX, SIM_MODEM_TX);

    DBG("Initializing modem...");
    if (!modem.init())
    {
        DBG("Failed to restart modem, delaying 10s and retrying");
        return false;
    }

    /**
     * 2 Automatic
     * 13 GSM Only
     * 14 WCDMA Only
     * 38 LTE Only
     */
    modem.setNetworkMode(38);
    if (modem.waitResponse(10000L) != 1)
    {
        DBG(" setNetworkMode faill");
        return false;
    }

    String name = modem.getModemName();
    DBG("Modem Name:", name);

    String modemInfo = modem.getModemInfo();
    DBG("Modem Info:", modemInfo);

    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork())
    {
        SerialMon.println(" fail");
        delay(10000);
        return false;
    }
    SerialMon.println(" success");

    if (modem.isNetworkConnected())
        SerialMon.println("Network connected");

    return true;
}

void setup()
{
    Serial.begin(115200);

    if (!initModem())
        return;

    ssl_client.setClient(&gsm_client);
    ssl_client.setInsecure(); // For testing only — use setRootCA() in production
    ssl_client.setDebugLevel(1);
    ssl_client.setBufferSizes(1048 /* rx */, 1024 /* tx */);

    smtp.connect("smtp.example.com", 465);
    smtp.authenticate("user@example.com", "password", readymail_auth_password);

    SMTPMessage msg;
    msg.headers.add(rfc822_from, "ReadyMail <user@example.com>");
    msg.headers.add(rfc822_to, "Recipient <recipient@example.com>");
    msg.headers.add(rfc822_subject, "PPP Email");
    msg.text.body("Sent via PPPClient + NetworkClientSecure");
    
    configTime(0, 0, "pool.ntp.org");
    while (time(nullptr) < 100000) delay(100);
    msg.timestamp = time(nullptr);
    
    smtp.send(msg);
}

void loop()
{
}
```

---

### 🧠 ESP32 + EThernet + ESP_SSLClient

| W5500 Pin   | ESP32 GPIO | Signal Description     |
|-------------|------------|-------------------------|
| RESET       | GPIO26     | Module Reset            |
| CS          | GPIO5      | Chip Select (SPI SS)    |
| MISO        | GPIO19     | Master In Slave Out     |
| MOSI        | GPIO23     | Master Out Slave In     |
| SCLK        | GPIO18     | SPI Clock               |


```cpp
#include <Arduino.h>
#include <Ethernet.h>
#include <ESP_SSLClient.h>

#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>

#define WIZNET_RESET_PIN 26 // -1 for no reset pin assigned
#define WIZNET_CS_PIN 5
#define WIZNET_MISO_PIN 19
#define WIZNET_MOSI_PIN 23
#define WIZNET_SCLK_PIN 18

uint8_t Eth_MAC[] = {0x02, 0xF0, 0x0D, 0xBE, 0xEF, 0x01};

EthernetClient eth_client;
ESP_SSLClient ssl_client
SMTPClient smtp(ssl_client);

bool connectEthernet()
{
    Serial.println("Resetting Ethernet Board...");

    pinMode(WIZNET_RESET_PIN, OUTPUT);
    digitalWrite(WIZNET_RESET_PIN, HIGH);
    delay(200);
    digitalWrite(WIZNET_RESET_PIN, LOW);
    delay(50);
    digitalWrite(WIZNET_RESET_PIN, HIGH);
    delay(200);
    Serial.println("Starting Ethernet connection...");
    Ethernet.begin(Eth_MAC);

    unsigned long to = millis();

    while (Ethernet.linkStatus() != LinkON && millis() - to < 2000)
    {
        delay(100);
    }

    if (Ethernet.linkStatus() == LinkON)
    {
        Serial.print("Connected with IP ");
        Serial.println(Ethernet.localIP());
        return true;
    }
    else
        Serial.println("Can't connected");

    return false;
}

void setup()
{
    Serial.begin(115200);

    if (!connectEthernet())
        return;

    ssl_client.setClient(&eth_client);
    ssl_client.setInsecure(); // For testing only — use setRootCA() in production
    ssl_client.setDebugLevel(1);
    ssl_client.setBufferSizes(1048 /* rx */, 1024 /* tx */);

    smtp.connect("smtp.example.com", 465);
    smtp.authenticate("user@example.com", "password", readymail_auth_password);

    SMTPMessage msg;
    msg.headers.add(rfc822_from, "ReadyMail <user@example.com>");
    msg.headers.add(rfc822_to, "Recipient <recipient@example.com>");
    msg.headers.add(rfc822_subject, "PPP Email");
    msg.text.body("Sent via PPPClient + NetworkClientSecure");
    
    configTime(0, 0, "pool.ntp.org");
    while (time(nullptr) < 100000) delay(100);
    msg.timestamp = time(nullptr);
    
    smtp.send(msg);
}

void loop()
{

}
```
---

### 🧠 STM32 + Ethernet + ESP_SSLClient

| W5500 Pin | STM32F103C8T6 Pin | Function         |
|-----------|-------------------|------------------|
| VCC       | 3.3V              | Power supply     |
| GND       | GND               | Ground           |
| SCS (CS)  | PA4               | SPI Chip Select  |
| SCLK      | PA5               | SPI Clock        |
| MISO      | PA6               | SPI MISO         |
| MOSI      | PA7               | SPI MOSI         |
| INT       | PA0 (optional)    | Interrupt        |
| RST       | PB13 (optional)   | Hardware Reset   |

[STM32 ↔ W5500 Wiring Image](/resources/images/stm32_w5500.svg)

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <time.h>
#include <EthernetUdp.h>
#include <NTPClient.h>
#include <ESP_SSLClient.h>

#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>


// Ethernet settings
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// NTP client
EthernetUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 0, 60000); // GMT+0

// Network and SSL clients
EthernetClient eth_client;
ESP_SSLClient ssl_client;

// ReadyMail SMTP client
SMTPClient smtp(ssl_client);

void setup() {
  Serial.begin(115200);

  // Initialize Ethernet
  Ethernet.begin(mac);
  delay(1000);

  // Sync time via NTP
  timeClient.begin();
  timeClient.update();
  time_t now = timeClient.getEpochTime();
  struct timeval tv = { now, 0 };
  settimeofday(&tv, nullptr);

  Serial.print("Synced time: ");
  Serial.println(ctime(&now));

  // Configure SSL client
  ssl_client.setClient(&eth_client);
  ssl_client.setInsecure(); // For testing only — use setRootCA() in production

  // Connect to SMTP server
  smtp.connect("smtp.example.com", 465);
  smtp.authenticate("user@example.com", "password", readymail_auth_password);

  // Compose and send email
  SMTPMessage msg;
  msg.headers.add(rfc822_from, "ReadyMail <user@example.com>");
  msg.headers.add(rfc822_to, "Recipient <recipient@example.com>");
  msg.headers.add(rfc822_subject, "STM32 Ethernet Email");
  msg.text.body("Sent from STM32 + W5500 + ESP_SSLClient with NTP time");

  msg.timestamp = time(nullptr);
  smtp.send(msg);
}

void loop() {
  // Optional: update time periodically
  timeClient.update();
}

```

---

### 🧠 Teensy4.1 + NativeEthernet + ESP_SSLClient

| Teensy 4.1 Pin | Ethernet Kit Pin | Description           |
|----------------|------------------|------------------------|
| Pin 23         | G                | Ground                 |
| Pin 22         | 4                | Data line              |
| Pin 21         | 3                | Data line              |
| Pin 20         | 2                | Data line              |
| Pin 19         | 1                | Data line              |
| Pin 18         | 0                | Data line              |


```cpp
#include <Arduino.h>
#include <SPI.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <ESP_SSLClient.h>

#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; // Custom MAC
unsigned int localPort = 8888;                     // UDP port
const char timeServer[] = "time.nist.gov";         // NTP server
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

ESP_SSLClient ssl_client;
EthernetClient eth_client;
EthernetUDP Udp;

SMTPClient smtp(ssl_client);

unsigned long ntp_ms = 0;
unsigned long timestamp = 0;
bool mail_sent = false;

void sendNTPpacket(const char *address)
{
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;
    packetBuffer[1] = 0;
    packetBuffer[2] = 6;
    packetBuffer[3] = 0xEC;
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    Udp.beginPacket(address, 123);
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}

void getTimestamp()
{
    sendNTPpacket(timeServer);
    delay(1000);

    if (Udp.parsePacket())
    {
        Udp.read(packetBuffer, NTP_PACKET_SIZE);

        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secsSince1900 = (highWord << 16) | lowWord;

        const unsigned long seventyYears = 2208988800UL;
        timestamp = secsSince1900 - seventyYears - millis();
    }
}

void setup()
{
    Serial.begin(115200);

    while (!Serial)
        ; // Wait for Serial Monitor

    if (Ethernet.begin(mac) == 0)
    {
        Serial.println("DHCP failed");
        while (true)
            delay(1);
    }

    Serial.print("Local IP: ");
    Serial.println(Ethernet.localIP());

    Udp.begin(localPort);

    delay(1000);

    ssl_client.setClient(&eth_client);
    ssl_client.setInsecure(); // For testing only — use setRootCA() in production
    ssl_client.setDebugLevel(1);
    ssl_client.setBufferSizes(1048 /* rx */, 1024 /* tx */);
}

void loop()
{
    if (millis() - ntp_ms > 60 * 60 * 1000 || ntp_ms == 0)
    {
        ntp_ms = millis();
        getTimestamp();
    }

    if (timestamp > 0 && !mail_sent)
    {
        mail_sent = true;
        smtp.connect("smtp.example.com", 465);
        smtp.authenticate("user@example.com", "password", readymail_auth_password);

        SMTPMessage msg;
        msg.headers.add(rfc822_from, "ReadyMail <user@example.com>");
        msg.headers.add(rfc822_to, "Recipient <recipient@example.com>");
        msg.headers.add(rfc822_subject, "PPP Email");
        msg.text.body("Sent via PPPClient + NetworkClientSecure");

        msg.timestamp = timestamp + millis();

        smtp.send(msg);
    }

    Ethernet.maintain();
}
```

---

### 🧠 ESP32 + ETH + NetworkClientSecure

| ESP32 Pin        | Signal Name           | LAN8720 Pin / Function             | Notes              |
|------------------|------------------------|------------------------------------|---------------------|
| GPIO17           | EMAC_CLK_OUT_180       | XTAL1 / CLKIN (nINT/REFCLK)        | 4.7kΩ Pulldown      |
| GPIO22           | EMAC_TXD1              | TX1                                |                     |
| GPIO19           | EMAC_TXD0              | TX0                                |                     |
| GPIO21           | EMAC_TX_EN             | TX_EN                              |                     |
| GPIO26           | EMAC_RXD1              | RX1                                |                     |
| GPIO25           | EMAC_RXD0              | RX0                                |                     |
| GPIO27           | EMAC_RX_DV             | CRS                                |                     |
| GPIO23           | MDC                    | MDC                                |                     |
| GPIO18           | MDIO                   | MDIO                               |                     |
| GND              | Ground                 | GND                                | Common ground       |
| 3V3              | Power Supply           | VCC                                | 3.3V regulated      |


> 🔗 Reference: [LAN8720 modified board](/resources/images/lan8720_modified_board.png) and [LAN8720 modified schematic](/resources/images/lan8720_modified_schematic.png)

```cpp

#include <Arduino.h>
#include <ETH.h>
#include <NetworkClientSecure.h>
#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>

#ifdef ETH_CLK_MODE
#undef ETH_CLK_MODE
#endif

// Type of the Ethernet PHY (LAN8720 or TLK110)
#define ETH_PHY_TYPE ETH_PHY_LAN8720

// I²C-address of Ethernet PHY (0 or 1 for LAN8720, 31 for TLK110)
#define ETH_PHY_ADDR 1

// Pin# of the I²C clock signal for the Ethernet PHY
#define ETH_PHY_MDC 23

// Pin# of the I²C IO signal for the Ethernet PHY
#define ETH_PHY_MDIO 18

// Pin# of the enable signal for the external crystal oscillator (-1 to disable)
#define ETH_PHY_POWER -1

// RMII clock output from GPIO17 (for modified LAN8720 module only)
// For LAN8720 built-in, RMII clock input at GPIO 0 from LAN8720 e.g. Olimex ESP32-EVB board
// #define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT

static bool eth_connected = false;
NetworkClientSecure ssl_client;
SMTPClient smtp(ssl_client);

void WiFiEvent(WiFiEvent_t event)
{
    // Do not run any function here to prevent stack overflow or nested interrupt
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)

    switch (event)
    {
    case ARDUINO_EVENT_ETH_START:
        Serial.println("ETH Started");
        // set eth hostname here
        ETH.setHostname("esp32-ethernet");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.print("ETH MAC: ");
        Serial.print(ETH.macAddress());
        Serial.print(", IPv4: ");
        Serial.print(ETH.localIP());
        if (ETH.fullDuplex())
        {
            Serial.print(", FULL_DUPLEX");
        }
        Serial.print(", ");
        Serial.print(ETH.linkSpeed());
        Serial.println("Mbps");
        eth_connected = true;
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");
        eth_connected = false;
        break;
    case ARDUINO_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        eth_connected = false;
        break;
    default:
        break;
    }

#else
    switch (event)
    {
    case SYSTEM_EVENT_ETH_START:
        Serial.println("ETH Started");
        // set eth hostname here
        ETH.setHostname("esp32-ethernet");
        break;
    case SYSTEM_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected");
        break;
    case SYSTEM_EVENT_ETH_GOT_IP:
        Serial.print("ETH MAC: ");
        Serial.print(ETH.macAddress());
        Serial.print(", IPv4: ");
        Serial.print(ETH.localIP());
        if (ETH.fullDuplex())
        {
            Serial.print(", FULL_DUPLEX");
        }
        Serial.print(", ");
        Serial.print(ETH.linkSpeed());
        Serial.println("Mbps");
        eth_connected = true;
        break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");
        eth_connected = false;
        break;
    case SYSTEM_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        eth_connected = false;
        break;
    default:
        break;
    }
#endif
}

void setup()
{
    Serial.begin(115200);

    // This delay is needed in case ETH_CLK_MODE was set to ETH_CLOCK_GPIO0_IN,
    // to allow the external clock source to be ready before initialize the Ethernet.
    delay(500);

    WiFi.onEvent(WiFiEvent);
    ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);

    while (!eth_connected)
    {
      delay(200);
    }

    ssl_client.setInsecure(); // For testing only — use setRootCA() in production

    smtp.connect("smtp.example.com", 465);
    smtp.authenticate("user@example.com", "password", readymail_auth_password);

    SMTPMessage msg;
    msg.headers.add(rfc822_from, "ReadyMail <user@example.com>");
    msg.headers.add(rfc822_to, "Recipient <recipient@example.com>");
    msg.headers.add(rfc822_subject, "PPP Email");
    msg.text.body("Sent via PPPClient + NetworkClientSecure");
    
    configTime(0, 0, "pool.ntp.org");
    while (time(nullptr) < 100000) delay(100);
    msg.timestamp = time(nullptr);
    
    smtp.send(msg);
}

void loop()
{
}
```
---

### 🧠 ESP8266 + LwipEthernet + WiFiClientSecure

| ESP8266 Pin       | Signal Name | ENC28J60 Pin | Description         |
|-------------------|-------------|--------------|---------------------|
| GPIO12 (D6)       | MISO        | SO           | Master In Slave Out |
| GPIO13 (D7)       | MOSI        | SI           | Master Out Slave In |
| GPIO14 (D5)       | SCK         | SCK          | SPI Clock           |
| GPIO16 (D0)       | CS          | CS           | Chip Select         |
| GND               | Ground      | GND          | Common Ground       |
| 3V3               | Power       | VCC          | 3.3V Supply         |


> 🔗 Reference: [EthClient.ino — esp8266/Arduino](https://github.com/esp8266/Arduino/blob/master/libraries/lwIP_Ethernet/examples/EthClient/EthClient.ino)

```cpp
#include <Arduino.h>
#include <LwipEthernet.h>
#include <WiFiClientSecure.h>
#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>

#define ETH_CS_PIN 16 // D0

WiFiClientSecure ssl_client;

ENC28J60lwIP eth(ETH_CS_PIN);
// Wiznet5100lwIP eth(ETH_CS_PIN);
// Wiznet5500lwIP eth(ETH_CS_PIN);

void setup()
{
    Serial.begin(115200);

     Serial.print("Connecting to Ethernet... ");

    if (!ethInitDHCP(eth))
    {
        Serial.println("no hardware found!");
        while (1)
        {
            delay(1000);
        }
    }

    while (!eth.connected())
    {
        Serial.printf(".");
        delay(500);
    }

    ssl_client.setInsecure(); // For testing only — use setRootCA() in production

    smtp.connect("smtp.example.com", 465);
    smtp.authenticate("user@example.com", "password", readymail_auth_password);

    SMTPMessage msg;
    msg.headers.add(rfc822_from, "ReadyMail <user@example.com>");
    msg.headers.add(rfc822_to, "Recipient <recipient@example.com>");
    msg.headers.add(rfc822_subject, "PPP Email");
    msg.text.body("Sent via PPPClient + NetworkClientSecure");
    
    configTime(0, 0, "pool.ntp.org");
    while (time(nullptr) < 100000) delay(100);
    msg.timestamp = time(nullptr);
    
    smtp.send(msg);
}

void loop()
{
}
```
---

### 🧠 ESP32 + PPP + NetworkClientSecure

| ESP32 GPIO       | PPP Modem Pin | Signal Description         |
|------------------|----------------|-----------------------------|
| GPIO21           | TX             | ESP32 TX → Modem RX         |
| GPIO22           | RX             | ESP32 RX ← Modem TX         |
| GPIO26           | RTS            | Request to Send (ESP32 → Modem) |
| GPIO27           | CTS            | Clear to Send (ESP32 ← Modem)   |
| GPIO25           | RST            | Module Reset                |


```cpp
#include <Arduino.h>
#include <PPP.h>
#include <NetworkClientSecure.h>
#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>

#define PPP_MODEM_APN "YourAPN"
#define PPP_MODEM_PIN "0000" // or NULL

// WaveShare SIM7600 HW Flow Control
#define PPP_MODEM_RST 25
#define PPP_MODEM_RST_LOW false // active HIGH
#define PPP_MODEM_RST_DELAY 200
#define PPP_MODEM_TX 21
#define PPP_MODEM_RX 22
#define PPP_MODEM_RTS 26
#define PPP_MODEM_CTS 27
#define PPP_MODEM_FC ESP_MODEM_FLOW_CONTROL_HW
#define PPP_MODEM_MODEL PPP_MODEM_SIM7600

// LilyGO TTGO T-A7670 development board (ESP32 with SIMCom A7670)
// #define PPP_MODEM_RST 5
// #define PPP_MODEM_RST_LOW true // active LOW
// #define PPP_MODEM_RST_DELAY 200
// #define PPP_MODEM_TX 26
// #define PPP_MODEM_RX 27
// #define PPP_MODEM_RTS -1
// #define PPP_MODEM_CTS -1
// #define PPP_MODEM_FC ESP_MODEM_FLOW_CONTROL_NONE
// #define PPP_MODEM_MODEL PPP_MODEM_SIM7600

// SIM800 basic module with just TX,RX and RST
// #define PPP_MODEM_RST     0
// #define PPP_MODEM_RST_LOW true //active LOW
// #define PPP_MODEM_TX      2
// #define PPP_MODEM_RX      19
// #define PPP_MODEM_RTS     -1
// #define PPP_MODEM_CTS     -1
// #define PPP_MODEM_FC      ESP_MODEM_FLOW_CONTROL_NONE
// #define PPP_MODEM_MODEL   PPP_MODEM_SIM800

NetworkClientSecure ssl_client;

SMTPClient smtp(ssl_client);

void onEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    switch (event)
    {
    case ARDUINO_EVENT_PPP_START:
        Serial.println("PPP Started");
        break;
    case ARDUINO_EVENT_PPP_CONNECTED:
        Serial.println("PPP Connected");
        break;
    case ARDUINO_EVENT_PPP_GOT_IP:
        Serial.println("PPP Got IP");
        break;
    case ARDUINO_EVENT_PPP_LOST_IP:
        Serial.println("PPP Lost IP");
        break;
    case ARDUINO_EVENT_PPP_DISCONNECTED:
        Serial.println("PPP Disconnected");
        break;
    case ARDUINO_EVENT_PPP_STOP:
        Serial.println("PPP Stopped");
        break;
    default:
        break;
    }
}

void setup() {

    Serial.begin(115200);

    // Resetting the modem
#if defined(PPP_MODEM_RST)
    pinMode(PPP_MODEM_RST, PPP_MODEM_RST_LOW ? OUTPUT_OPEN_DRAIN : OUTPUT);
    digitalWrite(PPP_MODEM_RST, PPP_MODEM_RST_LOW);
    delay(100);
    digitalWrite(PPP_MODEM_RST, !PPP_MODEM_RST_LOW);
    delay(3000);
    digitalWrite(PPP_MODEM_RST, PPP_MODEM_RST_LOW);
#endif

    // Listen for modem events
    Network.onEvent(onEvent);

    // Configure the modem
    PPP.setApn(PPP_MODEM_APN);
    PPP.setPin(PPP_MODEM_PIN);
    PPP.setResetPin(PPP_MODEM_RST, PPP_MODEM_RST_LOW, PPP_MODEM_RST_DELAY);
    PPP.setPins(PPP_MODEM_TX, PPP_MODEM_RX, PPP_MODEM_RTS, PPP_MODEM_CTS, PPP_MODEM_FC);

    Serial.println("Starting the modem. It might take a while!");
    PPP.begin(PPP_MODEM_MODEL);

    Serial.print("Manufacturer: ");
    Serial.println(PPP.cmd("AT+CGMI", 10000));
    Serial.print("Model: ");
    Serial.println(PPP.moduleName());
    Serial.print("IMEI: ");
    Serial.println(PPP.IMEI());

    bool attached = PPP.attached();
    if (!attached)
    {
        int i = 0;
        unsigned int s = millis();
        Serial.print("Waiting to connect to network");
        while (!attached && ((++i) < 600))
        {
            Serial.print(".");
            delay(100);
            attached = PPP.attached();
        }
        Serial.print((millis() - s) / 1000.0, 1);
        Serial.println("s");
        attached = PPP.attached();
    }

    Serial.print("Attached: ");
    Serial.println(attached);
    Serial.print("State: ");
    Serial.println(PPP.radioState());
    if (attached)
    {
        Serial.print("Operator: ");
        Serial.println(PPP.operatorName());
        Serial.print("IMSI: ");
        Serial.println(PPP.IMSI());
        Serial.print("RSSI: ");
        Serial.println(PPP.RSSI());
        int ber = PPP.BER();
        if (ber > 0)
        {
            Serial.print("BER: ");
            Serial.println(ber);
            Serial.print("NetMode: ");
            Serial.println(PPP.networkMode());
        }

        Serial.println("Switching to data mode...");
        PPP.mode(ESP_MODEM_MODE_CMUX); // Data and Command mixed mode
        if (!PPP.waitStatusBits(ESP_NETIF_CONNECTED_BIT, 1000))
        {
            Serial.println("Failed to connect to internet!");
        }
        else
        {
            Serial.println("Connected to internet!");
        }
    }
    else
    {
        Serial.println("Failed to connect to network!");
    }

    ssl_client.setInsecure(); // For testing only — use setRootCA() in production

    smtp.connect("smtp.example.com", 465);
    smtp.authenticate("user@example.com", "password", readymail_auth_password);

    SMTPMessage msg;
    msg.headers.add(rfc822_from, "ReadyMail <user@example.com>");
    msg.headers.add(rfc822_to, "Recipient <recipient@example.com>");
    msg.headers.add(rfc822_subject, "PPP Email");
    msg.text.body("Sent via PPPClient + NetworkClientSecure");
    
    configTime(0, 0, "pool.ntp.org");
    while (time(nullptr) < 100000) delay(100);
    msg.timestamp = time(nullptr);

    smtp.send(msg);
}

void loop()
{
}
```

> 🔗 Reference: [PPP_Basic.ino — arduino-esp32](https://github.com/espressif/arduino-esp32/blob/master/libraries/PPP/examples/PPP_Basic/PPP_Basic.ino)

---

## 🔀 Dynamic Port Switching

ReadyMail supports runtime port switching via:

- `AutoPort.ino` — manual switching  
- `AutoClient.ino` — uses ReadyClient wrapper

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

MIT License © 2025 Suwatchai K (Mobizt).  
See [LICENSE](../../LICENSE) for details.
