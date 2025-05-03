/**
 * The example to fetch the latest message in the INBOX.
 * If you want to download the content to file, see Download.ino.
 * 
 * For proper network/SSL client and port selection, please see http://bit.ly/437GkRA
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_IMAP  // Allows IMAP class and data
#define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial
#include "ReadyMail.h"

#define IMAP_HOST "_______"
#define IMAP_PORT 993
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

#define READ_ONLY_MODE true
#define AWAIT_MODE true
#define MAX_CONTENT_SIZE 1024 * 1024 // Maximum size in bytes of the body parts (text and attachment) to be downloaded.

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

// For more information, see https://bit.ly/3RH9ock
void imapCb(IMAPStatus status)
{
    ReadyMail.printf("ReadyMail[imap][%d]%s\n", status.state, status.text.c_str());
}

void showMailboxInfo(MailboxInfo info)
{
    ReadyMail.printf("Info of the selected mailbox\nTotal Messages: %d\n", info.msgCount);
    ReadyMail.printf("UID Validity: %d\n", info.UIDValidity);
    ReadyMail.printf("Predicted next UID: %d\n", info.nextUID);
    if (info.UnseenIndex > 0)
        ReadyMail.printf("First Unseen Message Number: %d\n", info.UnseenIndex);
    else
        ReadyMail.printf("Unseen Messages: No\n");

    if (info.noModseq)
        ReadyMail.printf("Highest Modification Sequence: %llu\n", info.highestModseq);
    for (size_t i = 0; i < info.flags.size(); i++)
        ReadyMail.printf("%s%s%s", i == 0 ? "Flags: " : ", ", info.flags[i].c_str(), i == info.flags.size() - 1 ? "\n" : "");

    if (info.permanentFlags.size())
    {
        for (size_t i = 0; i < info.permanentFlags.size(); i++)
            ReadyMail.printf("%s%s%s", i == 0 ? "Permanent Flags: " : ", ", info.permanentFlags[i].c_str(), i == info.permanentFlags.size() - 1 ? "\n" : "");
    }
    ReadyMail.printf("\n");
}

// For more information, see https://github.com/mobizt/ReadyMail#imap-enveloping-data-and-content-stream
void dataCb(IMAPCallbackData data)
{
    // Showing envelope data.
    if (data.isEnvelope)
    {
        // Additional search info
        if (data.isSearch)
            ReadyMail.printf("Showing Search result %d (%d) of %d from %d\n\n", data.currentMsgIndex + 1, data.msgList[data.currentMsgIndex], data.msgList.size(), data.searchCount);

        // Headers data
        for (int i = 0; i < data.header.size(); i++)
            ReadyMail.printf("%s: %s\n%s", data.header[i].first.c_str(), data.header[i].second.c_str(), i == data.header.size() - 1 ? "\n" : "");
    }
    // Processing content stream.
    else
    {
        // Showing the text content
        if (strcmp(data.mime, "text/html") == 0 || strcmp(data.mime, "text/plain") == 0)
        {
            if (data.dataIndex == 0) // Fist chunk
                Serial.println("------------------");
            Serial.print((char *)data.blob);
            if (data.isComplete) // Last chunk
                Serial.println("------------------");
        }
        else
        {
            // Showing the progress of content fetching
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

    Serial.println("ReadyMail, version " + String(READYMAIL_VERSION));

    imap.connect(IMAP_HOST, IMAP_PORT, imapCb);
    if (!imap.isConnected())
        return;

    imap.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!imap.isAuthenticated())
        return;

    // List all mailboxes.
    imap.list();

    for (size_t i = 0; i < imap.mailboxes.size(); i++)
        ReadyMail.printf("Attributes: %s, Delimiter: %s, Name: %s\n%s", imap.mailboxes[i][0].c_str(), imap.mailboxes[i][1].c_str(), imap.mailboxes[i][2].c_str(), (i == imap.mailboxes.size() - 1 ? "\n" : ""));

    // Select INBOX mailbox.
    // If READ_ONLY_MODE is false, the flag /Seen will set to the fetched message.
    imap.select("INBOX", READ_ONLY_MODE);

    // Show the INBOX information.
    showMailboxInfo(imap.getMailbox());

    // Fetch the latest message in INBOX.
    imap.fetch(imap.getMailbox().msgCount, dataCb, NULL /* FileCallback */, AWAIT_MODE, MAX_CONTENT_SIZE);

    // To fetch message with UID, use imap.fetchUID instead.
}

void loop()
{
}