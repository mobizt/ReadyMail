/**
 * The example to send simple text message which port can be changed at run time.
 * For proper network/SSL client and port selection, please see http://bit.ly/46Xu9Yk
 */
#include <Arduino.h>
#include "Networks.h"
#include <WiFiClient.h>
#include <ESP_SSLClient.h>

#define ENABLE_SMTP
#define ENABLE_DEBUG
#define READYMAIL_DEBUG_PORT Serial
#include <ReadyMail.h>

#define SMTP_HOST "_______"
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"
#define RECIPIENT_EMAIL "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

WiFiClient basic_client;
ESP_SSLClient ssl_client;
SMTPClient smtp(ssl_client);

// For more information, see http://bit.ly/474niML
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
  // Starting SSL Client connection in SSL mode when port is 465.
  ssl_client.enableSSL(port == 465);

  // Send start TLS only when port is 587
  bool startTLS = port == 587;

  // Use SSL for SMTP connection option when port is 465 or 587
  bool ssl = (port == 465 || port == 587);

  // Anonymous function that handles TLS handshake process.
  auto startTLSCallback = [](bool &success)
  { success = ssl_client.connectSSL(); };

  // Set/reset the TLS handshake callback and STARTTLS option.
  // Set when port is 587 otherwise reset.
  if (startTLS)
    smtp.setStartTLS((startTLSCallback), true);
  else
    smtp.setStartTLS(NULL, false);

  // In case ESP8266 crashes, please see https://bit.ly/48r4wSe

  // Everytime you call connect(), the previouse session will stop.
  // If you want to reuse the session, just skipping the connect() and authenticate().
  smtp.connect(SMTP_HOST, port, smtpCb, ssl);
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

  ssl_client.setClient(&basic_client);

  // If server SSL certificate verification was ignored for this SSL Client.
  // To verify root CA or server SSL cerificate,
  // please consult SSL Client documentation.
  ssl_client.setInsecure();

  Serial.println("ReadyMail, version " + String(READYMAIL_VERSION));

  sendEmail(465); // SSL

  sendEmail(587); // STARTTLS

  // If your server supports this protocol.
  sendEmail(25); // Plain Text
}

void loop()
{
}