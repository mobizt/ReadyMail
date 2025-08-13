/**
 * The example to use SMTP command to connect, authenticate and send Email.
 *
 * Due to the STARTTLS is also presented in this example, the ESP_SSLClient will be used as SSL client
 * and use WiFiClient for the network client.
 *
 * For ESP32, WiFiClientSecure or NetworkClientSecure can also be used in this example for STARTTLS.
 *
 * For proper network/SSL client and port selection, please see http://bit.ly/437GkRA
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP  // Allows SMTP class and data
#define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial

// If message timestamp and/or Date header was not set,
// the message timestamp will be taken from this source, otherwise
// the default timestamp will be used.
#if defined(ESP32) || defined(ESP8266)
#define READYMAIL_TIME_SOURCE time(nullptr); // Or using WiFi.getTime() in WiFiNINA and WiFi101 firmwares.
#endif

#include <ReadyMail.h>

#define SMTP_HOST "_______"
#define SMTP_PORT 465 // SSL or 587 for STARTTLS
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"
#define RECIPIENT_EMAIL "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

#include <ESP_SSLClient.h>
#include <WiFiClient.h>

// https://github.com/mobizt/ESP_SSLClient
ESP_SSLClient ssl_client;
WiFiClient basic_client;

SMTPClient smtp(ssl_client);

bool startTLSCap = false;
bool plainCap = false;

// For debugging
void smtpCb(SMTPStatus status)
{
    ReadyMail.printf("ReadyMail[dbg][%d]%s\n", status.state, status.text.c_str());
}

void cmdCb(SMTPCommandResponse response)
{
    // The current command can be obtained from status.command.
    // The response.text provides a response line.
    ReadyMail.printf("ReadyMail[cmd][%d] %s\n", smtp.status().state, response.text.c_str());

    // Check for STARTTLS capability
    if (response.text.indexOf("STARTTLS") > -1)
        startTLSCap = true;

    // Check for PLAIN SASL capability.
    if (response.text.indexOf("PLAIN") > -1)
        plainCap = true;
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

    // For ESP_SSLClient
    // if SMTP port is 465, starts in SSL, otherwise starts in plain text
    ssl_client.setClient(&basic_client, SMTP_PORT == 465);

    // For ESP32 WiFiClientSecure
    // If SMTP port is 587, starts in plain text

    // if (SMTP_PORT == 587)
    //  ssl_client.setPlainStart();

    // If server SSL certificate verification was ignored for this ESP32 WiFiClientSecure.
    // To verify root CA or server SSL cerificate,
    // please consult your SSL client documentation.
    ssl_client.setInsecure();

    Serial.println("ReadyMail, version " + String(READYMAIL_VERSION));

    bool esmtp = true;

    // In case ESP8266 crashes, please see https://bit.ly/4iX1NkO

    smtp.connect(SMTP_HOST, SMTP_PORT, cmdCb, smtpCb /* for showing debug info */);
    if (smtp.commandResponse().statusCode != 220)
        return;

    smtp.sendCommand("EHLO 127.0.0.1", cmdCb);
    if (smtp.commandResponse().statusCode != 250)
    {
        // No ESMTP supports
        smtp.sendCommand("HELO 127.0.0.1", cmdCb);
        if (smtp.commandResponse().statusCode != 250)
            return;
        esmtp = false;
    }

    // Find the "STARTTLS" in the response if server supports STARTTLS.
    if (SMTP_PORT == 587 && !startTLSCap)
    {
        // Server does not support STARTTLS, use port 465 for SSL instead.
        return;
    }

    if (SMTP_PORT == 587)
    {
        smtp.sendCommand("STARTTLS", cmdCb);
        if (smtp.commandResponse().statusCode != 220)
            return;

        // Calling SSL client to Start TLS
        // For ESP_SSLClient
        ssl_client.connectSSL();

        // For ESP32 WiFIClientSecure
        // ssl_client.startTLS();

        smtp.sendCommand(esmtp ? "EHLO 127.0.0.1" : "HELO 127.0.0.1", cmdCb);
        if (smtp.commandResponse().statusCode != 250)
            return;
    }

    // Find the "PLAIN" in the response if server supports the PLAIN SASL mechanism
    if (plainCap)
    {
        String cmd = "AUTH PLAIN " + ReadyMail.plainSASLEncode(AUTHOR_EMAIL, AUTHOR_PASSWORD);
        smtp.sendCommand(cmd, cmdCb);
        if (smtp.commandResponse().statusCode != 235)
            return;
    }
    else
    {
        smtp.sendCommand("AUTH LOGIN", cmdCb);
        if (smtp.commandResponse().statusCode != 334)
            return;

        smtp.sendCommand(ReadyMail.base64Encode(AUTHOR_EMAIL), cmdCb);
        if (smtp.commandResponse().statusCode != 334)
            return;

        smtp.sendCommand(ReadyMail.base64Encode(AUTHOR_PASSWORD), cmdCb);
        if (smtp.commandResponse().statusCode != 235)
            return;
    }

    // If client is authenticated, send Email.
    if (smtp.isAuthenticated())
    {
        smtp.sendCommand("MAIL FROM:<" + String(AUTHOR_EMAIL) + ">", cmdCb);
        if (smtp.commandResponse().statusCode != 250)
            return;

        smtp.sendCommand("RCPT TO:<" + String(RECIPIENT_EMAIL) + ">", cmdCb);
        if (smtp.commandResponse().statusCode != 250)
            return;

        smtp.sendCommand("DATA", cmdCb);
        if (smtp.commandResponse().statusCode != 354)
            return;

        smtp.sendData("From: <" + String(AUTHOR_EMAIL) + ">\r\n", cmdCb);
        smtp.sendData("To: <" + String(RECIPIENT_EMAIL) + ">\r\n", cmdCb);

        smtp.sendData("Subject: ReadyMail Test\r\n", cmdCb);
        smtp.sendData("Date: Mon, 05 May 2025 16:51:36 +0300\r\n", cmdCb);
        smtp.sendData("Mime-Version: 1.0\r\n", cmdCb);
        smtp.sendData("Content-Type: text/plain; charset=\"utf-8\";\r\n\r\n", cmdCb);
        smtp.sendData("Hello again!\r\n", cmdCb);

        smtp.sendCommand(".", cmdCb);
        if (smtp.commandResponse().statusCode != 250)
            return;
    }

    smtp.sendCommand("QUIT", cmdCb);
    Serial.println("The Email was sent successfully.");
}

void loop()
{
}