/**
 * The example to send the IMAP command to create the mailbox, copy the message to and delete the message and mailbox.
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
#define MAX_SEARCH_RESULT 10
#define RECENT_SORT true

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

// The processing status will show here.
void imapCb(IMAPStatus status)
{
    ReadyMail.printf("ReadyMail[imap][%d]%s\n", status.state, status.text.c_str());
    // The status.state is the imap_state enum defined in src/imap/Common.h
}

void cmdCb(const char *cmd, const char *response)
{
    ReadyMail.printf("ReadyMail[imap][%d] %s --> %s\n", imap.currentState(), cmd, response);
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

    for (int i = 0; i < imap.mailboxes.size(); i++)
        ReadyMail.printf("Attributes: %s, Delimiter: %s, Name: %s\n%s", imap.mailboxes[i][0].c_str(), imap.mailboxes[i][1].c_str(), imap.mailboxes[i][2].c_str(), (i == imap.mailboxes.size() - 1 ? "\n" : ""));

    // Select INBOX mailbox.
    bool success = imap.select("INBOX", READ_ONLY_MODE);
    if (!success)
        return;

    // Create "ReadyMail" mailbox.
    success = imap.sendCommand("CREATE ReadyMail", cmdCb);
    if (!success)
        return;

    // Copy the latest message from INBOX to ReadyMail
    success = imap.sendCommand("COPY " + String(imap.getMailbox().msgCount) + " ReadyMail", cmdCb);
    if (!success)
        return;

    // Select ReadyMail for read/write
    success = imap.select("ReadyMail", false);
    if (!success)
        return;

    // Delete the latest message in ReadyMail mailbox by assigning the \Deleted flag.
    success = imap.sendCommand("STORE " + String(imap.getMailbox().msgCount) + " +FLAGS (\\Deleted)", cmdCb);
    if (!success)
        return;

    // Permanently delete messages in ReadyMail that the \Deleted flag was assigned.
    success = imap.sendCommand("EXPUNGE", cmdCb);
    if (!success)
        return;

    // Before deleting ReadyMail mailbox that we are currently selected,
    // select other mailbox first e.g. INBOX to avoid server connection closing error especially in ESP32.
    success = imap.select("INBOX", false);
    if (!success)
        return;

    // Delete ReadyMail mailbox
    success = imap.sendCommand("DELETE ReadyMail", cmdCb);

    if (success)
        Serial.println("All commands are sent successfully");
}

void loop()
{
}