/**
 * The example to use WIZnet W5500 SPI Ethernet module and ESP32
 * to connect to SMTP server.
 *
 * The Ethernet libraries are used in this example.
 *
 * For proper network/SSL client and port selection, please see http://bit.ly/437GkRA
 */
#include <Arduino.h>
#include <WiFi.h>

#define ENABLE_SMTP  // Allows SMTP class and data
#define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial
#include <ReadyMail.h>
#include <ESP_SSLClient.h>

#include <Ethernet.h>
#define WIZNET_RESET_PIN 26 // Connect W5500 Reset pin to GPIO 26 of ESP32 (-1 for no reset pin assigned)
#define WIZNET_CS_PIN 5     // Connect W5500 CS pin to GPIO 5 of ESP32
#define WIZNET_MISO_PIN 19  // Connect W5500 MISO pin to GPIO 19 of ESP32
#define WIZNET_MOSI_PIN 23  // Connect W5500 MOSI pin to GPIO 23 of ESP32
#define WIZNET_SCLK_PIN 18  // Connect W5500 SCLK pin to GPIO 18 of ESP32

uint8_t Eth_MAC[] = {0x02, 0xF0, 0x0D, 0xBE, 0xEF, 0x01};

EthernetClient basic_client;

#define SMTP_HOST "_______"
#define SMTP_PORT 465
#define DOMAIN_OR_IP "127.0.0.1"

// https://github.com/mobizt/ESP_SSLClient
ESP_SSLClient ssl_client;
SMTPClient smtp(ssl_client);

void smtpCb(SMTPStatus status)
{
    if (status.progressUpdated)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state, status.filename.c_str(), status.progress);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
}

bool initEthernet()
{
    ReadyMail.printf("Resetting Ethernet Board...\n");

#if defined(WIZNET_CS_PIN)
    Ethernet.init(WIZNET_CS_PIN);
#endif

#if defined(WIZNET_RESET_PIN)
    pinMode(WIZNET_RESET_PIN, OUTPUT);
    digitalWrite(WIZNET_RESET_PIN, HIGH);
    delay(200);
    digitalWrite(WIZNET_RESET_PIN, LOW);
    delay(50);
    digitalWrite(WIZNET_RESET_PIN, HIGH);
#endif

    ReadyMail.printf("Starting Ethernet connection...\n");
    Ethernet.begin(Eth_MAC);

    unsigned long ms = millis();
    while (Ethernet.linkStatus() != LinkON && millis() - ms < 3000)
    {
        delay(100);
    }

    if (Ethernet.linkStatus() == LinkON)
        ReadyMail.printf("Connected with IP: %s\n", Ethernet.localIP().toString().c_str());
    else
        ReadyMail.printf("Can't connect to network\n");

    return Ethernet.linkStatus() == LinkON;
}

void setup()
{
    Serial.begin(115200);
    Serial.println();

    if (!initEthernet())
        return;

    ssl_client.setClient(&basic_client);
    ssl_client.setInsecure();
    ssl_client.setBufferSizes(2048, 1024);
    ssl_client.setDebugLevel(1);

    Serial.println("ReadyMail, version " + String(READYMAIL_VERSION));

    smtp.connect(SMTP_HOST, SMTP_PORT, DOMAIN_OR_IP, smtpCb);
    if (!smtp.isConnected())
        return;
}

void loop()
{
}