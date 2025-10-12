/**
 * The example to send nested RFC822 messages that contain attachment.
 * For proper network/SSL client and port selection, please see http://bit.ly/46Xu9Yk
 */
#include <Arduino.h>
#include "Networks.h"

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
#define AUTHENTICATION true

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

const char *blueImg = "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAEnQAABJ0Ad5mH3gAAAGHaVRYdFh"
                      "NTDpjb20uYWRvYmUueG1wAAAAAAA8P3hwYWNrZXQgYmVnaW49J++7vycgaWQ9J1c1TTBNcENlaGlIenJlU3pOVGN6a2M5ZCc/Pg0KPHg6eG1wbWV0YSB4bWxucz"
                      "p4PSJhZG9iZTpuczptZXRhLyI+PHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj48cmRmOkRlc"
                      "2NyaXB0aW9uIHJkZjphYm91dD0idXVpZDpmYWY1YmRkNS1iYTNkLTExZGEtYWQzMS1kMzNkNzUxODJmMWIiIHhtbG5zOnRpZmY9Imh0dHA6Ly9ucy5hZG9iZS5j"
                      "b20vdGlmZi8xLjAvIj48dGlmZjpPcmllbnRhdGlvbj4xPC90aWZmOk9yaWVudGF0aW9uPjwvcmRmOkRlc2NyaXB0aW9uPjwvcmRmOlJERj48L3g6eG1wbWV0YT4"
                      "NCjw/eHBhY2tldCBlbmQ9J3cnPz4slJgLAAABAElEQVR4Xu3RIQEAIADAMCAs/RuApwAXm7z9HPucQcZ6A38ZEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEm"
                      "NIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIj"
                      "CExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCEx"
                      "hsQYEmNIzAVWiwMsnMB6awAAAABJRU5ErkJggg==";

const char *pinkImg = "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAEnQAABJ0Ad5mH3gAAAGHaVRYdFh"
                      "NTDpjb20uYWRvYmUueG1wAAAAAAA8P3hwYWNrZXQgYmVnaW49J++7vycgaWQ9J1c1TTBNcENlaGlIenJlU3pOVGN6a2M5ZCc/Pg0KPHg6eG1wbWV0YSB4bWxucz"
                      "p4PSJhZG9iZTpuczptZXRhLyI+PHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj48cmRmOkRlc"
                      "2NyaXB0aW9uIHJkZjphYm91dD0idXVpZDpmYWY1YmRkNS1iYTNkLTExZGEtYWQzMS1kMzNkNzUxODJmMWIiIHhtbG5zOnRpZmY9Imh0dHA6Ly9ucy5hZG9iZS5j"
                      "b20vdGlmZi8xLjAvIj48dGlmZjpPcmllbnRhdGlvbj4xPC90aWZmOk9yaWVudGF0aW9uPjwvcmRmOkRlc2NyaXB0aW9uPjwvcmRmOlJERj48L3g6eG1wbWV0YT4"
                      "NCjw/eHBhY2tldCBlbmQ9J3cnPz4slJgLAAABAUlEQVR4Xu3RoQHAIADAMOBYruUn8HtgFYms7bz73EHG+gb+ZUiMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0"
                      "iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMI"
                      "TGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGGxBgSY0iMITGG"
                      "xBgSY0iMITEPMCgD+BGiCr8AAAAASUVORK5CYII=";

const char *orangeImg = "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAEnQAABJ0Ad5mH3gAAAGHaVRYd"
                        "FhNTDpjb20uYWRvYmUueG1wAAAAAAA8P3hwYWNrZXQgYmVnaW49J++7vycgaWQ9J1c1TTBNcENlaGlIenJlU3pOVGN6a2M5ZCc/Pg0KPHg6eG1wbWV0YSB4bW"
                        "xuczp4PSJhZG9iZTpuczptZXRhLyI+PHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj48cmR"
                        "mOkRlc2NyaXB0aW9uIHJkZjphYm91dD0idXVpZDpmYWY1YmRkNS1iYTNkLTExZGEtYWQzMS1kMzNkNzUxODJmMWIiIHhtbG5zOnRpZmY9Imh0dHA6Ly9ucy5h"
                        "ZG9iZS5jb20vdGlmZi8xLjAvIj48dGlmZjpPcmllbnRhdGlvbj4xPC90aWZmOk9yaWVudGF0aW9uPjwvcmRmOkRlc2NyaXB0aW9uPjwvcmRmOlJERj48L3g6e"
                        "G1wbWV0YT4NCjw/eHBhY2tldCBlbmQ9J3cnPz4slJgLAAABAElEQVR4Xu3RIQEAIADAMKB/AdKCpwAXm7z9PHucQcZ6A38ZEmNIjCExhsQYEmNIjCExhsQYEm"
                        "NIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmN"
                        "IjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNIjCExhsQYEmNI"
                        "jCExhsQYEmNIjCExhsQYEmNIzAWOoANfanEodwAAAABJRU5ErkJggg==";

const char *greenImg = "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAAEnQAABJ0Ad5mH3gAAAGHaVRYdF"
                       "hNTDpjb20uYWRvYmUueG1wAAAAAAA8P3hwYWNrZXQgYmVnaW49J++7vycgaWQ9J1c1TTBNcENlaGlIenJlU3pOVGN6a2M5ZCc/Pg0KPHg6eG1wbWV0YSB4bWxu"
                       "czp4PSJhZG9iZTpuczptZXRhLyI+PHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj48cmRmOk"
                       "Rlc2NyaXB0aW9uIHJkZjphYm91dD0idXVpZDpmYWY1YmRkNS1iYTNkLTExZGEtYWQzMS1kMzNkNzUxODJmMWIiIHhtbG5zOnRpZmY9Imh0dHA6Ly9ucy5hZG9i"
                       "ZS5jb20vdGlmZi8xLjAvIj48dGlmZjpPcmllbnRhdGlvbj4xPC90aWZmOk9yaWVudGF0aW9uPjwvcmRmOkRlc2NyaXB0aW9uPjwvcmRmOlJERj48L3g6eG1wbW"
                       "V0YT4NCjw/eHBhY2tldCBlbmQ9J3cnPz4slJgLAAABAUlEQVR4Xu3RoQHAIBDAwKf7L8C04LsAEXcyNmv2nCHj+wfeMiTGkBhDYgyJMSTGkBhDYgyJMSTGkBhD"
                       "YgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYg"
                       "yJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJMSTGkBhDYgyJ"
                       "MSTGkBhDYgyJMSTGkJgLfx8CYHc7t9oAAAAASUVORK5CYII=";

static const uint8_t blueText[] PROGMEM = {0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68, 0x65, 0x20, 0x74, 0x65, 0x78, 0x74,
                                           0x20, 0x66, 0x69, 0x6C, 0x65, 0x20, 0x61, 0x74, 0x74, 0x61, 0x63, 0x68, 0x6D, 0x65, 0x6E, 0x74,
                                           0x20, 0x69, 0x6E, 0x20, 0x62, 0x6C, 0x75, 0x65, 0x20, 0x6D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
                                           0x2E};

static const uint8_t pinkText[] PROGMEM = {0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68, 0x65, 0x20, 0x74, 0x65, 0x78, 0x74,
                                           0x20, 0x66, 0x69, 0x6C, 0x65, 0x20, 0x61, 0x74, 0x74, 0x61, 0x63, 0x68, 0x6D, 0x65, 0x6E, 0x74,
                                           0x20, 0x69, 0x6E, 0x20, 0x70, 0x69, 0x6E, 0x6B, 0x20, 0x6D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
                                           0x2E};

static const uint8_t orangeText[] PROGMEM = {0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68, 0x65, 0x20, 0x74, 0x65, 0x78, 0x74,
                                             0x20, 0x66, 0x69, 0x6C, 0x65, 0x20, 0x61, 0x74, 0x74, 0x61, 0x63, 0x68, 0x6D, 0x65, 0x6E, 0x74,
                                             0x20, 0x69, 0x6E, 0x20, 0x6F, 0x72, 0x61, 0x6E, 0x67, 0x65, 0x20, 0x6D, 0x65, 0x73, 0x73, 0x61,
                                             0x67, 0x65, 0x2E};

static const uint8_t greenText[] PROGMEM = {0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68, 0x65, 0x20, 0x74, 0x65, 0x78, 0x74,
                                            0x20, 0x66, 0x69, 0x6C, 0x65, 0x20, 0x61, 0x74, 0x74, 0x61, 0x63, 0x68, 0x6D, 0x65, 0x6E, 0x74,
                                            0x20, 0x69, 0x6E, 0x20, 0x67, 0x72, 0x65, 0x65, 0x6E, 0x20, 0x6D, 0x65, 0x73, 0x73, 0x61, 0x67,
                                            0x65, 0x2E};

// For more information, see http://bit.ly/474niML
void smtpCb(SMTPStatus status)
{
    if (status.progress.available)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state,
                         status.progress.filename.c_str(), status.progress.value);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
}

void createMessage(SMTPMessage &msg, const String &name, const String &email, const String &subject, const String &recipient, const String &recipientEmail, const String &content, const String &htmlColor)
{
    msg.headers.add(rfc822_subject, subject);
    msg.headers.add(rfc822_from, name + " <" + email + ">");
    // msg.headers.add(rfc822_sender, name + " <" + email + ">");
    msg.headers.add(rfc822_to, recipient + " <" + recipientEmail + ">");

    String bodyText = "Hello everyone.";
    msg.text.body(content);
    msg.html.body("<html><body><div style=\"color:" + htmlColor + ";\">" + content + "</div></body></html>");

    // Set message timestamp (change this with current time)
    // See https://bit.ly/4nEuBlk
    msg.timestamp = 1746013620;
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

    // If server SSL certificate verification was ignored for this ESP32 WiFiClientSecure.
    // To verify root CA or server SSL cerificate,
    // please consult your SSL client documentation.
    ssl_client.setInsecure();

    Serial.println("ReadyMail, version " + String(READYMAIL_VERSION));

    // In case ESP8266 crashes, please see https://bit.ly/48r4wSe

    smtp.connect(SMTP_HOST, SMTP_PORT, smtpCb, SSL_MODE);
    if (!smtp.isConnected())
        return;

    if (AUTHENTICATION)
    {
        smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
        if (!smtp.isAuthenticated())
            return;
    }

    SMTPMessage blueMsg, pinkMsg, orangeMsg, greenMsg;
    createMessage(greenMsg, "Green", "green@example.com", "ReadyMail green message", "Steve", "steve@example.com", "This is the green text message", "#009900");
    addBlobAttachment(greenMsg, "green.png", "image/png", "green.png", (const uint8_t *)greenImg, strlen(greenImg), "base64");
    addBlobAttachment(greenMsg, "green.txt", "text/plain", "green.txt", greenText, sizeof(greenText));

    createMessage(orangeMsg, "Orange", "orange@example.com", "ReadyMail orange message", "Mike", "mike@xample.com", "This is the orange text message that contains green message", "#ff9900");
    addBlobAttachment(orangeMsg, "orange.png", "image/png", "orange.png", (const uint8_t *)orangeImg, strlen(orangeImg), "base64");
    addBlobAttachment(orangeMsg, "orange.txt", "text/plain", "orange.txt", orangeText, sizeof(orangeText));
    orangeMsg.addMessage(greenMsg, "green", "green.eml");

    createMessage(pinkMsg, "Pink", "pink@example.com", "ReadyMail pink message", "Joe", "joe@example.com", "This is the pink text message that contains orange message", "#ff66cc");
    addBlobAttachment(pinkMsg, "pink.png", "image/png", "pink.png", (const uint8_t *)pinkImg, strlen(pinkImg), "base64");
    addBlobAttachment(pinkMsg, "pink.txt", "text/plain", "pink.txt", pinkText, sizeof(pinkText));
    pinkMsg.addMessage(orangeMsg, "orange", "orange.eml");

    createMessage(blueMsg, "Blue", "blue@example.com", "ReadyMail blue message", "User", RECIPIENT_EMAIL, "This is the blue text message that contains pink message", "#0066ff");
    addBlobAttachment(blueMsg, "blue.png", "image/png", "blue.png", (const uint8_t *)blueImg, strlen(blueImg), "base64");
    addBlobAttachment(blueMsg, "blue.txt", "text/plain", "blue.txt", blueText, sizeof(blueText));
    blueMsg.addMessage(pinkMsg, "pink", "pink.eml");
    smtp.send(blueMsg);
}

void loop()
{
}