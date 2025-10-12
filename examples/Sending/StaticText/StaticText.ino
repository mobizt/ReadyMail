/**
 * The example to send simple text message from file of flash.
 * For proper network/SSL client and port selection, please see http://bit.ly/46Xu9Yk
 */
#include <Arduino.h>
#include "Networks.h"

#define ENABLE_SMTP  // Allows SMTP class and data
#define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial
#define ENABLE_FS // Allow filesystem integration

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

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

#if defined(ENABLE_FS)
#include <FS.h>
File myFile;
#if defined(ESP32)
#include <SPIFFS.h>
#endif
#define MY_FS SPIFFS

const char *static_text1 = "A rabies-like disease spreads across the planet, transforming people into aggressive creatures. Manel takes refuge at "
                           "home with his cat, using his wits to survive. Soon, they’ll have to leave for food, looking for safe places on land "
                           "and sea. Apocalypse Z: The Beginning of the End is a story of survival, both physical and emotional, with action, "
                           "tension, a rabid infection... and a grumpy cat.";

const char *static_text2 = "Speak No Evil is a 2024 American psychological horror thriller film written and directed by James Watkins. A remake "
                           "of the 2022 Danish-Dutch film of the same name, the film stars James McAvoy, Mackenzie Davis, Aisling Franciosi, Alix "
                           "West Lefler, Dan Hough, and Scoot McNairy. Its plot follows an American family who are invited to stay at a remote "
                           "farmhouse of a British couple for the weekend, and the hosts soon test the limits of their guests as the situation "
                           "escalates. Jason Blum serves as a producer through his Blumhouse Productions banner. Speak No Evil premiered at the "
                           "DGA Theater in New York City on September 9, 2024 and was released in the United States by Universal Pictures on "
                           "September 13, 2024. The film received generally positive reviews from critics and grossed $77 million worldwide "
                           "with a budget of $15 million.";

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

    // This is required by library to get the file object
    // that uses in its read/write processes.
    file = myFile;
}

void createTextFile()
{
    MY_FS.begin();

    File file = MY_FS.open("/static.txt", FILE_OPEN_MODE_WRITE);
    file.print(static_text2);
    file.close();
}
#endif

// For more information, see http://bit.ly/474niML
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

#if defined(ENABLE_FS)
    createTextFile();
#endif

    Serial.println("ReadyMail, version " + String(READYMAIL_VERSION));

    // In case ESP8266 crashes, please see https://bit.ly/48r4wSe

    smtp.connect(SMTP_HOST, SMTP_PORT, smtpCb);
    if (!smtp.isConnected())
        return;

    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!smtp.isAuthenticated())
        return;

    SMTPMessage msg;
    msg.headers.add(rfc822_subject, "ReadyMail static message");

    // Using 'name <email>' or <email> or 'email' for the from, sender and recipients.
    // The 'name' section of cc and bcc is ignored.

    // Multiple recipents can be added but only the first one of sender and from can be added.
    msg.headers.add(rfc822_from, "ReadyMail <" + String(AUTHOR_EMAIL) + ">");
    // msg.headers.add(rfc822_sender, "ReadyMail <" + String(AUTHOR_EMAIL) + ">");
    msg.headers.add(rfc822_to, "User <" + String(RECIPIENT_EMAIL) + ">");

    // Text wrapping
    msg.text.textFlow(true);

    // Set the static body content from flle
    msg.text.body("/static.txt", fileCb);

    // Set the static body content from blob
    // msg.text.body(static_text1, strlen(static_text1));

    // Set message timestamp (change this with current time)
    // See https://bit.ly/4nEuBlk
    msg.timestamp = 1746013620;

    smtp.send(msg);
}

void loop()
{
}