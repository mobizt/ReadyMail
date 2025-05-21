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

void textDecodingCb(const String &charset, const uint8_t *in, int inSize, uint8_t *out, int &outSize)
{
    /** Custom text body decoding based on the character set
     *
     *  in - The encoded text
     *  inSize - The length of encoded text
     *  out - The buffer to save the unencoded (converted) text
     *  outSize - The length of unencoded text
     */
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

            // To decode the text body based on the character set.
            // if (data.fileInfo(i).charset.length())
            //    data.setTextEncodingCallback(i, textDecodingCb);
            ReadyMail.printf("name: %s, mime: %s, charset: %s, trans-enc: %s, size: %d, fetch: %s%s\n", data.fileInfo(i).filename.c_str(), data.fileInfo(i).mime.c_str(), data.fileInfo(i).charset.c_str(), data.fileInfo(i).transferEncoding.c_str(), data.fileInfo(i).fileSize, data.fetchOption(i) ? "yes" : "no", i == data.fileCount() - 1 ? "\n" : "");
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