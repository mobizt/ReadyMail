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

// Please see https://github.com/mobizt/ReadyMail#ports-and-clients-selection
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

### SMTP Processing Information

The `SMTPStatus` is the struct of processing information which can be obtained from the `SMTPClient::status()` function.

This `SMTPStatus` is also available from the `SMTPResponseCallback` function that is assigned to the `SMTPClient::connect()` function.

The `SMTPResponseCallback` function provides the instant processing information.

The `SMTPResponseCallback` callback function will be called when:
1. The sending process infornation is available.
2. The file upload information is available.

When the `SMTPStatus::progressUpdated` value is `true`, the information is from file upload process,  otherwise the information is from other sending process.

When the sending process is finished, the `SMTPStatus::isComplete` value will be `true`.

When `SMTPStatus::isComplete` value is `true`, user can check the `SMTPStatus::errorCode` value for the error. The [negative](/src/smtp/Common.h#L6-L12) value means the error is occurred otherwise the sending process is fishished without error.

The `SMTPStatus::statusCode` value provides the SMTP server returning status code.

The `SMTPStatus::text` value provieds the status details which includes the result of each process state.

The code example below shows how the `SMTPStatus` information are used. 

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

    if(status.complete)
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

The `IMAPDataCallback` function provides the envelope (headers) and content stream of fetching message while the `FileCallback` function provides the file downloading.

The size of content that allows for downloading or content streaming can be set.

The processes for server connection and authentication for `IMAPClient` are the same as `SMTPClient` except for no domain or IP requires in the `IMAPClient::connect` method.

The mailbox must be selected before fetching or working with the messages.

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

### IMAP Processing Information

The `IMAPStatus` is the struct of processing information which can be obtained from the `IMAPClient::status()` function.

This `IMAPStatus` is also available from the `IMAPResponseCallback` function that is assigned to the `IMAPClient::connect()` function.

The `IMAPResponseCallback` function provides the instant processing information.

When the IMAP function process is finished, the `IMAPStatus::isComplete` value will be `true`.

When `IMAPStatus::isComplete` value is `true`, user can check the `IMAPStatus::errorCode` value for the error. The [negative](/src/imap/Common.h#L9-L18) value means the error is occurred otherwise the sending process is fishished without error.

The `IMAPStatus::text` value provieds the status details which includes the result of each process state.

The code example below shows how the `IMAPStatus` information are used. 

```cpp
void imapStatusCallback(IMAPStatus status)
{
    ReadyMail.printf("State: %d, %s\n", status.state, status.text.c_str());
}
```

### IMAP Envelope Data and Content Stream

The `IMAPDataCallback` function provides the stream of content that is currently fetching.

The data sent to the `IMAPDataCallback` consists of envelope data and content stream.

Those data and information are available from `IMAPCallbackData` struct.

**Envelope Data**

The envelope or headers information is available when the `IMAPDataCallback` function is assigned to the search and fetch functions.

The following envelope information is avaliable when `IMAPCallbackData::isEnvelope` value is `true`.

The `header` object provides the list of message headers (name and value) at `currentMsgIndex` of the message numbers/UIDs list.

The additional information below is available when `IMAPCallbackData::isSearch` value is `true`.

The `IMAPCallbackData::searchCount` value provides the total messages found (available when `IMAPCallbackData::isSearch` value is `true`).

The `IMAPCallbackData::msgList` provides the array or list of message numbers/UIDs.

The list of message in case of search, can also be obtained from `IMAPClient::searchResult()` function.

The maximum numbers of messages that can be stored in the list and the soring order can be set via the second param (`searchLimit`) and third param (`recentSort`) of `IMAPClient::search()` function.

The `currentMsgIndex` value provides the index of message in the list.

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

For OTA firmware update implementation, the chunked data and its information can be used.

When the `IMAPCallbackData::progressUpdated` value is `true`, the information that set to callback contains the progress of content fetching. Because of `IMAPCallbackData::size` will be zero for `text/plain` and `text/html` types content, the progress of this type of content fetching will not available.

This progress (percentage) information of content fetching is optional and will be available only when user fetched the message.


The code below shows how to get the data and information from the `IMAPCallbackData` data in the `DataCallback` function.

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

The `IMAPCustomComandCallback` function which used with `IMAPClient::sendCommand()`, provides two parameters e.g. `cmd` that shows the current command and `response` that shows the untagged response of command except for the tagged responses that contains `OK`, `NO` and `BAD`.


## Ports and Clients Selection

When selecting some ports e.g. 587 for SMTP and 143 for IMAP, the proper network/SSL client should be used. The following sections showed how to select proper ports and Clients based on protocols.

### Plain Text Connection

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


### TLS Connection with STARTTLS

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

### SSL Connection

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