/**
 * The example to send simple text message in async mode (non-await mode).
 * For proper network/SSL client and port selection, please see http://bit.ly/437GkRA
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP  // Allows SMTP class and data
#define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial

// If message timestamp and/or Date header was not set,
// the message timestamp will be taken from this source, otherwise
// the default timestamp will be used.
#if defined(ESP32) || defined(ESP8266)
#define READYMAIL_TIME_SOURCE time(nullptr); // Or using WiFi.getTime() in WiFiNINA and WiFi101 firmwares.
#endif

#include <ReadyMail.h>

#define SMTP_HOST "_______"
#define SMTP_PORT 465 // SSL or 587 for STARTTLS
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"
#define RECIPIENT_EMAIL "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

#define SSL_MODE true
#define AWAIT_MODE false
#define NOTIFY "SUCCESS,FAILURE,DELAY" // Delivery Status Notification (if SMTP server supports this DSN extension)

const char *greenImg = "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAEnQAABJ0Ad5mH3gAAAGHaVRYd"
                       "FhNTDpjb20uYWRvYmUueG1wAAAAAAA8P3hwYWNrZXQgYmVnaW49J++7vycgaWQ9J1c1TTBNcENlaGlIenJlU3pOVGN6a2M5ZCc/Pg0KPHg6eG1wbWV0YSB4bW"
                       "xuczp4PSJhZG9iZTpuczptZXRhLyI+PHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj48cmR"
                       "mOkRlc2NyaXB0aW9uIHJkZjphYm91dD0idXVpZDpmYWY1YmRkNS1iYTNkLTExZGEtYWQzMS1kMzNkNzUxODJmMWIiIHhtbG5zOnRpZmY9Imh0dHA6Ly9ucy5h"
                       "ZG9iZS5jb20vdGlmZi8xLjAvIj48dGlmZjpPcmllbnRhdGlvbj4xPC90aWZmOk9yaWVudGF0aW9uPjwvcmRmOkRlc2NyaXB0aW9uPjwvcmRmOlJERj48L3g6e"
                       "G1wbWV0YT4NCjw/eHBhY2tldCBlbmQ9J3cnPz4slJgLAAABAUlEQVR4Xu3RoQHAIBDAwKf7L8C04LsAEXcyNmv2nCHj+wfeMiTGkBhDYgyJMSTGkBhDYgyJMS"
                       "TGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMST"
                       "GkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTG"
                       "kBhDYgyJMSTGkBhDYgyJMSTGkJgLfx8CYHc7t9oAAAAASUVORK5CYII=";

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

unsigned long ms = 0;

// For more information, see https://bit.ly/44g9Fuc
void smtpCb(SMTPStatus status)
{
    if (status.progress.available)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state,
                         status.progress.filename.c_str(), status.progress.value);
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

void sendMesssage()
{
    SMTPMessage &msg = smtp.getMessage();
    msg.headers.add(rfc822_subject, "ReadyMail Hello message");
    msg.headers.add(rfc822_from, "ReadyMail <" + String(AUTHOR_EMAIL) + ">");
    // msg.headers.add(rfc822_sender, "ReadyMail <" + String(AUTHOR_EMAIL) + ">");
    msg.headers.add(rfc822_to, "User <" + String(RECIPIENT_EMAIL) + ">");

    String bodyText = "Hello everyone.";
    msg.text.body(bodyText);
    msg.html.body("<html><body><div style=\"color:#cc0066;\">" + bodyText + "</div></body></html>");

    // Set message timestamp (change this with current time)
    // See https://bit.ly/4jy8oU1
    msg.timestamp = 1746013620;

    addBlobAttachment(msg, "green.png", "image/png", "green.png", (const uint8_t *)greenImg, strlen(greenImg), "base64");
    smtp.send(msg, NOTIFY, AWAIT_MODE);
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

    // If server SSL certificate verification was ignored for this ESP32 WiFiClientSecure.
    // To verify root CA or server SSL cerificate,
    // please consult your SSL client documentation.
    ssl_client.setInsecure();

    Serial.println("ReadyMail, version " + String(READYMAIL_VERSION));

    // In case ESP8266 crashes, please see https://bit.ly/4iX1NkO

    // Setting AWAIT_MODE parameter with false
    smtp.connect(SMTP_HOST, SMTP_PORT, smtpCb, SSL_MODE, AWAIT_MODE);
}

void loop()
{
    // This is required to be placed in the loop for async mode usage.
    smtp.loop();

    if (smtp.isProcessing())
        return;

    if (!smtp.isConnected())
        smtp.connect(SMTP_HOST, SMTP_PORT, smtpCb, SSL_MODE, AWAIT_MODE);

    if (smtp.isConnected() && !smtp.isAuthenticated())
        smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password, AWAIT_MODE);

    if ((millis() - ms > 20 * 1000 || ms == 0) && smtp.isAuthenticated())
    {
        ms = millis();
        sendMesssage();
    }
}