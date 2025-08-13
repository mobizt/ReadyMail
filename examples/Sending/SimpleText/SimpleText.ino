/**
 * The example to send simple text message.
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
#define AUTHENTICATION true
#define NOTIFY "SUCCESS,FAILURE,DELAY" // Delivery Status Notification (if SMTP server supports this DSN extension)
#define PRIORITY "High"                // High, Normal, Low
#define PRIORITY_NUM "1"               // 1 = high, 3, 5 = low
#define EMBED_MESSAGE false            // To send the html or text content as attachment

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

// For more information, see https://bit.ly/44g9Fuc
void smtpCb(SMTPStatus status)
{
    if (status.progress.available)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state,
                         status.progress.filename.c_str(), status.progress.value);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
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

    smtp.connect(SMTP_HOST, SMTP_PORT, smtpCb, SSL_MODE);
    if (!smtp.isConnected())
        return;

    if (AUTHENTICATION)
    {
        smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
        if (!smtp.isAuthenticated())
            return;
    }

    SMTPMessage msg;
    msg.headers.add(rfc822_subject, "ReadyMail test message");

    // Using 'name <email>' or <email> or 'email' for the from, sender and recipients.
    // The 'name' section of cc and bcc is ignored.

    // Multiple recipents can be added but only the first one of sender and from can be added.
    msg.headers.add(rfc822_from, "ReadyMail <" + String(AUTHOR_EMAIL) + ">");
    // msg.headers.add(rfc822_sender, "ReadyMail <" + String(AUTHOR_EMAIL) + ">");
    msg.headers.add(rfc822_to, "User <" + String(RECIPIENT_EMAIL) + ">");

    // Use addCustom to add custom header e.g. Imprtance and Priority.
    msg.headers.addCustom("Importance", PRIORITY);
    msg.headers.addCustom("X-MSMail-Priority", PRIORITY);
    msg.headers.addCustom("X-Priority", PRIORITY_NUM);

    String bodyText = "Hello everyone.\r\n";
    bodyText += "こんにちは、日本の皆さん\r\n";
    bodyText += "大家好，中国人\r\n";
    bodyText += "Здравей български народе";

    // Set the content, content transfer encoding or charset
    msg.text.body(bodyText);

    // content transfer encoding e.g. 7bit, base64, quoted-printable
    // 7bit is default for ascii text and quoted-printable is default set for non-ascii text.
    // msg.text.transferEncoding("base64");

    bodyText.replace("\r\n", "<br>\r\n");
    msg.html.body("<html><body><div style=\"color:#cc0066;\">" + bodyText + "</div></body></html>");

    // msg.html.transferEncoding("base64");

    // With embedFile function, the html message will send as attachment.
    if (EMBED_MESSAGE)
        msg.html.embedFile(true, "msg.html", embed_message_type_attachment);

    // Set message timestamp (change this with current time)
    // See https://bit.ly/4jy8oU1
    msg.timestamp = 1746013620;

    smtp.send(msg, NOTIFY);
}

void loop()
{
}