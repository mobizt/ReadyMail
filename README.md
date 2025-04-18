# ReadyMail #

The fast and lightweight async Email client library for Arduino.

This Email client library supports sending the Email and reading, searching and appending the mailbox messages.

All 32-bit MCU Arduino devices are supported including ESP32, ESP8266, Raspberry Pi Pico, Arduino MKRs etc.

# Examples #

To send an Email message, user needs to defined the `SMTPClient` and `SMTPMessage` class objects.

The one of SSL client if you are sending Email over SSL/TLS or basic network client should be set for the `SMTPClient` class constructor.

Note that the SSL client or network client assigned to the `SMTPClient` class object should be lived in the `SMTPClient` class object usage scope otherwise the error can be occurred.

The `STARTTLS` options can be set in the advance usage.

Starting the SMTP server connection first via `SMTPClient::connect` and providing the required parameters e.g. host, port, domain or IP and SMTPResponseCallback function.

Note that, the following code uses the lambda expression as the SMTPResponseCallback callback in `SMTPClient::connect`.

The SSL connection mode and await options are set true by default which can be change via its parameters.

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

smtp.connect("smtp host here", 465, "127.0.0.1", [](SMTPStatus status){ Serial.println(status.text);});
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
        msg.date = "Fri, 18 Apr 2025 11:42:30 +0300";
        smtp.send(msg);
    }
}
```

To receive or fetch the Email, only `IMAPClient` calss object is required. The received message will not store in device memory but redirects to the callback function (`IMAPCallbackData`) for user processing.

This requires less memory than storing the message. The little memory is required for the chunked buffer that sent to the user callback function. 

User can compile or store the message by himself.

TWhen the `FileCallback` function was assigned to the `IMAPClient::fetch` or `IMAPClient::fetchUID` function, the content will be downloaded as files automatically.

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

auto dataCallback = [](IMAPCallbackData data)
{
    if (data.isEnvelope) // For showing message headers.
    {
        for (int i = 0; i < data.header.size(); i++)
        ReadyMail.printf("%s: %s\n%s", data.header[i].first.c_str(), data.header[i].second.c_str(), (i == data.header.size() - 1 ? "\n" : ""));
    }
};

imap.connect("imap host here", 993, [](IMAPStatus status){ Serial.println(status.text);});
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

The more features usages are available in [examples](/examples/) folder.


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