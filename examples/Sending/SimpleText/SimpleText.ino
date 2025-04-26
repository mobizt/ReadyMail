/**
 * The example to send simple text message.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP  // Allows SMTP class and data
#define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial
#include "ReadyMail.h"

#define SMTP_HOST "_______"
#define SMTP_PORT 465
#define DOMAIN_OR_IP "127.0.0.1"
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"
#define RECIPIENT_EMAIL "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

#define SSL_MODE true
#define AUTHENTICATION true

// Importance!
// Please see https://github.com/mobizt/ReadyMail#ports-and-clients-selection
WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

// For more information, see https://github.com/mobizt/ReadyMail#smtp-processing-information
void smtpCb(SMTPStatus status)
{
    if (status.progressUpdated)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state, status.filename.c_str(), status.progress);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
}

void setMessageTime(SMTPMessage &msg, const String &date, uint32_t timestamp = 0)
{
    // Three methods to set the message date.
    // Can be selected one of these methods.
    if (date.length())
    {
        msg.addHeader("Date: " + date);
        msg.date = date;
    }
    if (timestamp > 0) // The UNIX timestamp (seconds since Midnight Jan 1, 1970)
        msg.timestamp = timestamp;
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

    ssl_client.setInsecure();

    smtp.connect(SMTP_HOST, SMTP_PORT, DOMAIN_OR_IP, smtpCb, SSL_MODE);
    if (!smtp.isConnected())
        return;

    if (AUTHENTICATION)
    {
        smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
        if (!smtp.isAuthenticated())
            return;
    }

    SMTPMessage msg;
    msg.sender.name = "ReadyMail";
    msg.sender.email = AUTHOR_EMAIL;
    msg.subject = "ReadyMail test message";
    msg.addRecipient("User", RECIPIENT_EMAIL);

    String bodyText = "Hello everyone.\n";
    bodyText += "こんにちは、日本の皆さん\n";
    bodyText += "大家好，中国人\n";
    bodyText += "Здравей български народе\n";
    msg.text.content = bodyText;
    msg.text.transfer_encoding = "base64";
    msg.html.content = "<html><body><div style=\"color:#00ffff;\">" + bodyText + "</div></body></html>";
    msg.html.transfer_encoding = "base64";

    // To prevent mail server spam or junk mail consideration.
    setMessageTime(msg, "Fri, 14 Mar 2025 09:13:23 +0300");
    smtp.send(msg);
}

void loop()
{
}