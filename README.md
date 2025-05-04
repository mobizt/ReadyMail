![ESP Mail](/resources/images/readymail.png)

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

Starting the SMTP server connection first via `SMTPClient::connect` and providing the required parameters e.g. host, port, domain or IP and `SMTPResponseCallback` function.

Note that, the following code uses the lambda expression as the `SMTPResponseCallback` callback in `SMTPClient::connect`.

The SSL connection mode and await options are set true by default which can be changed via its parameters.

Then authenticate using `SMTPClient::authenticate` by providing the auth credentials and the type of authentication enum e.g. `readymail_auth_password`, `readymail_auth_access_token` and `readymail_auth_disabled`

Compose your message by adding the `SMTPMessage`'s headers and the text's body and html's body etc. 

Then, calling `SMTPClient::send` using the composed message to send the message.

The following example code is for ESP32 using it's ESP32 core `WiFi.h` and `WiFiClientSecure.h` libraries for network interface and SSL client.

The `ENABLE_SMTP` macro is required for using `SMTPClient` and `SMTPMessage` classes. The `ENABLE_DEBUG` macro is for allowing the processing information debugging.

You may wonder that when you change the SMTP port to something like 25 or 587, the error `Connection timed out` is occurred even the network connection and internet are ok. For the reason, plese see [Ports and Clients Selection](#ports-and-clients-selection) section.

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_SMTP
#define ENABLE_DEBUG
#include "ReadyMail.h"

// Please see https://github.com/mobizt/ReadyMail#ports-and-clients-selection
WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

ssl_client.setInsecure();

// WiFi or network connection here
// ...
// ...

auto statusCallback = [](SMTPStatus status){ Serial.println(status.text); };

smtp.connect("smtp host here", 465, "127.0.0.1", statusCallback);
if (smtp.isConnected())
{
    smtp.authenticate("sender email here", "sender email password here", readymail_auth_password);
    if (smtp.isAuthenticated())
    {
        SMTPMessage msg;
        msg.headers.add(rfc822_from, "ReadyMail <sender email here>");
        // msg.headers.add(rfc822_sender, "ReadyMail <sender email here>");
        msg.headers.add(rfc822_subject, "Greeting message");
        msg.headers.add(rfc822_to, "User1 <recipient email here>");
        msg.headers.add(rfc822_to, "User2 <another recipient email here>");
        msg.headers.add(rfc822_cc, "User3 <cc email here>");
        msg.text.body("Hello");
        msg.html.body("<html><body>Hello</body></html>");
        msg.timestamp = 1744951350; // The UNIX timestamp (seconds since Midnight Jan 1, 1970)
        smtp.send(msg);
    }
}
```

### Changes from v0.0.x to v0.1.0 and newer

Many SMTP classes and structs are refactored. The `SMTPMessage` public members are removed or kept private and the methods are added.

There are four structs that are public and access from the `SMTPMessage` class are `SMTPMessage::headers`, `SMTPMessage::text`, `SMTPMessage::html` and `SMTPMessage::attachments`.

The sender, recipients and subject are now provided using `SMTPMessage::headers::add()` function.

The `SMTPMessage::text` and `SMTPMessage::html`'s members are kept private and the new methods are added.

The content should set via `SMTPMessage::text::body()` and `SMTPMessage::html::body()` functions.

The attachments can be added with `SMTPMessage::attachments::add()` function.

The DSN option is added to the `SMTPClient::send()` function.

Plese check the library's [examples](/examples/Sending/) for the changes.


### SMTP Server Rejection and Spam Prevention

**Host/Domain or public IP**

User should provides the host name or you public IPv4 or IPv6 to the third parameter of `SMTPClient::connect()` function.
This information is the part of `EHLO/HELO` SMTP command to identify the client system to prevent connection rejection.

If host name or public IP is not available, ignore this or use the loopback address `127.0.0.1`. This library will set the loopback address `127.0.0.1` to the `EHLO/HELO` SMTP command for the case that user provides blank, invalid host and IP to this parameter.

**Date Header**

The message date header should be set to prevent spam mail. 

This library does not set the date header to SMTP message automatically unless system time was set in ESP8266 and ESP32 devices. 

User needs to set the message date by one of the following methods before sending the SMTP message.

Providing the RFC 2822 `Date` haeader with `SMTPMessage::headers.add(rfc822_date, "Fri, 18 Apr 2025 11:42:30 +0300")` or setting the UNIX timestamp with `SMTPMessage::timestamp = UNIX timestamp`. 

For ESP8266 and ESP32 devices as mentioned above the message date header will be auto-set, if the device system time was already set before sending the message.

In ESP8266 and ESP32, the system time is able to set with time from NTP server e.g. configTime(0, 0, "pool.ntp.org"); then wait until the time(nullptr) returns the valid timestamp, and library will use the system time for Date header setting.

In some Arduino devices that work with `WiFiNINA/WiFi101` firmwares, use `SMTPMessage::timestamp = WiFi.getTime();`

**Half Line-Break (LF)**

Depending on server policy, some SMTP server may reject the Email sending when the Lf (line feed) was used for line break in the content instead of CrLf (Carriage return + Line feed). 

Then we recommend using CrLf instead of Lf in the content to avoid this issue.


### SMTP Processing Information

The `SMTPStatus` is the struct of processing information which can be obtained from the `SMTPClient::status()` function.

This `SMTPStatus` is also available from the `SMTPResponseCallback` function that is assigned to the `SMTPClient::connect()` function.

The `SMTPResponseCallback` function provides the instant processing information.

The `SMTPResponseCallback` callback function will be called when:
1. The sending process infornation is available.
2. The file upload information is available.

When the `SMTPStatus::progressUpdated` value is `true`, the information is from file upload process,  otherwise the information is from other sending process.

When the sending process is finished, the `SMTPStatus::isComplete` value will be `true`.

When the `SMTPStatus::isComplete` value is `true`, user can check the `SMTPStatus::errorCode` value for the error. The [negative value](/src/smtp/Common.h#L6-L12) means the error is occurred otherwise the sending process is finished without error.

The `SMTPStatus::statusCode` value provides the SMTP server returning status code.

The `SMTPStatus::text` value provieds the status details which includes the result of each process state.

The code example below shows how the `SMTPStatus` information is used. 

```cpp
void smtpStatusCallback(SMTPStatus status)
{
    // For debugging.

    // Showing the uploading info.
    if (status.progressUpdated) 
        ReadyMail.printf("State: %d, Uploading file %s, %d %% completed\n", status.state, status.filename.c_str(), status.progress);
     // otherwise, showing the process state info.
    else
        ReadyMail.printf("State: %d, %s\n", status.state, status.text.c_str());

    // To check the sending result when sending is completed.
    if(status.isComplete)
    {
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
}
```

## Email Reading

To receive or fetch the Email, only `IMAPClient` calss object is required.

The `IMAPDataCallback` and `FileCallback` functions can be assigned to the `IMAPClient::fetch` and `IMAPClient::fetchUID` functions.

The `IMAPDataCallback` function provides the envelope (headers) and content stream of fetching message while the `FileCallback` function provides the file downloading capability.

The size of content that allows for downloading or content streaming can be set.

The processes for server connection and authentication for `IMAPClient` are the same as `SMTPClient` except for no domain or IP requires in the `IMAPClient::connect` method.

The mailbox must be selected before fetching or working with the messages.

The following example code is for ESP32 using it's ESP32 core `WiFi.h` and `WiFiClientSecure.h` libraries for network interface and SSL client.

You may wonder that when you change the IMAP port to something like 143, the error `Connection timed out` is occurred even the network connection and internet are ok. For the reason, plese see [Ports and Clients Selection](#ports-and-clients-selection) section.

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_IMAP
#define ENABLE_DEBUG
#include "ReadyMail.h"

// Please see https://github.com/mobizt/ReadyMail#ports-and-clients-selection
WiFiClientSecure ssl_client;
IMAPClient imap(ssl_client);

ssl_client.setInsecure();

// WiFi or network connection here
// ...
// ...

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text);};
auto dataCallback = [](IMAPCallbackData data)
{
    if (data.isEnvelope) // For showing message headers.
    {
        for (size_t i = 0; i < data.header.size(); i++)
        ReadyMail.printf("%s: %s\n%s", data.header[i].first.c_str(), data.header[i].second.c_str(), (i == data.header.size() - 1 ? "\n" : ""));
    }
};

imap.connect("imap host here", 993, statusCallback);
if (imap.isConnected())
{
    imap.authenticate("sender email here", "sender email password here", readymail_auth_password);
    if (imap.isAuthenticated())
    {
        imap.list(); // Optional. List all mailboxes.
        imap.select("INBOX"); // Select the mailbox to fetch its message.
        imap.fetch(imap.getMailbox().msgCount, dataCallback, NULL /* FileCallback */);
    }
}
```

The library provides the simple IMAP APIs for idling (mailbox polling), searching and fetching the messages. If additional works are needed e.g. setting and deleting flags, or creating, moving and deleting folder, or copying, moving and deleting mssage etc., those taks can be done through the `IMAPClient::sendCommand()`. 

The [Command.ino](/examples/Reading/Command/Command.ino) example show how to do those works. The server responses from sending the command will be discussed in the [IMAP Custom Comand Processing Information](#imap-custom-comand-processing-information) section below.


### IMAP Processing Information

The `IMAPStatus` is the struct of processing information which can be obtained from the `IMAPClient::status()` function.

This `IMAPStatus` is also available from the `IMAPResponseCallback` function that is assigned to the `IMAPClient::connect()` function.

The `IMAPResponseCallback` function provides the instant processing information.

When the IMAP function process is finished, the `IMAPStatus::isComplete` value will be `true`.

When the `IMAPStatus::isComplete` value is `true`, user can check the `IMAPStatus::errorCode` value for the error. The [negative value](/src/imap/Common.h#L9-L18) means the error is occurred otherwise the sending process is finished without error.

The `IMAPStatus::text` value provieds the status details which includes the result of each process state.

The code example below shows how the `IMAPStatus` information is used. 

```cpp
void imapStatusCallback(IMAPStatus status)
{
    ReadyMail.printf("State: %d, %s\n", status.state, status.text.c_str());
}
```

### IMAP Enveloping Data and Content Stream

The `IMAPDataCallback` function provides the stream of content that is currently fetching.

The data sent to the `IMAPDataCallback` consists of envelope data and content stream.

Those data and information are available from `IMAPCallbackData` struct.

**Envelope Data**

The envelope or headers information is available when the `IMAPDataCallback` function is assigned to the search and fetch functions.

The following envelope information is avaliable when `IMAPCallbackData::isEnvelope` value is `true`.

The `IMAPCallbackData::header` object provides the list of message headers (name and value) at `IMAPCallbackData::currentMsgIndex` of the message numbers/UIDs list.

The additional information below is available when `IMAPCallbackData::isSearch` value is `true`.

The `IMAPCallbackData::searchCount` value provides the total messages found (available when `IMAPCallbackData::isSearch` value is `true`).

The `IMAPCallbackData::msgList` provides the array or list of message numbers/UIDs.

The list of message in case of search, can also be obtained from `IMAPClient::searchResult()` function.

The maximum numbers of messages that can be stored in the list and the soring order can be set via the second param (`searchLimit`) and third param (`recentSort`) of `IMAPClient::search()` function.

The `IMAPCallbackData::currentMsgIndex` value provides the index of message in the list.

**Content Stream**

The following chunked data and its information are avaliable when `IMAPCallbackData::isEnvelope` value is `false`.

The `IMAPCallbackData::blob`provides the chunked data.

The `IMAPCallbackData::dataIndex`provides the position of chunked data in `IMAPCallbackData::size`. 

The `IMAPCallbackData::dataLength` provides the length in byte of chunked data. 

The `IMAPCallbackData::size` provides the number of bytes of complete data. This size will be zero in case of `text/plain` and `text/html` content type due to the issue of incorrect non-multipart octets/transfer-size field of text part reports by some IMAP server.

If the content stream with `text/plain` and `text/html` types are need be stored in memory, the memory allocation should be re-allocate for incoming chunked data.

The `IMAPCallbackData::mime` provides the content MIME type. 

The `IMAPCallbackData::filename` provies the content name or file name.

Both `IMAPCallbackData::mime` and `IMAPCallbackData::filename` can be used for file identifier.

There is no total numbers of chunks information provided. Then zero from `IMAPCallbackData::dataIndex` value means the chunked data that sent to the callback is the first chunk while the last chunk is delivered when `IMAPCallbackData::isComplete` value is `true`. 

For OTA firmware update implementation, the chunked data and its information can be used. See [OTA.ino](/examples/Reading/OTA/OTA.ino) example for OTA update usage.

When the `IMAPCallbackData::progressUpdated` value is `true`, the information that set to callback contains the progress of content fetching. Because of `IMAPCallbackData::size` will be zero for `text/plain` and `text/html` types content, the progress of this type of content fetching will not available.

This progress (percentage) information of content fetching is optional and will be available only when user fetched the message.

The code below shows how to get the content stream and information from the `IMAPCallbackData` data in the `DataCallback` function.

```cpp
void dataCallback(IMAPCallbackData data)
{
    // Showing envelope data.
    if (data.isEnvelope)
    {
        // Additional search info
        if (data.isSearch)
            ReadyMail.printf("Showing Search result %d (%d) of %d from %d\n\n", data.currentMsgIndex + 1, data.msgList[data.currentMsgIndex], data.msgList.size(), data.searchCount);
       
        // Headers data
        for (int i = 0; i < data.header.size(); i++)
            ReadyMail.printf("%s: %s\n%s", data.header[i].first.c_str(), data.header[i].second.c_str(), i == data.header.size() - 1 ? "\n" : "");
    }
    // Processing content stream.
    else
    {
        // Showing the progress of content fetching 
        if (data.progressUpdated)
            ReadyMail.printf("Downloading file %s, type %s, %d %%% completed", data.filename, data.mime, data.progress);

        ReadyMail.printf("Data Index: %d, Length: %d, Size: %d\n", data.dataIndex, data.dataLength, data.size);

        // The stream can be processed here.
        
    }
}
```

### IMAP Custom Comand Processing Information

The `IMAPCustomComandCallback` function which assigned to `IMAPClient::sendCommand()` function, provides the instance of `IMAPCommandResponse` for the IMAP command. The `IMAPCommandResponse` can also be obtained from `IMAPClient::getCmdResponse()`.

The `IMAPCommandResponse` consists of `IMAPCommandResponse::command`, `IMAPCommandResponse::text`, `IMAPCommandResponse::isComplete`, and `IMAPCommandResponse::errorCode`.

The `IMAPCommandResponse::command` provides the command of the response.  The `IMAPCommandResponse::text` provides the instance of untagged response when it obtains from `IMAPCustomComandCallback` callback function or represents all untagged server responses when it obtains from `IMAPClient::getCmdResponse()` function.

The `IMAPCommandResponse::isComplete` value will be true when the server responses are complete. When the `IMAPCommandResponse::isComplete` value is true, the `IMAPCommandResponse::errorCode` value can be used for error checking if its value is negative number.

The [Command.ino](/examples/Reading/Command/Command.ino) example showed how to use `IMAPClient::sendCommand()` to more with flags, message and folder or mailbox.


## Ports and Clients Selection

As the library works with external network/SSL client, the client that was selected, should work with protocols or ports that are used for the server connection.

The network client works only with plain text connection. Some SSL clients support only SSL connection while some SSL clients support plain text, ssl and connecion upgrades (`STARTTLS`).

Additional to the proper SSL client selected for the ports, the SSL client itself may require some additional settings before use.

Some SSL client allows user to use in insecure mode without server or Rooth CA SSL certificate verification e.g. using `WiFiClientSecure::setInsecure()` in ESP32 and ESP8266 `WiFiClientSecure.h`.

All examples in this library are for ESP32 for simply demonstation and `WiFiClientSecure` is used for SSL client and skipping for certificate verification by using `WiFiClientSecure::setInsecure()`

If server supports the SSL fragmentation and some SSL client supports SSL fragmentation by allowing user to setup the allocate IO buffers in any size, this allows user to operate the SSL client in smaller amount of RAM usage. Such SSL clients are ESP8266's `WiFiClientSecure` and `ESP_SSLClient`.

Some SsL client e.g. `WiFiNINA` and `WiFi101`, they require secure connection. The server or Rooth CA SSL certificate is required for verification during establishing the connection. This kind of SSL client works with device firmware that stores the list of cerificates in its firmware.

There is no problem when connecting to Google and Microsoft servers as the SSL Root certificate is already installed in firmware and it does not exire. The connection to other servers may be failed because of missing the server certificates. User needs to add or upload SSL certificates to the device firmware in this case.

In some use case where the network to connect is not WiFi but Ethernet or mobile GSM modem, if the SSL client is required, there are few SSL clients that can be used. One of these SSL client is `ESP_SSLClient`.

Back to our ports and clients selection, the following sections showed how to select proper ports and Clients based on the protocols.

### Plain Text Connection

In plain connection (non-secure), the network besic client (Arduino Client derived class) should be assigned to the `SMTPClient` and `IMAPClient` classes constructors instead of SSL client. The `ssl` option, the fifth param of `SMTPClient::connect()` and fourth param of `IMAPClient::connect()` should set to `false` for using in plain text mode.

*This may not support by many mail services and is blocked by many ISPs. Please use SSL or TLS instead*.

*Port 25 is for plain text or non-encryption mail treansfer and may be reserved for local SMTP Relay usage.*

**SMTP Port 25**
```cpp
#include <WiFiClient.h>

WiFiClient basic_client; // Network client
SMTPClient smtp(basic_client);

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text);};
smtp.connect("smtp host", 25, "127.0.0.1", statusCallback, false /* non-secure */);

```
**IMAP Port 143**
```cpp
#include <WiFiClient.h>

WiFiClient basic_client; // Network client
IMAPClient imap(basic_client);

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text);};
imap.connect("imap host", 143, statusCallback, false /* non-secure */);

```

### TLS Connection with STARTTLS

The SSL client that supports the protocol upgrades (from plain text to encrypted) is required.

There are two SSL clients that currently support protocol upgrades i.e. ESP32 v3 `WiFiClientSecure` and [ESP_SSLClient](https://github.com/mobizt/ESP_SSLClient).

The `TLSHandshakeCallback` function and `startTLS` boolean option should be assigned to the second and third parameters of `SMTPClient` and `IMAPClient` classes constructors.

Note that, when using [ESP_SSLClient](https://github.com/mobizt/ESP_SSLClient), the basic network client e.g. `WiFiClient`, `EthernetClient` and `GSMClient` sould be assigned to `ESP_SSLClient::setClient()` and the second parameter should be  `false` to start the connection in plain text mode.

The benefits of using [ESP_SSLClient](https://github.com/mobizt/ESP_SSLClient) are it supports all 32-bit MCUs, PSRAM and adjustable IO buffer while the only trade off is it requires additional 85k program space. 

When the TLS handshake is done inside the `TLSHandshakeCallback` function, the reference parameter, `success` should be set (`true`).

**SMTP Port 587 (ESP_SSLClient)**
```cpp
#include <WiFiClient.h>
#include <ESP_SSLClient.h>

WiFiClient basic_client;
ESP_SSLClient ssl_client;

auto startTLSCallback = [](bool &success){ success = ssl_client.connectSSL(); };
SMTPClient smtp(ssl_client, startTLSCallback, true /* start TLS */);

ssl_client.setClient(&basic_client, false /* starts connection in plain text */);
ssl_client.setInsecure();

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text);};
smtp.connect("smtp host", 587, "127.0.0.1", statusCallback);

```

**SMTP Port 587 (ESP32 v3 WiFiClientSecure)**
```cpp
#include <WiFiClientSecure.h>

WiFiClientSecure ssl_client;

auto startTLSCallback = [](bool &success){ success = ssl_client.startTLS(); };
SMTPClient smtp(ssl_client, startTLSCallback, true /* start TLS */);

ssl_client.setInsecure();
ssl_client.setPlainStart();

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text);};
smtp.connect("smtp host", 587, "127.0.0.1", statusCallback);

```

**IMAP Port 143 (ESP_SSLClient)**
```cpp
#include <WiFiClient.h>
#include <ESP_SSLClient.h>

WiFiClient basic_client;
ESP_SSLClient ssl_client;

auto startTLSCallback = [](bool &success){ success = ssl_client.connectSSL(); };
IMAPClient imap(ssl_client, startTLSCallback, true /* start TLS */);

ssl_client.setClient(&basic_client, false /* starts connection in plain text */);
ssl_client.setInsecure();

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text);};
imap.connect("imap host", 143, statusCallback);

```

**IMAP Port 143 (ESP32 v3 WiFiClientSecure)**
```cpp
#include <WiFiClientSecure.h>

WiFiClientSecure ssl_client;

auto startTLSCallback = [](bool &success){ success = ssl_client.startTLS(); };
IMAPClient imap(ssl_client, startTLSCallback, true /* start TLS */);

ssl_client.setInsecure();
ssl_client.setPlainStart();

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text);};
imap.connect("imap host", 143, statusCallback);

```

### SSL Connection

All SSL clients support this mode e.g. `ESP_SSLClient`, `WiFiClientSecure` and `WiFiSSLClient`.

The `ssl` option, the fifth param of `SMTPClient::connect()` and fourth param of `IMAPClient::connect()` are set to `true` by default and can be disgarded.


**SMTP Port 465 (ESP_SSLClient)**
```cpp
#include <WiFiClient.h> // Network client
#include <ESP_SSLClient.h>

WiFiClient basic_client;
ESP_SSLClient ssl_client;

SMTPClient smtp(ssl_client);

ssl_client.setClient(&basic_client);
ssl_client.setInsecure();

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text); };
smtp.connect("smtp host", 465, "127.0.0.1", statusCallback);

```

**SMTP Port 465 (WiFiClientSecure/WiFiSSLClient)**
```cpp
#include <WiFiClientSecure.h>
// #include <WiFiSSLClient.h>

WiFiClientSecure ssl_client;
// WiFiSSLClient ssl_client;
SMTPClient smtp(ssl_client);

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text); };
smtp.connect("smtp host", 465, "127.0.0.1", statusCallback);

```

**IMAP Port 993 (ESP_SSLClient)**
```cpp
#include <WiFiClient.h> // Network client
#include <ESP_SSLClient.h>

WiFiClient basic_client;
ESP_SSLClient ssl_client;

IMAPClient imap(ssl_client);

ssl_client.setClient(&basic_client);
ssl_client.setInsecure();

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text); };
imap.connect("imap host", 993, statusCallback);

```

**IMAP Port 993 (WiFiClientSecure/WiFiSSLClient)**
```cpp
#include <WiFiClientSecure.h>
// #include <WiFiSSLClient.h>

WiFiClientSecure ssl_client;
// WiFiSSLClient ssl_client;

IMAPClient imap(ssl_client);

auto statusCallback = [](IMAPStatus status){ Serial.println(status.text); };
imap.connect("imap host", 993, statusCallback);

```

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