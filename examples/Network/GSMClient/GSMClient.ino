/**
 * The example to use LilyGO TTGO T-A7670 (ESP32 board with built-in SIMA7670)
 * to connect to SMTP server.
 *
 * The TinyGSMClient and ESP_SSLClient libraries are used in this example.
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

#define TINY_GSM_MODEM_SIM7600
#define SerialMon Serial
#define SerialAT Serial1

#define TINY_GSM_DEBUG SerialMon
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

#define GSM_PIN ""
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
#include <TinyGsmClient.h>

#define SMTP_HOST "_______"
#define SMTP_PORT 465

TinyGsm modem(SerialAT);
TinyGsmClient basic_client(modem);

// https://github.com/mobizt/ESP_SSLClient
ESP_SSLClient ssl_client;
SMTPClient smtp(ssl_client);

void smtpCb(SMTPStatus status)
{
    if (status.progress.available)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state,
                         status.progress.filename.c_str(), status.progress.value);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
}

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
    Serial.println();

    if (!initModem())
        return;

    ssl_client.setClient(&basic_client);
    ssl_client.setInsecure();
    ssl_client.setBufferSizes(2048, 1024);
    ssl_client.setDebugLevel(1);

    Serial.println("ReadyMail, version " + String(READYMAIL_VERSION));

    // In case ESP8266 crashes, please see https://bit.ly/4iX1NkO

    smtp.connect(SMTP_HOST, SMTP_PORT, smtpCb);
    if (!smtp.isConnected())
        return;
}

void loop()
{
}