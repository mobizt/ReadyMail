/**
 * The example to send simple text message using ReadyClient class which port can be changed at run time.
 * For proper network/SSL client and port selection, please see http://bit.ly/437GkRA
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define ENABLE_SMTP
#define ENABLE_DEBUG
#define READYMAIL_DEBUG_PORT Serial
#define USE_ESP_SSLCLIENT

#if defined(USE_ESP_SSLCLIENT)
#include <ESP_SSLClient.h>
#else
#include <NetworkClientSecure.h>
#endif

#if defined(USE_ESP_SSLCLIENT)
#define READYCLIENT_SSL_CLIENT ESP_SSLClient
#define READYCLIENT_TYPE_1 // TYPE 1 when using ESP_SSLClient
#else
#define READYCLIENT_SSL_CLIENT NetworkClientSecure
#define READYCLIENT_TYPE_2 // TYPE 2 when using NetworkClientSecure
#endif

#include <ReadyMail.h>

#define SMTP_HOST "_______"
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"
#define RECIPIENT_EMAIL "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

#if defined(USE_ESP_SSLCLIENT)
WiFiClient basic_client;
ESP_SSLClient ssl_client;
#else
NetworkClientSecure ssl_client;
#endif

ReadyClient rClient(ssl_client);
SMTPClient smtp(rClient);

// For more information, see https://bit.ly/44g9Fuc
void smtpCb(SMTPStatus status)
{
    if (status.progress.available)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state,
                         status.progress.filename.c_str(), status.progress.value);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
}

void sendEmail(int port)
{

    smtp.connect(SMTP_HOST, port, smtpCb);
    if (!smtp.isConnected())
        return;

    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!smtp.isAuthenticated())
        return;

    SMTPMessage msg;
    msg.headers.add(rfc822_subject, "ReadyMail test via port " + String(port));
    msg.headers.add(rfc822_from, "ReadyMail <" + String(AUTHOR_EMAIL) + ">");
    msg.headers.add(rfc822_to, "User <" + String(RECIPIENT_EMAIL) + ">");

    String bodyText = "Hello test.";
    msg.text.body(bodyText);
    msg.html.body("<html><body><div style=\"color:#cc0066;\">" + bodyText + "</div></body></html>");
    msg.timestamp = 1746013620;
    smtp.send(msg);
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

#if defined(USE_ESP_SSLCLIENT)
    ssl_client.setClient(&basic_client);
#endif

    // If server SSL certificate verification was ignored for this SSL Client.
    // To verify root CA or server SSL cerificate,
    // please consult SSL Client documentation.
    ssl_client.setInsecure();

    Serial.println("ReadyMail, version " + String(READYMAIL_VERSION));

    rClient.addPort(465, readymail_protocol_ssl);
    rClient.addPort(587, readymail_protocol_tls);
    rClient.addPort(25, readymail_protocol_plain_text);

    sendEmail(465); // SSL

    sendEmail(587); // STARTTLS

    // If your server supports this protocol.
    sendEmail(25); // Plain Text
}

void loop()
{
}