/**
 * The example to listen to the changes in selected mailbox.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

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

#define IDLE_MODE true
#define IDLE_TIMEOUT 10 * 60 * 1000
#define READ_ONLY_MODE true
#define AWAIT_MODE true
#define MAX_CONTENT_SIZE 1024 * 1024 // Maximum size in bytes of the body parts (text and attachment) to be downloaded.

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

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

    ssl_client.setInsecure();

    imap.connect(IMAP_HOST, IMAP_PORT, imapCb);
    if (!imap.isConnected())
        return;

    imap.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!imap.isAuthenticated())
        return;

    // Select INBOX mailbox.
    imap.select("INBOX", READ_ONLY_MODE);
}

void loop()
{
    imap.loop(IDLE_MODE, IDLE_TIMEOUT);

    if (imap.available())
    {
        ReadyMail.printf("ReadyMail[imap][%d] %s\n", imap.currentState(), imap.idleStatus().c_str());
        // The imap.currentMessage() returns the message number that added/removed or flags updated from idling
        imap.fetch(imap.currentMessage(), dataCb, NULL /* FileCallback */, AWAIT_MODE, MAX_CONTENT_SIZE);

        // Note that, imap.idleStatus() returns the string in the following formats
        // [+] 123456 When the message number 123456 was added to the mailbox or new message is arrived.
        // [-] 123456 When the message number 123456 was removed or deleted from mailbox.
        // [=][/aaa /bbb ] 123456 When the message number 123456 status was changed as the existing flag /aaa and /bbb are assigned
    }
}