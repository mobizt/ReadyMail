/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef READYMAIL_H
#define READYMAIL_H

#include <Arduino.h>
#if defined(ENABLE_FS)
#include <FS.h>
#endif

#if defined(ARDUINO_UNOWIFIR4) || defined(ARDUINO_MINIMA) || defined(ARDUINO_PORTENTA_C33)
#define READYMAIL_USE_STRSEP_IMPL
#endif

#include <array>
#include <vector>
#include <algorithm>
#include <time.h>
#include <Client.h>
#include "./core/ReadyTimer.h"
#include "./core/ReadyCodec.h"
#include "./core/Utils.h"

#define READYMAIL_VERSION "0.3.1"
#define READYMAIL_TIMESTAMP 1749196521
#define READYMAIL_LOOPBACK_IPV4 "127.0.0.1"

#if defined(READYMAIL_DEBUG_PORT)
#define READYMAIL_DEFAULT_DEBUG_PORT READYMAIL_DEBUG_PORT
#else
#define READYMAIL_DEFAULT_DEBUG_PORT Serial
#endif

#if defined(ENABLE_SMTP)
#define ENABLE_IMAP_APPEND
#endif

#if defined(ARDUINO_ARCH_RP2040)

#if defined(ARDUINO_NANO_RP2040_CONNECT)

#else
#ifndef ARDUINO_ARCH_RP2040_PICO
#define ARDUINO_ARCH_RP2040_PICO
#endif
#endif

#endif



#define MAX_LINE_LEN 76

#define TCP_CLIENT_ERROR_INITIALIZE -1
#define TCP_CLIENT_ERROR_CONNECTION -2
#define TCP_CLIENT_ERROR_NOT_CONNECTED -3
#define TCP_CLIENT_ERROR_CONNECTION_TIMEOUT -4
#define TCP_CLIENT_ERROR_TLS_HANDSHAKE -5
#define TCP_CLIENT_ERROR_SEND_DATA -6
#define TCP_CLIENT_ERROR_READ_DATA -7

#define AUTH_ERROR_UNAUTHENTICATE -200
#define AUTH_ERROR_AUTHENTICATION -201
#define AUTH_ERROR_OAUTH2_NOT_SUPPORTED -202

#if defined(ENABLE_FS)
#if __has_include(<FS.h>)
#include <FS.h>
#elif __has_include(<SD.h>) && __has_include(<SPI.h>)
#include <SPI.h>
#include <SD.h>
#else
#undef ENABLE_FS
#endif
#endif // ENABLE_FS

#if defined(ENABLE_FS)

#if (defined(ESP8266) || defined(CORE_ARDUINO_PICO)) || (defined(ESP32) && __has_include(<SPIFFS.h>))

#if !defined(FILE_OPEN_MODE_READ)
#define FILE_OPEN_MODE_READ "r"
#endif

#if !defined(FILE_OPEN_MODE_WRITE)
#define FILE_OPEN_MODE_WRITE "w"
#endif

#if !defined(FILE_OPEN_MODE_APPEND)
#define FILE_OPEN_MODE_APPEND "a"
#endif

#elif __has_include(<SD.h>) && __has_include(<SPI.h>)

#if !defined(FILE_OPEN_MODE_READ)
#define FILE_OPEN_MODE_READ FILE_READ
#endif

#if !defined(FILE_OPEN_MODE_WRITE)
#define FILE_OPEN_MODE_WRITE FILE_WRITE
#endif

#if !defined(FILE_OPEN_MODE_APPEND)
#define FILE_OPEN_MODE_APPEND FILE_WRITE
#endif

#endif // __has_include(<SD.h>) && __has_include(<SPI.h>)

#endif // ENABLE_FS

class ReadyMailClass
{
public:
    ReadyMailClass() {};
    ~ReadyMailClass() {};

    /** Printf
     */
    void printf(const char *format, ...)
    {
#if defined(READYMAIL_PRINTF_BUFFER)
        const int size = READYMAIL_PRINTF_BUFFER;
#else
        const int size = 1024;
#endif
        char s[size];
        va_list va;
        va_start(va, format);
        vsnprintf(s, size, format, va);
        va_end(va);
        READYMAIL_DEFAULT_DEBUG_PORT.print(s);
    }

    /** Provides date/time string
     *
     * @param ts The UNIX timestamp in seconds since midnight January 1, 1970.
     * @param format The date/time format e.g. "%a, %d %b %Y %H:%M:%S %z".
     * @return String of date/time.
     */
    String getDateTimeString(time_t ts, const char *format)
    {
        char tbuf[100];
        strftime(tbuf, 100, format, localtime(&ts));
        return tbuf;
    }

    /** Provides base64 encoded string
     *
     * @param str The string to convert to base64 string.
     * @return String of base64 encoding.
     */
    String base64Encode(const String &str)
    {
        String buf;
        char *enc = rd_b64_enc(rd_cast<const unsigned char *>(str.c_str()), str.length());
        if (enc)
        {
            buf = enc;
            rd_free(&enc);
        }
        return buf;
    }

    /** Provides base64 encoded string used for RFC 4616 PLAIN SASL mechanism
     *
     * @param email The email to convert to base64 PLAIN SASL string.
     * @param email The password to convert to base64 PLAIN SASL string.
     * @return String of base64 PLAIN SASL string.
     */
    String plainSASLEncode(const String &email, const String &password) { return rd_enc_plain(email, password); }

private:
};

ReadyMailClass ReadyMail;

enum readymail_auth_type
{
    readymail_auth_password,
    readymail_auth_accesstoken,
    readymail_auth_disabled
};

enum readymail_file_operating_mode
{
    readymail_file_mode_open_read,
    readymail_file_mode_open_write,
    readymail_file_mode_open_append,
    readymail_file_mode_remove
};

#if defined(READYCLIENT_SSL_CLIENT) && (defined(ENABLE_IMAP) || defined(ENABLE_SMTP))

#if __has_include(<EPS_SSLClient.h>) && !defined(READYCLIENT_TYPE_1)
#include <EPS_SSLClient.h>
#define READYCLIENT_TYPE_1
#endif

#if __has_include(<WiFiClientSecure.h>) && !defined(READYCLIENT_TYPE_2)
#include <WiFiClientSecure.h>
#define READYCLIENT_TYPE_2
#endif

#if defined(READYCLIENT_TYPE_1) || defined(READYCLIENT_TYPE_2)
#define ENABLE_READYCLIENT
#endif

enum readymail_protocol
{
    readymail_protocol_plain_text,
    readymail_protocol_ssl,
    readymail_protocol_tls
};

struct readymail_port_function
{
    uint16_t port;
    readymail_protocol protocol;
};
#include "./core/ReadyClient.h"
#endif

namespace ReadyMailCallbackNS
{
#if defined(ENABLE_FS)
    typedef void (*FileCallback)(File &file, const char *filename, readymail_file_operating_mode mode);
#else
    typedef void (*FileCallback)();
#endif
    typedef void (*TLSHandshakeCallback)(bool &success);
}

#include "./core/ReadyError.h"

#if defined(ENABLE_IMAP)
#include "imap/MailboxInfo.h"
#include "imap/IMAPClient.h"
#endif

#if defined(ENABLE_SMTP)
#include "smtp/SMTPMessage.h"
#include "smtp/SMTPClient.h"
#endif

#endif