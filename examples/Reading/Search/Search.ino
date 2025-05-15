/**
 * The example to search the message in the selected mailbox.
 * For proper network/SSL client and port selection, please see http://bit.ly/437GkRA
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

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
#define MAX_SEARCH_RESULT 10
#define RECENT_SORT true

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

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
        // While in search mode, this event will be called when data.fetchOption is true.

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

    for (int i = 0; i < imap.mailboxes.size(); i++)
        ReadyMail.printf("Attributes: %s, Delimiter: %s, Name: %s\n%s", imap.mailboxes[i][0].c_str(), imap.mailboxes[i][1].c_str(), imap.mailboxes[i][2].c_str(), (i == imap.mailboxes.size() - 1 ? "\n" : ""));

    // Select INBOX mailbox.
    imap.select("INBOX", READ_ONLY_MODE);

    imap.search("SEARCH ALL" /* criteria */, MAX_SEARCH_RESULT, RECENT_SORT, dataCb, AWAIT_MODE);

    // If UID is provided in the search criteria,
    // the imap.searchResult() will contains the list of message UIDs otherwise the message numbers.
    for (int i = 0; i < imap.searchResult().size(); i++)
    {
        uint32_t uid_num = imap.searchResult()[i];
    }
}

void loop()
{
}