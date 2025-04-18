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

uint8_t *content = nullptr;

// The processing status will show here.
void imapCb(IMAPStatus status)
{
    ReadyMail.printf("ReadyMail[imap][%d]%s\n", status.state, status.text.c_str());
    // The status.state is the imap_state enum defined in src/imap/Common.h
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

// This is how you can allocate the memory for storing all chunked data.
void storeChunkedData(const char *filename, const char *mime, const uint8_t *data, int data_index, size_t data_len, size_t total_len, bool complete)
{
    if (!complete)
    {
        // Allocate memory for storing the whole content.
        if (!content)
            content = (uint8_t *)malloc(total_len);

        if (content)
            memcpy(content + data_index, data, data_len);
    }
    else
    {
        // Data is finished.
        // Proocess your data here.
        // The filename and mime type can be used.

        // Free the data
        if (content)
            free(content);
        content = nullptr;
    }
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

            // storeChunkedData(data.filename, data.mime, data.blob, data.dataIndex, data.dataLength, data.size, data.isComplete);
            // Note that, data.size will be zero for text and html content
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