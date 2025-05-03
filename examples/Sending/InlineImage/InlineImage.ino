/**
 * The example to send message with inline image.
 * For proper network/SSL client and port selection, please see http://bit.ly/437GkRA
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP  // Allows SMTP class and data
#define ENABLE_DEBUG // Allows debugging
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

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

const char *orangeImg = "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAEnQAABJ0Ad5mH3gAAAEASURBVHhe7dEhAQAgEMBA2hCT6I+nABMnzsxuzdlDx3oDfxkSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0jMBYxyLEpP9PqyAAAAAElFTkSuQmCCjhSDb5FKG9Q4";
const char *blueImg = "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAEnQAABJ0Ad5mH3gAAAEASURBVHhe7dEhAQAgAMAwmmEJTyfwFOBiYub2Y6596Bhv4C9DYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJuZ7+qGdQMlUbAAAAAElFTkSuQmCC";

// For more information, see https://bit.ly/44g9Fuc
void smtpCb(SMTPStatus status)
{
    if (status.progressUpdated)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state, status.filename.c_str(), status.progress);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
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
    msg.attachments.add(attachment, cid.length() > 0 ? attach_type_inline : attach_type_attachment);
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

    smtp.connect(SMTP_HOST, SMTP_PORT, DOMAIN_OR_IP, smtpCb);
    if (!smtp.isConnected())
        return;

    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!smtp.isAuthenticated())
        return;

    SMTPMessage msg;
    msg.headers.add(rfc822_subject, "ReadyMail message with inline image");
    msg.headers.add(rfc822_from, "ReadyMail <" + String(AUTHOR_EMAIL) + ">");
    // msg.headers.add(rfc822_sender, "ReadyMail <" + String(AUTHOR_EMAIL) + ">");
    msg.headers.add(rfc822_to, "User <" + String(RECIPIENT_EMAIL) + ">");

    String bodyText = "This message contains inline images.";
    msg.text.body(bodyText);
    msg.html.body("<html><body><div style=\"color:#cc0066;\">" + bodyText + "<br/><br/><img src=\"cid:orange_image\" alt=\"orange image\"> <img src=\"cid:blue_image\" alt=\"blue image\"></div></body></html>");

    // Set message timestamp (change this with current time)
    // See https://bit.ly/4jy8oU1
    msg.timestamp = 1746013620;

    addBlobAttachment(msg, "orange.png", "image/png", "orange.png", (const uint8_t *)orangeImg, strlen(orangeImg), "base64", "orange_image");
    addBlobAttachment(msg, "blue.png", "image/png", "blue.png", (const uint8_t *)blueImg, strlen(blueImg), "base64", "blue_image");
    smtp.send(msg);
}

void loop()
{
}