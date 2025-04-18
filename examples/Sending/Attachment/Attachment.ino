/**
 * The example to send message with attachment.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP  // Allow SMTP class and data
#define ENABLE_DEBUG // Allow debugging
#define READYMAIL_DEBUG_PORT Serial
#define ENABLE_FS // Allow filesystem integration
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

#if defined(ENABLE_FS)
#include <FS.h>
File myFile;
#if defined(ESP32)
#include <SPIFFS.h>
#endif
#define MY_FS SPIFFS

const char *orangeImg = "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAEnQAABJ0Ad5mH3gAAAEASURBVHhe7dEhAQAgEMBA2hCT6I+nABMnzsxuzdlDx3oDfxkSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0jMBYxyLEpP9PqyAAAAAElFTkSuQmCCjhSDb5FKG9Q4";
const char *blueImg = "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAEnQAABJ0Ad5mH3gAAAEASURBVHhe7dEhAQAgAMAwmmEJTyfwFOBiYub2Y6596Bhv4C9DYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJuZ7+qGdQMlUbAAAAAElFTkSuQmCC";

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
    file = myFile;
}

void createAttachment()
{
    MY_FS.begin(true);

    File file = MY_FS.open("/orange.png", FILE_WRITE);
    file.print(orangeImg);
    file.close();

    file = MY_FS.open("/blue.png", FILE_WRITE);
    file.print(blueImg);
    file.close();
}
#endif

void smtpCb(SMTPStatus status)
{
    if (status.progressUpdated)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state, status.filename.c_str(), status.progress);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
    // The status.state is the smtp_state enum defined in src/smtp/Common.h
}

void addFileAttachment(SMTPMessage &msg, const String &filename, const String &mime, const String &name, FileCallback cb, const String &filepath, const String &encoding = "", const String &cid = "")
{
    Attachment attachment;
    attachment.filename = filename;
    attachment.mime = mime;
    attachment.name = name;
    // The inline content disposition.
    // Should be matched the image src's cid in html body
    attachment.content_id = cid;
    attachment.attach_file.callback = cb;
    attachment.attach_file.path = filepath;
    // Specify only when content is already encoded.
    attachment.content_encoding = encoding;
    if (cid.length() > 0)
        msg.addInlineImage(attachment);
    else
        msg.addAttachment(attachment);
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

    createAttachment();

    ssl_client.setInsecure();

    smtp.connect(SMTP_HOST, SMTP_PORT, DOMAIN_OR_IP, smtpCb);
    if (!smtp.isConnected())
        return;

    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!smtp.isAuthenticated())
        return;

    SMTPMessage msg;
    msg.sender.name = "ReadyMail";
    msg.sender.email = AUTHOR_EMAIL;
    msg.subject = "ReadyMail Hello message with attachment";
    msg.addRecipient("User", RECIPIENT_EMAIL);

    String bodyText = "Hello everyone.\n";
    msg.text.content = bodyText;
    msg.text.transfer_encoding = "base64";
    msg.html.content = "<html><body><div style=\"color:#00ffff;\">" + bodyText + "</div></body></html>";
    msg.html.transfer_encoding = "base64";

    addFileAttachment(msg, "orange.png", "image/png", "orange.png", fileCb, "/orange.png", "base64");
    addFileAttachment(msg, "blue.png", "image/png", "blue.png", fileCb, "/blue.png", "base64");

    smtp.send(msg);
}

void loop()
{
}