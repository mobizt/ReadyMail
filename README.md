# ReadyMail #

[![Github Stars](https://img.shields.io/github/stars/mobizt/ReadyMail?logo=github)](https://github.com/mobizt/ESP-Mail-Client/stargazers) ![Github Issues](https://img.shields.io/github/issues/mobizt/ReadyMail?logo=github)

![arduino-library-badge](https://www.ardu-badge.com/badge/ReadyMail.svg) ![PlatformIO](https://badges.registry.platformio.org/packages/mobizt/library/ReadyMail.svg)

The fast and lightweight async Email client library for Arduino.

This Email client library supports Email sending, reading, searching and appending.

This library supports all 32-bit `Arduino` devices e.g. `STM32`, `SAMD`, `ESP32`, `ESP8266`, `Raspberry Pi RP2040`, and `Renesas` devices. 

The 8-bit `Atmel's AVR` MCUs are not supported.

This library requires 52k program space (network library excluded) for only `SMTP` or `IMAP` feature and 82k for using both `SMTP` and `IMAP` features.

# Examples #

## Email Sending

To send an Email message, user needs to defined the `SMTPClient` and `SMTPMessage` class objects.

The one of SSL client if you are sending Email over SSL/TLS or basic network client should be set for the `SMTPClient` class constructor.

Note that the SSL client or network client assigned to the `SMTPClient` class object should be lived in the `SMTPClient` class object usage scope otherwise the error can be occurred.

The `STARTTLS` options can be set in the advance usage.

Starting the SMTP server connection first via `SMTPClient::connect` and providing the required parameters e.g. host, port, domain or IP and SMTPResponseCallback function.

Note that, the following code uses the lambda expression as the SMTPResponseCallback callback in `SMTPClient::connect`.

The SSL connection mode and await options are set true by default which can be changed via its parameters.

Then authenticate using `SMTPClient::authenticate` by providing the auth credentials and the type of authentication enum e.g. `readymail_auth_password`, `readymail_auth_access_token` and `readymail_auth_disabled`

Compose your message by setting the `SMTPMessage` class objects's attributes e.g. sender, recipient, subject, content etc. 

Then, calling `SMTPClient::send` using the composed message to send the message.

The following example code is for ESP32 using it's ESP32 core `WiFi.h` and `WiFiClientSecure.h` libraries for network interface and SSL client.

The `ENABLE_SMTP` macro is required for using `SMTPClient` and `SMTPMessage` classes. The `ENABLE_DEBUG` macro is for allowing the processing information debugging.

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP
#define ENABLE_DEBUG
#include "ReadyMail.h"

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

ssl_client.setInsecure();

// WiFi or network connection here
// ...
// ...

auto statusCallback = [](SMTPStatus status){ Serial.println(status.text);}

smtp.connect("smtp host here", 465, "127.0.0.1", statusCallback);
if (smtp.isConnected())
{
    smtp.authenticate("sender email here", "sender email password here", readymail_auth_password);
    if (smtp.isAuthenticated())
    {
        SMTPMessage msg;
        msg.sender.name = "ReadyMail";
        msg.sender.email = "sender email here";
        msg.subject = "Greeting message";
        msg.addRecipient("User", "recipient email here");
        msg.text.content = "Hello";
        msg.html.content = "<html><body>Hello</body></html>";
        // Set the message date, select one
        msg.timestamp = 1744951350; // The UNIX timestamp (seconds since Midnight Jan 1, 1970)
        // msg.date = "Fri, 18 Apr 2025 11:42:30 +0300"; //  
        // msg.addHeader("Date: Fri, 18 Apr 2025 11:42:30 +0300");
        smtp.send(msg);
    }
}
```

This library does not set the date header to SMTP message automatically unless system time was set in ESP8266 and ESP32 devices. 

User needs to set the message date by one of the following methods before sending the SMTP message.

Providing the RFC 2822 `Date` haeader using `SMTPMessage::addHeader("Date: ?????")` or using `SMTPMessage::date` with RFC 2822 date string or using `SMTPMessage::timestamp` with the UNIX timestamp. 

For ESP8266 and ESP32 devices as mentioned above the message date header will be auto-set, if the device system time was already set before sending the message.

### SMTP Response Callback and Status Data

The `SMTPStatus` struct param of `SMTPResponseCallback` function, provides the statuses of sending process for debugging purposes.

```cpp
typedef struct smtp_response_status_t
{
    int errorCode = 0;
    int statusCode = 0;
    smtp_state state = smtp_state_prompt;
    bool complete = false;
    bool progressUpdated = false;
    int progress = 0;
    String filename;
    String text;
} SMTPStatus;
```
The [negative value](/src/smtp/Common.h#L6-L12) of `errorCode` var represents the error of process otherwise no error. 

The `statusCode` value represents the SMTP server response's status codes.

The `state` value represents the `smtp_state` enum to show the current state of sending process.

The `complete` value represents the sending process is completed or finished.

When the sending process is finished, the `SMTPClient::isComplete()` will return true.

The `SMTPClient::send` will return the status of sending process. When it retuns `true` when using in await mode, means the sending process is success while using in async mode, it represents the success of current `smtp_state` otherwise it fails at some `smtp_state`.

The `progressUpdated` value will use to show the uploading progress debug when the `SMTPResponseCallback` was called. 

It represents the status when upload progress percentage (`progress`) has changed  by 4 or 5 percents.

The `progress` value shows the percentage of current uploading progress.

The `filename` value shows the current uploading file name.

The `text` value shows the status information details which included the result of process and `errorCode` and its detail in case of error.

The code below shows how to get the information from the `SMTPStatus` data in the `SMTPResponseCallback` function. 

```cpp
void smtpStatusCallback(SMTPStatus status)
{
    // For debugging.

    // If progressUpdated value is true, the callback was called due to the 
    // attachment uploading progress is available.
    // Showing the name of file that is uploading and the progress percentage.
    if (status.progressUpdated) 
        ReadyMail.printf("State: %d, Uploading file %s, %d %% completed\n", status.state, status.filename.c_str(), status.progress);
     // otherwise, normal debugging status is available
    else
        ReadyMail.printf("State: %d, %s\n", status.state, status.text.c_str());

    // For checking the sending result.

    if (status.errorCode < 0)
    {
        // Sending error handling here
        ReadyMail.printf("Process Error: %d\n", status.errorCode);
    }
    else
    {
         // Sending complete handling here
         ReadyMail.printf("Server Status: %d\n", status.statusCode);
    }

}
        
```

## Email Reading

To receive or fetch the Email, only `IMAPClient` calss object is required. The received message will not store in device memory but redirects to the callback function (`IMAPCallbackData`) for user processing.

This requires less memory than storing the message. The little memory is required for the chunked buffer that sent to the user callback function. 

User can compile or store the message by himself.

When the `FileCallback` function was assigned to the `IMAPClient::fetch` or `IMAPClient::fetchUID` function, the content will be downloaded as files automatically.

The user can limit the size of parts (text, message, application, audio and video) content to be ignored from download and redirect to `IMAPCallbackData`.

The processes for server connection and authentication or `IMAPClient` are the same as `SMTPClient` except for no doman or IP requires in the `IMAPClient::connect` method.

The mailbox must be selected before fetching or working with its messages.

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_IMAP
#define ENABLE_DEBUG
#include "ReadyMail.h"

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

ssl_client.setInsecure();

// WiFi or network connection here
// ...
// ...

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text);}
auto dataCallback = [](IMAPCallbackData data)
{
    if (data.isEnvelope) // For showing message headers.
    {
        for (size_t i = 0; i < data.header.size(); i++)
        ReadyMail.printf("%s: %s\n%s", data.header[i].first.c_str(), data.header[i].second.c_str(), (i == data.header.size() - 1 ? "\n" : ""));
    }
};

imap.connect("imap host here", 993, statusCallback);
if (smtp.isConnected())
{
    smtp.authenticate("sender email here", "sender email password here", readymail_auth_password);
    if (smtp.isAuthenticated())
    {
         imap.list(); // Optional. List all mailboxes.
         imap.select("INBOX"); // Select the mailbox to fetch its message.
         imap.fetch(imap.getMailbox().msgCount, dataCallback, NULL /* FileCallback */);
    }
}
```

### IMAP Response Callback and Status Data

The `IMAPStatus` struct param of `IMAPResponseCallback` function, provides the statuses of IMAP functions's process for debugging purposes.

```cpp
typedef struct imap_response_status_t
{
    int errorCode = 0;
    imap_state state = imap_state_prompt;
    String text;
} IMAPStatus;
```
The [negative value](/src/imap/Common.h#L9-L18) of `errorCode` var represents the error of process otherwise no error. 

The `state` var represents the `imap_state` enum to show the current state of sending process.

The `text` var shows the status information details which included the result of process and `errorCode` and its detail in case of error.

The code below shows how to get the information from the `IMAPStatus` data in the `IMAPResponseCallback` function.

```cpp
void imapStatusCallback(IMAPStatus status)
{
    // For debugging
    ReadyMail.printf("State: %d, %s\n", status.state, status.text.c_str());
}
```

### IMAP Data Callback and Callback Data

The `IMAPCallbackData` struct param of `DataCallback` function, provides the result data and information of IMAP functions.

```cpp
typedef struct imap_callback_data
{
    const char *filename = "";
    const char *mime = "";
    const uint8_t *blob = nullptr;
    uint32_t dataLength = 0, size = 0;
    int progress = 0, dataIndex = 0, currentMsgIndex = 0;
    uint32_t searchCount = 0;
    std::vector<uint32_t> msgList;
    std::vector<std::pair<String, String>> header;
    bool isComplete = false, isEnvelope = false, isSearch = false, progressUpdated = false;
} IMAPCallbackData;
```

The `filename`, `mime`, `blob`, `dataLength`, `size` and `dataIndex` are presented the file name, mime type, chunked data buffer, length of chunked data buffer, size of completed data, and index of chunked data of currently fetching body part content respectively.

Due to sometimes, the IMAP server returns incorrect octet size of non-base64 decoded text and html body part, then the `size` of `text/plain` and `text/html` content type `mime` will be zero for unknown.

The `dataIndex` var represents the current index position of completed data.

The size of data and information may not fit the available memory for storing in the device. It is suitable for data preview or data stream processing.

As this library does not provide the OTA update functionality as in the old library, the chunked binary content of firmware that is fetching can be used for the OTA update process.

For content and file downloads, please set the `FileCallback` to the fetching function.

The `progressUpdated` var will use for the body part content fetching or downloading progress report.

This `progressUpdated` is the member of `IMAPCallbackData` struct instead of `IMAPStatus` struct because it is optional and was used when data fetching or downloading is needed. 

It represents the status when the progress percentage (`progress`) has changed  by 4 or 5 percents.

The `searchCount` var shows the total messages that found from search.

The `msgList` is the search result that stores the array of message numbers or UIDs (if the keyword UID is provided in search criterial).

The `IMAPClient::searchResult()` will return the array of message numbers or UID as in `msgList`.

The maximum number of messages that can be stored in `msgList` and the soring order was set via the second param (`searchLimit`) and third param (`recentSort`) of `IMAPClient::search()`.

The `currentMsgIndex` var shows the current index of message in the `msgList` that is currently fetching the envelope (headers).

The `header` object contains the list of message headers's name and its value.

The `isComplete` var shows the complete status of fetching or searching process.

The `isEnvelope` var is used for checking whether the available data at this state is envelope (headers) information or body part content.

The `isSearch` var shows that the search result is available.

The code below shows how to get the data and information from the `IMAPCallbackData` data in the `DataCallback` function.

```cpp
void dataCallback(IMAPCallbackData data)
{
    // If headers info is available.
    // Showing the message headers.
    if (data.isEnvelope)
    {
        if (data.isSearch)
            ReadyMail.printf("Showing Search result %d (%d) of %d from %d\n\n", data.currentMsgIndex + 1, data.msgList[data.currentMsgIndex], data.msgList.size(), data.searchCount);

        for (int i = 0; i < data.header.size(); i++)
            ReadyMail.printf("%s: %s\n%s", data.header[i].first.c_str(), data.header[i].second.c_str(), i == data.header.size() - 1 ? "\n" : "");
    }
    // otherwise, processing the body part content.
    else
    {
        // Showing some information of content

        if (data.progressUpdated)
            ReadyMail.printf("Downloading file %s, type %s, %d %%% completed", data.filename, data.mime, data.progress);

        ReadyMail.printf("Data Index: %d, Length: %d, Size: %d\n", data.dataIndex, data.dataLength, data.size);

        // Process the content (data.blob) here
        
    }
}
```


### IMAP Custom Comand Callback

The `IMAPCustomComandCallback` function which used with `IMAPClient::sendCommand()`, provides two parameters e.g. `cmd` that shows the current command and `response` that shows the untagged response of command except for the tagged responses that contains `OK`, `NO` and `BAD`.

## File Upload and Download

The `FileCallback` function that used with SMTP attachment (`Attachment::smtp_attach_file_config`), `IMAPClient::fetch()` and `IMAPClient::fetchUID()` for file operations.

The parametes `file`, `filename`, and `mode` are provided. The `file` is the reference to the uninitialized `File` class object that defined internally in the `SMTPClient` and `IMAPClient` classes.

User needs to assign his static or global defined `File` class object to this `file` reference.

The `filename` provides the path and name of file to be processed.

The `mode` is the `readymail_file_operating_mode` enum i.e. `readymail_file_mode_open_read`, `readymail_file_mode_open_write`, `readymail_file_mode_open_append` and `readymail_file_mode_remove`.

Note that `FILE_OPEN_MODE_READ`, `FILE_OPEN_MODE_WRITE` and `FILE_OPEN_MODE_APPEND` are macros that are defined in this library for demonstation purpose.

User may need to assign his own file operations code (read, write, append and remove) in the `FileCallback` function.


## Plain Text Connection

To use library with plain connection (non-secure), the network besic client (Arduino Client derived class) can be assigned to the `SMTPClient` and `IMAPClient` classes constructors. The `ssl` option, the fifth param of `SMTPClient::connect()` and fourth param of `IMAPClient::connect()` should set to `false`.

```cpp
#include <Ethernet.h>

EthernetClient basic_client;
SMTPClient smtp(basic_client);

smtp.connect("smtp host", 25, "127.0.0.1", statusCallback, false /* non-secure */);

```
```cpp
#include <Ethernet.h>

EthernetClient basic_client;
IMAPClient imap(basic_client);

imap.connect("imap host", 143, statusCallback, false /* non-secure */);

```


## TLS Connection with STARTTLS

The SSL client that supports protocol upgrades (from plain text to encrypted) e.g. `WiFiClientSecure` in ESP32 v3.x and [ESP_SSLClient](https://github.com/mobizt/ESP_SSLClient) can be assigned to the `SMTPClient` and `IMAPClient` classes constructors.

The `TLSHandshakeCallback` function and `startTLS` boolean option should be set to the second and third parameters of `SMTPClient` and `IMAPClient` classes constructors.

The `success` param of the `TLSHandshakeCallback` function should be set to true inside the `TLSHandshakeCallback` when connection upgrades is finished before exiting the function otherwise the TLS handshake error will show.

In case [ESP_SSLClient](https://github.com/mobizt/ESP_SSLClient), the basic network client e.g. WiFiClient will be assigned to the `ESP_SSLClient::setClient()`. The second parameter of `ESP_SSLClient::setClient()` should set to `false` to let the [ESP_SSLClient](https://github.com/mobizt/ESP_SSLClient) starts connection to server in plain text mode.

The library issues the STARTTLS command to upgrade and calling the `TLSHandshakeCallback` function that is provided to perform the TLS handshake and get the referenced handshake status.

The port 587 is for SMTP and 143 is for IMAP connections with STARTTLS.

```cpp

#include <ESP_SSLClient.h>
WiFiClient basic_client;
ESP_SSLClient ssl_client;

void tlsHandshakeCb(bool &success) { success = ssl_client.connectSSL(); }

SMTPClient smtp(ssl_client, tlsHandshakeCb, true /* STARTTLS */);

ssl_client.setClient(&basic_client, false /* start in plain connection */);
ssl_client.setInsecure();

smtp.connect("smtp host", 587, "127.0.0.1", statusCallback);

```
```cpp
#include <ESP_SSLClient.h>
WiFiClient basic_client;
ESP_SSLClient ssl_client;

void tlsHandshakeCb(bool &success) { success = ssl_client.connectSSL(); }

IMAPClient imap(ssl_client, tlsHandshakeCb, true /* STARTTLS */);

ssl_client.setClient(&basic_client, false /* start in plain connection */);
ssl_client.setInsecure();

imap.connect("imap host", 143, statusCallback);

```

## SSL Connection

The SSL client e.g. `WiFiClientSecure` and `WiFiSSLClient` can be assigned to the `SMTPClient` and `IMAPClient` classes constructors.

The `ssl` option, the fifth param of `SMTPClient::connect()` and fourth param of `IMAPClient::connect()` are set to `true` by default and can be disgarded.

```cpp
#include <WiFiClientSecure.h>

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

smtp.connect("smtp host", 465, "127.0.0.1", statusCallback);

```
```cpp
#include <WiFiClientSecure.h>

WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

imap.connect("imap host", 993, statusCallback);

```

<details>
<summary>Click here for using SSL client in all devices.</summary>

### Using WiFi Network

If user's Arduino boards have buit-in WiFi module or already equiped with WiFi capable MCUs, the platform's core SDK WiFi and netwowk (WiFi) SSL client libraries are needed.

**For ESP32**

```cpp
#include <WiFi.h>
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
```

**For ESP8266**

```cpp
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
```

**For Reaspberry Pi Pico W and 2 W**

```cpp
#include <WiFi.h>
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
```

**For Arduino® MKRx and Arduino® Nano RP2040**

Arduino® MKR WiFi 1010, Arduino® Nano 33 IoT, Arduino® MKR Vidor 4000

```cpp
#include <WiFiNINA.h>
WiFiSSLClient ssl_client;
```

**For Arduino® MKR 1000 WIFI**

```cpp
#include <WiFi101.h>
WiFiSSLClient ssl_client;
```

**For Arduino® UNO R4 WiFi (Renesas)**

```cpp
#include <WiFiS3.h>
#include <WiFiSSLClient.h>
WiFiSSLClient ssl_client;
```

**For Other Arduino WiFis**

Arduino® GIGA R1 WiFi, Arduino® OPTA etc.

```cpp
#include <WiFi.h>
#include <WiFiSSLClient.h>
WiFiSSLClient ssl_client;
```

### Using Ethernet Network

**For ESP32**

```cpp
#include <ETH.h>
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
```

To connect to the network, see [Ethernet examples](https://github.com/espressif/arduino-esp32/blob/master/libraries/Ethernet/examples).

**For ESP8266**

```cpp
#include <LwipEthernet.h>
// https://github.com/mobizt/ESP_SSLClient
#include <ESP_SSLClient.h>
Wiznet5500lwIP eth(16 /* Chip select pin */);
WiFiClient basic_client;
ESP_SSLClient ssl_client;
```
To connect to the network, see [this example](https://github.com/esp8266/Arduino/blob/master/libraries/lwIP_Ethernet/examples/EthClient/EthClient.ino).

To set up SSL client, see [Set Up ESP_SSLClient](#set-up-esp_sslclient).

**For Teensy Arduino**

```cpp
#include <SPI.h>
// https://github.com/PaulStoffregen/Ethernet
#include <Ethernet.h>
// https://github.com/mobizt/ESP_SSLClient
#include <ESP_SSLClient.h>
EthernetClient basic_client;
ESP_SSLClient ssl_client;
```
To connect to the network, see [this example](https://github.com/PaulStoffregen/Ethernet/blob/master/examples/WebClient/WebClient.ino).

To set up SSL client, see [Set Up ESP_SSLClient](#set-up-esp_sslclient).

**For STM32 Arduino**

```cpp
#include <Ethernet.h>
// https://github.com/mobizt/ESP_SSLClient
#include <ESP_SSLClient.h>
EthernetClient basic_client;
ESP_SSLClient ssl_client;
```

To set up SSL client, see [Set Up ESP_SSLClient](#set-up-esp_sslclient).

### Using GSM Network

```cpp
// https://github.com/vshymanskyy/TinyGSM
#include <TinyGsmClient.h>
// https://github.com/mobizt/ESP_SSLClient
#include <ESP_SSLClient.h>

TinyGsm modem(SerialAT);
TinyGsmClient basic_client;
ESP_SSLClient ssl_client;
```
To connect to the network, see [this example](https://github.com/vshymanskyy/TinyGSM/blob/master/examples/WebClient/WebClient.ino).

To set up SSL client, see [Set Up ESP_SSLClient](#set-up-esp_sslclient).

### Using PPP Network (ESP32)

```cpp
#include <PPP.h>
#include <WiFiClientSecure.h>
WiFiClientSecure ssl_client;
```
To connect to the network, see [this example](https://github.com/espressif/arduino-esp32/blob/master/libraries/PPP/examples/PPP_Basic/PPP_Basic.ino).


### Set Up ESP_SSLClient

If ESP_SSLClient library was used in some device that uses external network module e.g. `STM32` and `Teensy` or when STARTTLS protocol is needed, the network client e.g. basic Arduino client sould be assigned.

Some options e.g. insecure connection (server SSL certificate skipping) and protocol upgrades are available.

To start using with no SSL certificate, `ESP_SSLClient::setInsecure()` should be set.

```cpp
ssl_client.setClient(&basic_client);
ssl_client.setInsecure();
```

The `ESP_SSLClient` library requires 85k program space.

For using both `SMTP` and `IMAP` features with `ESP_SSLClient` will take approx. 170k program space.

</details>


# License #

The MIT License (MIT)

Copyright (c) 2025 K. Suwatchai (Mobizt)


Permission is hereby granted, free of charge, to any person returning a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.