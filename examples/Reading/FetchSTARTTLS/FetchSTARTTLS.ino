/**
 * The example to fetch the latest message in the INBOX.
 * If you want to download the content to file, see Download.ino.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_IMAP  // Allow IMAP class and data
#define ENABLE_DEBUG // Allow debugging
#define READYMAIL_DEBUG_PORT Serial
#include "ReadyMail.h"

#define IMAP_HOST "_______"
#define IMAP_PORT 143
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

#define READ_ONLY_MODE true
#define AWAIT_MODE true
#define MAX_CONTENT_SIZE 1024 * 1024 // Maximum size in bytes of the body parts (text and attachment) to be downloaded.

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

IMAPClient imap(ssl_client, tlsHandshakeCb, true /* STARTTLS */);

// The processing status will show here.
void imapCb(IMAPStatus status)
{
    ReadyMail.printf("ReadyMail[imap][%d]%s\n", status.state, status.text.c_str());
    // The status.state is the imap_state enum defined in src/imap/Common.h
}

// The fetching (or searching result) message data goes here.
void dataCb(IMAPCallbackData data)
{
    if (data.isEnvelope) // For showing message headers.
    {
        if (data.isSearch)
            ReadyMail.printf("Showing Search result %d (%d) of %d from %d\n\n", data.currentMsgIndex + 1, data.msgList[data.currentMsgIndex], data.msgList.size(), data.searchCount);

        for (int i = 0; i < data.header.size(); i++)
            ReadyMail.printf("%s: %s\n%s", data.header[i].first.c_str(), data.header[i].second.c_str(), i == data.header.size() - 1 ? "\n" : "");
    }
    else // For message body parts
    {
        // For showing text and html contents.
        if (strcmp(data.mime, "text/html") == 0 || strcmp(data.mime, "text/plain") == 0)
        {
            if (data.dataIndex == 0)
                Serial.println("------------------");
            Serial.print((char *)data.blob);
            if (data.isComplete)
                Serial.println("------------------");
        }
        else
        {
            // Other types contents can be processed here.

            // This is just showing the download progress
            // To get the cercentage of data download, use data.progress
            if (data.dataIndex == 0)
                Serial.print("Downloading");
            if (data.progressUpdated)
                Serial.print(".");
            if (data.isComplete)
                Serial.println(" complete");
        }
    }
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

    imap.connect(IMAP_HOST, IMAP_PORT, imapCb);
    if (!imap.isConnected())
        return;

    imap.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!imap.isAuthenticated())
        return;

    // Select INBOX mailbox.
    // If READ_ONLY_MODE is false, the flag /Seen will set to the fetched message.
    imap.select("INBOX", READ_ONLY_MODE);

    // Fetch the latest message in INBOX.
    imap.fetch(imap.getMailbox().msgCount, dataCb, NULL /* FileCallback */, AWAIT_MODE, MAX_CONTENT_SIZE);

    // To fetch message with UID, use imap.fetchUID instead.
}

void loop()
{
}