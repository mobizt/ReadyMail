/**
 * The example to send simple text message via the port that supports STARTTLS protocol.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP  // Allow SMTP class and data
#define ENABLE_DEBUG // Allow debugging
#define READYMAIL_DEBUG_PORT Serial
#include "ReadyMail.h"

#define SMTP_HOST "_______"
#define SMTP_PORT 587
#define DOMAIN_OR_IP "127.0.0.1"
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"
#define RECIPIENT_EMAIL "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

// Use the bug fixed version of WiFiClient in ESP32.
#include "WiFiClientImpl.h"
WiFiClientImpl basic_client;

// The SSL Client that support connection upgrades.
// https://github.com/mobizt/ESP_SSLClient
#include <ESP_SSLClient.h>
ESP_SSLClient ssl_client;

// WiFiClientSecure in ESP32 v3.x is now supported STARTTLS,
// to use it, please check its examples.

void tlsHandshakeCb(bool &success) { success = ssl_client.connectSSL(); }

SMTPClient smtp(ssl_client, tlsHandshakeCb, true /* STARTTLS */);

void smtpCb(SMTPStatus status)
{
    if (status.progressUpdated)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% complete\n", status.state, status.filename.c_str(), status.progress);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
    // The status.state is the smtp_state enum defined in src/smtp/Common.h
}

void setup()
{
    Serial.begin(115200);
    Serial.println();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    // This function is specific to ESP_SSLClient only.
    // Set the basic client and the SSL option to false to start connection in plain text mode.
    ssl_client.setClient(&basic_client, false /* start in plain connection */);
    ssl_client.setInsecure();

    smtp.connect(SMTP_HOST, SMTP_PORT /* 587 for STARTTLS */, DOMAIN_OR_IP, smtpCb);
    if (!smtp.isConnected())
        return;

    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!smtp.isAuthenticated())
        return;

    SMTPMessage msg;
    msg.sender.name = "ReadyMail";
    msg.sender.email = AUTHOR_EMAIL;
    msg.subject = "ReadyMail Hello message";
    msg.addRecipient("User", RECIPIENT_EMAIL);

    String bodyText = "Hello everyone.\n";
    msg.text.content = bodyText;
    msg.html.content = "<html><body><div style=\"color:#00ffff;\">" + bodyText + "</div></body></html>";

    smtp.send(msg);
}

void loop()
{
}