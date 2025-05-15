/**
 * The example to fetch the latest message in the INBOX.
 * If message contains attachment "firmware.bin", the firmware update will begin.
 *
 * For proper network/SSL client and port selection, please see http://bit.ly/437GkRA
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Update.h>

#define ENABLE_IMAP  // Allows IMAP class and data
#define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial
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

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

bool otaErr = false;

// For more information, see https://bit.ly/3RH9ock
void imapCb(IMAPStatus status)
{
    ReadyMail.printf("ReadyMail[imap][%d]%s\n", status.state, status.text.c_str());
}

// For more information, see https://bit.ly/3GObULu
void dataCb(IMAPCallbackData &data)
{
    // Showing envelope data.
    if (data.event() == imap_data_event_search || data.event() == imap_data_event_fetch_envelope)
    {
        // Show additional search info
        if (data.event() == imap_data_event_search)
            ReadyMail.printf("Showing Search result %d (%d) of %d from %d\n\n", data.messageIndex() + 1, data.messageNum(), data.messageAvailable(), data.messageFound());

        // Headers data
        for (size_t i = 0; i < data.headerCount(); i++)
            ReadyMail.printf("%s: %s\n%s", data.getHeader(i).first.c_str(), data.getHeader(i).second.c_str(), i == data.headerCount() - 1 ? "\n" : "");

        // Files data
        for (size_t i = 0; i < data.fileCount(); i++)
        {
            // You can fetch or download file while searching
            // (or disable it while fetching) with the following options.

            // To enable/disable this file for fetching.
            // data.fetchOption(i) = true;

            // To download if the fetch option is set.
            // data.setFileCallback(i, filecb, "/downloads");
            ReadyMail.printf("name: %s, mime: %s, charset: %s, trans-enc: %s, size: %d, fetch: %s%s\n", data.fileInfo(i).filename.c_str(), data.fileInfo(i).mime.c_str(), data.fileInfo(i).charset.c_str(), data.fileInfo(i).transferEncoding.c_str(), data.fileInfo(i).fileSize, data.fetchOption(i) ? "yes" : "no", i == data.fileCount() - 1 ? "\n" : "");
        }
    }
    else if (data.event() == imap_data_event_fetch_body)
    {
        // Showing the text content
        if (data.fileInfo().mime == "application/octet-stream" && data.fileInfo().filename == "firmware.bin")
        {
            if (data.fileChunk().index == 0) // Fist chunk
            {
                ReadyMail.printf("Performing OTA update...\n");
                otaErr = !Update.begin(data.fileInfo().fileSize);

                if (data.fileProgress().available)
                    ReadyMail.printf("Downloading %s, %d of %d, %d %% completed\n", data.fileInfo().filename, data.fileChunk().index + data.fileChunk().size, data.fileInfo().fileSize, data.fileProgress().value);

                if (!otaErr)
                    otaErr = Update.write((uint8_t *)data.fileChunk().data, data.fileChunk().size) != data.fileChunk().size;
            }
            else if (!data.fileChunk().isComplete)
            {
                if (!otaErr)
                    otaErr = Update.write((uint8_t *)data.fileChunk().data, data.fileChunk().size) != data.fileChunk().size;

                if (data.fileProgress().available)
                    ReadyMail.printf("Downloading %s, %d of %d, %d %% completed\n", data.fileInfo().filename, data.fileChunk().index + data.fileChunk().size, data.fileInfo().fileSize, data.fileProgress().value);
            }
            else // Last chunk
            {
                if (data.fileProgress().available)
                    ReadyMail.printf("Downloading %s, %d of %d, %d %% completed\n", data.fileInfo().filename, data.fileChunk().index + data.fileChunk().size, data.fileInfo().fileSize, data.fileProgress().value);

                if (!otaErr)
                    otaErr = !Update.end(true);

                if (otaErr)
                    ReadyMail.printf("OTA update failed.\n");
                else
                {
                    ReadyMail.printf("OTA update success.\n");
                    ReadyMail.printf("Restarting...\n");
                    delay(2000);
                    ESP.restart();
                }
            }
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

    // In case ESP8266 crashes, please see https://bit.ly/4iX1NkO

    imap.connect(IMAP_HOST, IMAP_PORT, imapCb);
    if (!imap.isConnected())
        return;

    imap.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!imap.isAuthenticated())
        return;

    // List all mailboxes.
    imap.list();

    // Select INBOX mailbox.
    // If READ_ONLY_MODE is false, the flag /Seen will set to the fetched message.
    imap.select("INBOX", READ_ONLY_MODE);

    // Fetch the latest message in INBOX.
    imap.fetch(imap.getMailbox().msgCount, dataCb, NULL /* FileCallback */, AWAIT_MODE, MAX_CONTENT_SIZE);
}

void loop()
{
}