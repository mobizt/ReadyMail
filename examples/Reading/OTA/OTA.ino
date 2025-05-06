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

// For more information, see https://bit.ly/4k6Uybd
void dataCb(IMAPCallbackData data)
{
    // Showing envelope data.
    if (data.isEnvelope)
    {
        // Headers data
        for (int i = 0; i < data.header.size(); i++)
            ReadyMail.printf("%s: %s\n%s", data.header[i].first.c_str(), data.header[i].second.c_str(), i == data.header.size() - 1 ? "\n" : "");
    }
    // Processing content stream.
    else
    {
        // Showing the text content
        if (strcmp(data.mime, "application/octet-stream") == 0 && strcmp(data.filename, "firmware.bin") == 0)
        {
            if (data.dataIndex == 0) // Fist chunk
            {
                ReadyMail.printf("Performing OTA update...\n");
                otaErr = !Update.begin(data.size);

                if (data.progressUpdated)
                    ReadyMail.printf("Downloading %s, %d of %d, %d %% completed\n", data.filename, data.dataIndex + data.dataLength, data.size, data.progress);

                if (!otaErr)
                    otaErr = Update.write((uint8_t *)data.blob, data.dataLength) != data.dataLength;
            }
            else if (!data.isComplete)
            {
                if (!otaErr)
                    otaErr = Update.write((uint8_t *)data.blob, data.dataLength) != data.dataLength;

                if (data.progressUpdated)
                    ReadyMail.printf("Downloading %s, %d of %d, %d %% completed\n", data.filename, data.dataIndex + data.dataLength, data.size, data.progress);
            }
            else // Last chunk
            {
                if (data.progressUpdated)
                    ReadyMail.printf("Downloading %s, %d of %d, %d %% completed\n", data.filename, data.dataIndex + data.dataLength, data.size, data.progress);

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