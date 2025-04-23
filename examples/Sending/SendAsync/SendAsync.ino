/**
 * The example to send simple text message in async mode (non-AWAIT_MODE).
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP  // Allow SMTP class and data
#define ENABLE_DEBUG // Allow debugging
#define READYMAIL_DEBUG_PORT Serial
#include "ReadyMail.h"

#define SMTP_HOST "_______"
#define SMTP_PORT 465
#define DOMAIN_OR_IP "127.0.0.1"
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"
#define RECIPIENT_EMAIL "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

#define SSL_MODE true
#define AWAIT_MODE false

const char *greenImg = "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAEnQAABJ0Ad5mH3gAAAGHaVRYdFhNTDpjb20uYWRvYmUueG1wAAAAAAA8P3hwYWNrZXQgYmVnaW49J++7vycgaWQ9J1c1TTBNcENlaGlIenJlU3pOVGN6a2M5ZCc/Pg0KPHg6eG1wbWV0YSB4bWxuczp4PSJhZG9iZTpuczptZXRhLyI+PHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj48cmRmOkRlc2NyaXB0aW9uIHJkZjphYm91dD0idXVpZDpmYWY1YmRkNS1iYTNkLTExZGEtYWQzMS1kMzNkNzUxODJmMWIiIHhtbG5zOnRpZmY9Imh0dHA6Ly9ucy5hZG9iZS5jb20vdGlmZi8xLjAvIj48dGlmZjpPcmllbnRhdGlvbj4xPC90aWZmOk9yaWVudGF0aW9uPjwvcmRmOkRlc2NyaXB0aW9uPjwvcmRmOlJERj48L3g6eG1wbWV0YT4NCjw/eHBhY2tldCBlbmQ9J3cnPz4slJgLAAABAUlEQVR4Xu3RoQHAIBDAwKf7L8C04LsAEXcyNmv2nCHj+wfeMiTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkJgLfx8CYHc7t9oAAAAASUVORK5CYII=";

WiFiClientSecure ssl_client;

SMTPClient smtp(ssl_client);

unsigned long ms = 0;

void smtpCb(SMTPStatus status)
{
    if (status.progressUpdated)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state, status.filename.c_str(), status.progress);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());

    // The status.state is the smtp_state enum defined in src/smtp/Common.h

    if (smtp.isComplete())
    {
        // Sending completed
    }
}

void addBlobAttachment(SMTPMessage &msg, const String &filename, const String &mime, const String &name, const uint8_t *blob, size_t size, const String &encoding = "", const String &cid = "")
{
    Attachment attachment;
    attachment.filename = filename;
    attachment.mime = mime;
    attachment.name = name;
    // The inline content disposition.
    // Should be matched the image src's cid in html body
    attachment.content_id = cid;
    attachment.attach_file.blob = blob;
    attachment.attach_file.blob_size = size;
    // Specify only when content is already encoded.
    attachment.content_encoding = encoding;
    if (cid.length() > 0)
        msg.addInlineImage(attachment);
    else
        msg.addAttachment(attachment);
}

void sendMesssage()
{
    SMTPMessage msg;
    msg.sender.name = "ReadyMail";
    msg.sender.email = AUTHOR_EMAIL;
    msg.subject = "ReadyMail Hello message";
    msg.addRecipient("User", RECIPIENT_EMAIL);

    String bodyText = "Hello everyone.\n";
    msg.text.content = bodyText;
    msg.html.content = "<html><body><div style=\"color:#00ffff;\">" + bodyText + "</div></body></html>";

    addBlobAttachment(msg, "green.png", "image/png", "green.png", (const uint8_t *)greenImg, strlen(greenImg), "base64");

    smtp.send(msg, AWAIT_MODE);
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

    // Setting AWAIT_MODE parameter with false
    smtp.connect(SMTP_HOST, SMTP_PORT, DOMAIN_OR_IP, smtpCb, SSL_MODE, AWAIT_MODE);
}

void loop()
{
    // Requires in the loop for async mode usage
    smtp.loop();

    if (smtp.isConnected() && !smtp.isAuthenticated())
        smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password, AWAIT_MODE);

    if ((millis() - ms > 120 * 1000 || ms == 0) && smtp.isAuthenticated())
    {
        ms = millis();
        sendMesssage();
    }
}