![ESP Mail](/resources/images/readymail.png)

![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/mobizt/ReadyMail/.github%2Fworkflows%2Fcompile_library.yml?logo=github&label=compile) [![Github Stars](https://img.shields.io/github/stars/mobizt/ReadyMail?logo=github)](https://github.com/mobizt/ESP-Mail-Client/stargazers) ![Github Issues](https://img.shields.io/github/issues/mobizt/ReadyMail?logo=github)

![arduino-library-badge](https://www.ardu-badge.com/badge/ReadyMail.svg) ![PlatformIO](https://badges.registry.platformio.org/packages/mobizt/library/ReadyMail.svg)

The fast and lightweight async email client library for Arduino that built from scratch. This library seamlessly works in async and await modes with simple API. The chunk data is processed for decodings, encodings, parsing, uploading and downloading then it will not block your other task in the same MCU core while working in async mode.

For sending email, the typical inline images and attachments plus the RFC 822 messages are supported.

For fetching email, this library provides the ready to use decoded data (headers, text body and attchments) for downloading and post processing.

This library supports all 32-bit `Arduino` devices e.g. `STM32`, `SAMD`, `ESP32`, `ESP8266`, `Raspberry Pi RP2040`, and `Renesas` devices except for 8-bit `Atmel's AVR` devices.


## Send Email

To send an email message, user needs to defined the `SMTPClient` and `SMTPMessage` class objects.

The one of SSL client if you are sending email over SSL/TLS or basic network client should be set for the `SMTPClient` class constructor.

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

Sending the large html or text message is availble, see the [StaticText.ino](/examples/Sending/StaticText/StaticText.ino) example.

For the error `Connection timed out`, plese see [Ports and Clients Selection](#ports-and-clients-selection) section for details.

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

smtp.connect("smtp host here", 465, statusCallback);
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

### Changes from v0.3.0 and newer

Normally, the global defined `SMTPMessage` is required for async Email sending.

Since v0.3.0, the internal `SMTPMessage` is required for async Email sending instead. To use internal `SMTPMessage` for async Email sending, the `SMTPMessage::getMessage()` is required for accessing the internal `SMTPMessage`. The following code is recommended for setting up the message.

```cpp
// The internal message will be used for async Email sending.
SMTPMessage &msg = smtp.getMessage();

msg.headers.add(rfc822_subject, "Hello");
```

### Changes from v0.1.x to v0.2.0 and newer

The `IMAPCallbackData` members are totally changed and cannot migrate from the old code. The `IMAPCallbackData::event()`, `imap_file_info`, `imap_file_chunk` and `imap_file_progress` are introduced.

The `SMTPStatus` class members are moved and renamed. The `SMTPStatus::progressUpdated` is moved and renamed to `SMTPStatus::smtp_file_progress::available`, `SMTPStatus::progress` is moved and renamed to `SMTPStatus::smtp_file_progress::value` and `SMTPStatus::filename` is moved to `SMTPStatus::smtp_file_progress::filename`.

The `STARTTLS` options can be changed directly from `SMTPClient` and `IMAPClient` classes function.


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

**Host/Domain or Public IP**

User should provides the host name or his public IPv4 or IPv6 to the third parameter of `SMTPClient::connect()` function.

This information is the part of `EHLO/HELO` SMTP command to identify the client system to prevent connection rejection.

If host name or public IP is not available, ignore this or use the loopback address `127.0.0.1`. This library will set the loopback address `127.0.0.1` to the `EHLO/HELO` SMTP command for the case that user provides blank, invalid host and IP to this parameter or setting `SMTPResponseCallback` as the third parameter.

Note that, for general use case, the domain/ip was ignored by setting `SMTPResponseCallback` as the third parameter of `SMTPClient::connect()` in all library SMTP examples and example codes that showed in this document.


**Date Header**

The message `Date` header should be set to prevent spam mail. 

This library does not set the `Date` header to SMTP message automatically unless system time was set in ESP8266 and ESP32 devices. 

User needs to set the message `Date` by one of the following methods before sending the SMTP message.

Providing the RFC 2822 `Date` haeader with `SMTPMessage::headers.add(rfc822_date, "Fri, 18 Apr 2025 11:42:30 +0300")` or setting the UNIX timestamp with `SMTPMessage::timestamp = UNIX timestamp`. 

For ESP8266 and ESP32 devices that mentioned above, the message `Date` header will be auto-set, if the device system time was already set before sending the message.

In ESP8266 and ESP32, the system time is able to set e.g. using configTime(0, 0, "pool.ntp.org"); then wait until the time(nullptr) returns the valid timestamp, and library will use the system time for `Date` header setting.

In some Arduino devices that work with `WiFiNINA/WiFi101` firmwares, use `SMTPMessage::timestamp = WiFi.getTime();`

**Half Line-Break**

Depending on server policy, some SMTP server may reject the email sending when the Lf (line feed) was used for line break in the content instead of CrLf (Carriage return + Line feed). 

Then we recommend using CrLf instead of Lf in the content to avoid this issue.

```cpp
// Recommend
String text1 = "Line 1 text.\r\nLine 2 text.\r\nLine 3 text.";

// Not recommend
String text2 = "Line 1 text.\nLine 2 text.\nLine 3 text.";
```


### SMTP Processing Information

The `SMTPStatus` is the struct of processing information which can be obtained from the `SMTPClient::status()` function.

This `SMTPStatus` is also available from the `SMTPResponseCallback` function that is assigned to the `SMTPClient::connect()` function.

The `SMTPResponseCallback` function provides the instant processing information.

The `SMTPResponseCallback` callback function will be called when:
1. The sending process infornation is available.
2. The file upload information is available.

When the `SMTPStatus::smtp_file_progress::available` value is `true`, the information is from file upload process,  otherwise the information is from other sending process.

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
    if (status.progress.available) 
        ReadyMail.printf("State: %d, Uploading file %s, %d %% completed\n", status.state, 
                         status.progress.filename.c_str(), status.progress.value);
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


### SMTP Custom Comand Processing Information

The `SMTPCustomComandCallback` function which assigned to `SMTPClient::sendCommand()` function, provides the instance of `SMTPCommandResponse` for the SMTP command. The `SMTPCommandResponse` can also be obtained from `SMTPClient::commandResponse()`.

The `SMTPCommandResponse` consists of `SMTPCommandResponse::command`, `SMTPCommandResponse::text`, `SMTPCommandResponse::statusCode`, and `IMAPCommandResponse::errorCode`.

The `SMTPCommandResponse::command` provides the command of the response.  The `SMTPCommandResponse::text` provides the instance of response when it obtains from `SMTPCustomComandCallback` callback function or represents all untagged server responses when it obtains from `SMTPClient::commandResponse()` function.

The `SMTPCommandResponse::errorCode` value can be used for error checking if its value is negative number.

The `SMTPCommandResponse::statusCode` value provides the SMTP server response code.

The [Command.ino](/examples/Sending/Command/Command.ino) example showed how to use `SMTPClient::sendCommand()` to work with flags, message and folder or mailbox.


## Receive Email

The IMAP protocol supports fetching, searching and idling. The idle and fetch functions can be used to get the incoming email alert.

While serach function with keywords also provides the mean to get the specific messages that match the search criteria e.g. unread, new, and flagged messages.

To search, receive or fetch the email, only `IMAPClient` calss object is required.

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
    if (data.event() == imap_data_event_search || data.event() == imap_data_event_fetch_envelope)
    {
         for (int i = 0; i < data.headerCount(); i++)
            ReadyMail.printf("%s: %s\n%s", data.getHeader(i).first.c_str(), 
                             data.getHeader(i).second.c_str(), i == data.headerCount() - 1 ? "\n" : "");
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

The [Command.ino](/examples/Reading/Command/Command.ino) example shows how to do those works. The server responses from sending the command will be discussed in the [IMAP Custom Comand Processing Information](#imap-custom-comand-processing-information) section below.


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

### IMAP Envelope and Body Data

The `IMAPDataCallback` function provides the body or multi-part body that is currently fetching.

The data sent to the `IMAPDataCallback` consists of envelope data and body data.

Those data and information are available from `IMAPCallbackData` struct.

**Envelope Data**

The envelope or headers information is available when the `IMAPDataCallback` function is assigned to the search and fetch functions.

The following envelope information is avaliable when `IMAPCallbackData::event()` value is `imap_data_event_search` or `imap_data_event_fetch_envelope`.

The `IMAPCallbackData::getHeader()` provides the list of message headers (name and value pair) at the index.

The `IMAPCallbackData::headerCount()` provides the number of headers that are available.

The additional information below is available when `IMAPCallbackData::event()` value is `imap_data_event_search`.

The `IMAPCallbackData::messageFound()` value provides the total messages that have been found.

The list of message in case of search, can be obtained from `IMAPClient::searchResult()` function.

The maximum numbers of messages that can be stored in the list and the soring order can be set via the second param (`searchLimit`) and third param (`recentSort`) of `IMAPClient::search()` function.

The `IMAPCallbackData::messageIndex()` value provides the index of message in the list.

In addition, the information of body or multi-part body is available during envenlope fetching via `IMAPCallbackData::fileInfo()`.

The `IMAPCallbackData::fileInfo().filename` provides the name of file.

The `IMAPCallbackData::fileInfo().mime` provides the mime type of file.

The `IMAPCallbackData::fileInfo().charset` provides the character set of text file.

The `IMAPCallbackData::fileInfo().transferEncoding` provides the content transfer encoding of file.

The `IMAPCallbackData::fileInfo().fileSize` provides the size in bytes of file.

The `IMAPCallbackData::fileCount()` provides the numbers of files that are available in current message.


**Body or Multi-Part Body Data**

The following `IMAPCallbackData::fileChunk()`, `IMAPCallbackData::fileInfo()` and `IMAPCallbackData::fileProgress()` are avaliable when `IMAPCallbackData::event()` value is `imap_data_event_fetch_body`.

The `IMAPCallbackData::fileChunk().data`provides the chunked data.

The `IMAPCallbackData::fileChunk().index`provides the position of chunked data in `IMAPCallbackData::fileInfo().fileSize`. 

The `IMAPCallbackData::fileChunk().size` provides the length in byte of the chunked data. 

The `IMAPCallbackData::fileInfo().fileSize` provides the number of bytes of complete data. This size will be zero in case of `text/plain` and `text/html` content type due to the issue of incorrect decoded size reports by some IMAP server.

There is no total numbers of chunks information provided. Then zero from `IMAPCallbackData::fileChunk().index` value means the chunked data that sent to the callback is the first chunk while the last chunk is delivered when `IMAPCallbackData::fileChunk().isComplete` value is `true`. 

For OTA firmware update implementation, the chunked data and its information can be used. See [OTA.ino](/examples/Reading/OTA/OTA.ino) example for OTA update usage.

When the `IMAPCallbackData::fileProgress().value` value is `true`, the information that set to the callback contains the progress of content that is fetching. Because of `IMAPCallbackData::fileInfo().fileSize` will be zero for `text/plain` and `text/html` file, the progress of this type of content fetching will not available.


The code below shows how to get the content stream and information from the `IMAPCallbackData` data in the `DataCallback` function.

```cpp
void dataCallback(IMAPCallbackData data)
{
    // Showing envelope data.
    if (data.event() == imap_data_event_search || data.event() == imap_data_event_fetch_envelope)
    {
        if (data.event() == imap_data_event_search)
            ReadyMail.printf("Showing Search result %d (%d) of %d from %d\n\n", data.messageIndex() + 1,
                             data.messageNum(), data.messageAvailable(), data.messageFound());

        // Headers data
        for (size_t i = 0; i < data.headerCount(); i++)
            ReadyMail.printf("%s: %s\n%s", data.getHeader(i).first.c_str(),
                             data.getHeader(i).second.c_str(), i == data.headerCount() - 1 ? "\n" : "");

        // Files data
        for (size_t i = 0; i < data.fileCount(); i++)
        {
            ReadyMail.printf("name: %s, mime: %s, charset: %s, trans-enc: %s, size: %d, fetch: %s%s\n",
                             data.fileInfo(i).filename.c_str(), data.fileInfo(i).mime.c_str(), data.fileInfo(i).charset.c_str(),
                             data.fileInfo(i).transferEncoding.c_str(), data.fileInfo(i).fileSize,
                             data.fetchOption(i) ? "yes" : "no", i == data.fileCount() - 1 ? "\n" : "");
        }

    }
    else if (data.event() == imap_data_event_fetch_body)
    {
        // Showing the progress of content fetching 
        if (data.fileProgress().available)
            ReadyMail.printf("Downloading file %s, type %s, %d %%% completed", data.fileInfo().filename, 
                             data.fileInfo().mime, data.fileProgress().value);

        ReadyMail.printf("Data Index: %d, Length: %d, Size: %d\n", data.fileChunk().index, data.fileChunk().size, data.fileInfo().fileSize);
        
    }
}
```

### IMAP Custom Comand Processing Information

The `IMAPCustomComandCallback` function which assigned to `IMAPClient::sendCommand()` function, provides the instance of `IMAPCommandResponse` for the IMAP command. The `IMAPCommandResponse` can also be obtained from `IMAPClient::commandResponse()`.

The `IMAPCommandResponse` consists of `IMAPCommandResponse::command`, `IMAPCommandResponse::text`, `IMAPCommandResponse::isComplete`, and `IMAPCommandResponse::errorCode`.

The `IMAPCommandResponse::command` provides the command of the response.  The `IMAPCommandResponse::text` provides the instance of untagged response when it obtains from `IMAPCustomComandCallback` callback function or represents all untagged server responses when it obtains from `IMAPClient::commandResponse()` function.

The `IMAPCommandResponse::isComplete` value will be true when the server responses are complete. When the `IMAPCommandResponse::isComplete` value is true, the `IMAPCommandResponse::errorCode` value can be used for error checking if its value is negative number.

The [Command.ino](/examples/Reading/Command/Command.ino) example showed how to use `IMAPClient::sendCommand()` to more with flags, message and folder or mailbox.


## Ports and Clients Selection

As the library works with external network/SSL client, the client that was selected, should work with protocols or ports that are used for the server connection.

The network client works only with plain text connection. Some SSL clients support only SSL connection while some SSL clients support plain text, ssl and connecion upgrades (`STARTTLS`).

*Additional to the proper SSL client selected for the ports, the SSL client itself may require some additional settings before use.*

*Some SSL client allows user to use in insecure mode without server or Rooth CA SSL certificate verification e.g. using `WiFiClientSecure::setInsecure()` in ESP32 and ESP8266 `WiFiClientSecure.h`.*

*All examples in this library are for ESP32 for simply demonstation and `WiFiClientSecure` is used for SSL client and skipping for certificate verification by using `WiFiClientSecure::setInsecure()`.*

*If server supports the SSL fragmentation and some SSL client supports SSL fragmentation by allowing user to allocate the IO buffers in any size, this allows user to operate the SSL client in smaller amount of RAM usage. Such SSL clients are ESP8266's `WiFiClientSecure` and `ESP_SSLClient`.*

*Some SSL client e.g. `WiFiNINA` and `WiFi101`, they require secure connection. The server or Rooth CA SSL certificate is required for verification during establishing the connection. This kind of SSL client works with device firmware that stores the list of cerificates in its firmware.*

*There is no problem when connecting to Google and Microsoft servers as the SSL Root certificate is already installed in firmware and it does not exire. The connection to other servers may be failed because of missing the server certificates. User needs to add or upload SSL certificates to the device firmware in this case.*

*In some use case where the network to connect is not WiFi but Ethernet or mobile GSM modem, if the SSL client is required, there are few SSL clients that can be used. One of these SSL client is `ESP_SSLClient`.*

Back to our ports and clients selection, the following sections showed how to select proper ports and Clients based on the protocols.

Anyway, this library also support port changing at run time, see [AutoPort.ino](/examples/Network/AutoPort/AutoPort.ino) and [AutoClient.ino](/examples/Network/AutoClient/AutoClient.ino) for how to.

The `ReadyClient` used in [AutoClient.ino](/examples/Network/AutoClient/AutoClient.ino) example is the SSL client wrapper class that allows user to assign the predefined ports and protocols which is easier than the method that is used in [AutoPort.ino](/examples/Network/AutoPort/AutoPort.ino) example. 

### Plain Text Connection

In plain connection (non-secure), the network besic client (Arduino Client derived class) should be assigned to the `SMTPClient` and `IMAPClient` classes constructors instead of SSL client. The `ssl` option, the fifth or forth param of `SMTPClient::connect()` and fourth param of `IMAPClient::connect()` should set to `false` for using in plain text mode.

There are two `SMTPClient::connect()` methods e.g. with domain and without domain params. Then the`ssl` option will be different in parameter orders.

*This may not support by many mail services and is blocked by many ISPs. Please use SSL or TLS instead*.

*Port 25 is for plain text or non-encryption mail treansfer and may be reserved for local SMTP Relay usage.*

**SMTP Port 25**
```cpp
#include <WiFiClient.h>

WiFiClient basic_client; // Network client
SMTPClient smtp(basic_client);

auto statusCallback = [](SMTPStatus status){ Serial.println(status.text);};
smtp.connect("smtp host", 25, statusCallback, false /* non-secure */);

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

*The benefits of using [ESP_SSLClient](https://github.com/mobizt/ESP_SSLClient) are it supports all 32-bit MCUs, PSRAM and adjustable IO buffer while the only trade off is it requires additional 85k program space.*

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

auto statusCallback = [](SMTPStatus status){ Serial.println(status.text);};
smtp.connect("smtp host", 587, statusCallback);

```

**SMTP Port 587 (ESP32 v3 WiFiClientSecure)**
```cpp
#include <WiFiClientSecure.h>

WiFiClientSecure ssl_client;

auto startTLSCallback = [](bool &success){ success = ssl_client.startTLS(); };
SMTPClient smtp(ssl_client, startTLSCallback, true /* start TLS */);

ssl_client.setInsecure();
ssl_client.setPlainStart();

auto statusCallback = [](SMTPStatus status){ Serial.println(status.text);};
smtp.connect("smtp host", 587, statusCallback);

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

auto statusCallback = [](SMTPStatus status){ Serial.println(status.text); };
smtp.connect("smtp host", 465, statusCallback);

```

**SMTP Port 465 (WiFiClientSecure/WiFiSSLClient)**
```cpp
#include <WiFiClientSecure.h>
// #include <WiFiSSLClient.h>

WiFiClientSecure ssl_client;
// WiFiSSLClient ssl_client;
SMTPClient smtp(ssl_client);

auto statusCallback = [](SMTPStatus status){ Serial.println(status.text); };
smtp.connect("smtp host", 465, statusCallback);

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

## Known Issues

**ESP8266 Issues**

When `ESP8266` device and its `WiFiClientSecure` library are used, they need some adjustments before using otherwise the device may crash when starting the connection.

The ESP8266's `WiFiClientSecure` requires some IO buffers adjustment. When it was used without IO buffer adjustment, it requires 17306 bytes for IO buffers (16k + 325 (overhead) for receive and 512 + 85 (overhead) for transmit).

If the mail server supports SSL fragmentation, the IO buffers can be set by using `WiFiClientSecure::setBufferSizes(rx-size, tx-size)`. In many cases, setting `WiFiClientSecure::setBufferSizes(1024, 1024)` is enough.

The `ESP8266` device itself, its Heap should be selected properly so that is enough for `WiFiClientSecure`, data and library memory usage.

By setting ESP8266 `MMU` options, from Arduino IDE, goto menu `Tools` > `MMU:` and select `16KB cache + 48 KB IRAM and 2nd Heap (shared)` (option 3).


In PlatformIO in `VSCode` IDE, the build flag set in the platformio.ini should be.

```ini
build_flags = -D PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48_SECHEAP_SHARED
```

The power supply should be robusted that provides enough current with low ripple and noise. The cable should provide good power e.g. short lenght and low impedance.

The library provides the wdt feed internally while operating in both sync and await modes.

Please note that library itself does not make your device to crash, the memory leak, memory allocation failure due to available memory is low and dangling pointer when the network/SSL client that assigned to the `SMTPClient` and `IMAPClient` does not exist in its usage scope, are the major causes of the crashes.

**ESP32 Issues**

This issue has occurred only in user code in ESP32 v3.x when using `NetworkClientSecure` in plain text mode.

Since ESP32 v3.0.0 RC1, the network protocols upgrade feature is added to the new `NetworkClientSecure` aka `WiFiClientSecure` class.

The issue that is from the commit [48072ee](https://github.com/espressif/arduino-esp32/commit/48072ee09802739cf4883c044e65bd1a77823038), when the `NetworkClienSecure` has stopped, if the `NetworkClienSecure::setPlainStart()` was called, the `NetworkClienSecure::connected()` will get stuck for 30 seconds.

Independent of networks, library uses `Client::connected()` to check the server status.

The issue is from [this line](https://github.com/espressif/arduino-esp32/blob/15e71a6afd21f9723a0777fa2f38d4c647279933/libraries/NetworkClientSecure/src/ssl_client.cpp#L455) where the write's `fdset` does not assign to the `lwIP::select()`. The result is it will get stuck at this line until the timeout is occurred.

[Update on 5/14/2025] This [PR](https://github.com/espressif/arduino-esp32/pull/11356) may fix this issue and the work around can be removed in the next library updates. 

The another issue is when `NetworkClienSecure::setPlainStart()` was called when the server is still connected and operated in the SSL mode, the unexpected error can be occurred due to all incoming and outgoing data will be treated as plain text.

Then using ESP32 `NetworkClienSecure` in plain text start mode at this time (6/30/2025) is not recommend for production.

**ESP_SSLClient Issues**

When you are using `ESP_SSLClient` in some devices e.g. Renesas devices (Arduino® `UNO R4 WiFi`) and SAMD devices (Arduino MKR WiFi 1010, Arduino MKR 1000 WIFI) etc., it will get stuck in TLS handshake process due to memory allocation failure.

If your mail server supports SSL fragmentation e.g. Gmail, you should set the size for receive and transmit buffers that is suitable for your device.

For Gmail usage, setting 1024 bytes for both buffers i.e. `ESP_SSLClient::setBufferSizes(1024, 1024)` is enough. 

In addition, please don't forget to set the network client with `ESP_SSLClient::setClient()` and also `ESP_SSLClient::setInsecure()` when the server SSL certificate was not assigned for verification.

Anyway, for IMAP usage, the memory allocation may fail and get stuck when fetching the message body that requires more memory in the process, the core SSL client i.e. `WiFiSSLClient` is recommeneded for those devices in this case.

The `ESP_SSLClient` library contains the large anount of C files from `BearSSL` engine. This can increase the compilation time in Arduino IDE compilation due to anti virus software interference.

**IMAP Response Parsing**

The complete responses of the multi-line, large IMAP's headers, envelope and body structures are required especially when decodings and nested list parsing. Parsing or decoding the partial/chunked responses may give the incorrect information. 

The memory allocation for those responses buffer may fail in some limited memory devices.

## License

The codes and examples in this repository are licensed under the MIT License. 

Copyright (c) 2025, Suwatchai K. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

*The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.*

`THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.`

*Last updated 2025-08-13 UTC.*