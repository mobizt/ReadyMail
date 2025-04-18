/**
 * The example to add new message to the mailbox.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP  // Allow SMTP class and data
#define ENABLE_IMAP  // Allow IMAP class and data
#define ENABLE_DEBUG // Allow debugging
#define READYMAIL_DEBUG_PORT Serial
#include "ReadyMail.h"

#define IMAP_HOST "_______"
#define IMAP_PORT 993
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

#define READ_ONLY_MODE false
#define AWAIT_MODE true
#define FLAGS ""

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

// The processing status will show here.
void imapCb(IMAPStatus status)
{
    ReadyMail.printf("ReadyMail[imap][%d]%s\n", status.state, status.text.c_str());
    // The status.state is the imap_state enum defined in src/imap/Common.h
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

    imap.connect(IMAP_HOST, IMAP_PORT, imapCb);
    if (!imap.isConnected())
        return;

    imap.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!imap.isAuthenticated())
        return;

    // Select INBOX mailbox.
    imap.select("INBOX", READ_ONLY_MODE);

    SMTPMessage smtp_msg;
    smtp_msg.sender.name = "Fred Foobar";
    smtp_msg.sender.email = "foobar@Blurdybloop.example.COM";
    smtp_msg.subject = "afternoon meeting";
    smtp_msg.addRecipient("Joe Mooch", "mooch@owatagu.example.net");
    smtp_msg.text.content = "Hello Joe, do you think we can meet at 3:30 tomorrow?";
    smtp_msg.text.charSet = "us-ascii";
    smtp_msg.text.transfer_encoding = "7bit";

    // The attachment can be included in the message as normal.

    // Append new message
    imap.append(smtp_msg, FLAGS, "Thu, 16 Jun 2022 12:30:25 -0800 (PST)" /* date */, AWAIT_MODE);
}

void loop()
{
}