#ifndef READYMAIL_H
#define READYMAIL_H

#include <Arduino.h>
#include <array>
#include <vector>
#include <algorithm>
#include <time.h> 
#include <Client.h>
#include "./core/ReadyTimer.h"
#include "./core/ReadyCodec.h"
#include "./core/NumString.h"

#define READYMAIL_VERSION "0.0.4"

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

#if defined(ENABLE_FS)
#include <FS.h>
#endif

#if defined(ARDUINO_UNOWIFIR4) || defined(ARDUINO_MINIMA) || defined(ARDUINO_PORTENTA_C33)
#define READYMAIL_STRSEP strsepImpl
#define READYMAIL_USE_STRSEP_IMPL
#else
#define READYMAIL_STRSEP strsep
#endif

#define READYMAIL_VALID_TS 1577836800
#define FLOWED_TEXT_LEN 78
#define BASE64_CHUNKED_LEN 76

#define TCP_CLIENT_ERROR_CONNECTION -1
#define TCP_CLIENT_ERROR_NOT_CONNECTED -2
#define TCP_CLIENT_ERROR_CONNECTION_TIMEOUT -3
#define TCP_CLIENT_ERROR_TLS_HANDSHAKE -4
#define TCP_CLIENT_ERROR_SEND_DATA -5
#define TCP_CLIENT_ERROR_READ_DATA -6

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
#ifndef FLASH_SUPPORTS
#define FLASH_SUPPORTS
#endif

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

namespace ReadyMailCallbackNS
{
#if defined(ENABLE_FS)
    typedef void (*FileCallback)(File &file, const char *filename, readymail_file_operating_mode mode);
#else
    typedef void (*FileCallback)();
#endif
    typedef void (*TLSHandshakeCallback)(bool &success);
}

#if defined(ENABLE_IMAP)
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
typedef void (*DataCallback)(IMAPCallbackData data);

#include "imap/MailboxInfo.h"
#include "imap/IMAPClient.h"

#endif

#if defined(ENABLE_SMTP)
#include "smtp/SMTPMessage.h"
#include "smtp/SMTPClient.h"
#endif

class ReadyMailClass
{
public:
    ReadyMailClass() {};
    ~ReadyMailClass() {};

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

private:
};
ReadyMailClass ReadyMail;

#endif