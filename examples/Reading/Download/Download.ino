/**
 * The example to fetch the latest message in the INBOX and download.
 * For proper network/SSL client and port selection, please see http://bit.ly/46Xu9Yk
 */
#include <Arduino.h>
#include "Networks.h"

#define ENABLE_IMAP  // Allows IMAP class and data
#define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial
#define ENABLE_FS // Allow filesystem integration
#include <ReadyMail.h>

#define IMAP_HOST "_______"
#define IMAP_PORT 993 // SSL or 143 for PLAIN TEXT or STARTTLS
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

#define READ_ONLY_MODE true
#define AWAIT_MODE true
#define MAX_CONTENT_SIZE 1024 * 1024 // Maximum size in bytes of the body parts (text and attachment) to be downloaded.
#define BASE_DOWNLOAD_FOLDER "readymail"

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

// For more information, see https://bit.ly/4h9JR7p
void imapCb(IMAPStatus status)
{
    ReadyMail.printf("ReadyMail[imap][%d]%s\n", status.state, status.text.c_str());
}

// For more information, see https://bit.ly/3KLvz0y
void dataCb(IMAPCallbackData &data)
{
    // Showing envelope data.
    if (data.event() == imap_data_event_search || data.event() == imap_data_event_fetch_envelope)
    {
        // Show additional search info
        if (data.event() == imap_data_event_search)
            ReadyMail.printf("Showing Search result %d (%d) of %d from %d\n\n", data.messageIndex() + 1,
                             data.messageNum(), data.messageAvailable(), data.messageFound());

        // Headers data
        for (size_t i = 0; i < data.headerCount(); i++)
            ReadyMail.printf("%s: %s\n%s", data.getHeader(i).first.c_str(),
                             data.getHeader(i).second.c_str(), i == data.headerCount() - 1 ? "\n" : "");

        // Files data
        for (size_t i = 0; i < data.fileCount(); i++)
        {
            // You can fetch or download file while searching
            // (or disable it while fetching) with the following options.

            // To enable/disable this file for fetching.
            // data.fetchOption(i) = true;

            // To download if the fetch option is set.
            // data.setFileCallback(i, filecb, "/downloads");
            ReadyMail.printf("name: %s, mime: %s, charset: %s, trans-enc: %s, size: %d, fetch: %s%s\n",
                             data.fileInfo(i).filename.c_str(), data.fileInfo(i).mime.c_str(), data.fileInfo(i).charset.c_str(),
                             data.fileInfo(i).transferEncoding.c_str(), data.fileInfo(i).fileSize,
                             data.fetchOption(i) ? "yes" : "no", i == data.fileCount() - 1 ? "\n" : "");
        }
    }
    else if (data.event() == imap_data_event_fetch_body)
    {
        // Showing the text file content
        if (data.fileInfo().mime == "text/html" || data.fileInfo().mime == "text/plain")
        {
            if (data.fileChunk().index == 0) // Fist chunk
                Serial.println("------------------");
            Serial.print((char *)data.fileChunk().data);
            if (data.fileChunk().isComplete) // Last chunk
                Serial.println("------------------");
        }
        else
        {
            // Showing the progress of current file fetching
            if (data.fileChunk().index == 0)
                Serial.print("Downloading");
            if (data.fileProgress().available)
                Serial.print(".");
            if (data.fileChunk().isComplete)
                Serial.println(" complete");
        }
    }
}

#if defined(ENABLE_FS)
#include <FS.h>
File myFile;
#if defined(ESP32)
#include <SPIFFS.h>
#endif
#define MY_FS SPIFFS

void fileCb(File &file, const char *filename, readymail_file_operating_mode mode)
{
    switch (mode)
    {
    case readymail_file_mode_open_read:
        myFile = MY_FS.open(filename, FILE_OPEN_MODE_READ);
        break;
    case readymail_file_mode_open_write:
        myFile = MY_FS.open(filename, FILE_OPEN_MODE_WRITE);
        break;
    case readymail_file_mode_open_append:
        myFile = MY_FS.open(filename, FILE_OPEN_MODE_APPEND);
        break;
    case readymail_file_mode_remove:
        MY_FS.remove(filename);
        break;
    default:
        break;
    }

    // This is required by library to get the file object
    // that uses in its read/write processes.
    file = myFile;
}
#endif

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

    // If server SSL certificate verification was ignored for this ESP32 WiFiClientSecure.
    // To verify root CA or server SSL cerificate,
    // please consult your SSL client documentation.
    ssl_client.setInsecure();

    MY_FS.begin();

    // In case ESP8266 crashes, please see https://bit.ly/48r4wSe

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
    imap.fetch(imap.getMailbox().msgCount, dataCb, fileCb, AWAIT_MODE, MAX_CONTENT_SIZE, BASE_DOWNLOAD_FOLDER);

    // To fetch message with UID, use imap.fetchUID instead.
}

void loop()
{
}