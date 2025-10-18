#ifndef READY_STREAM_H
#define READY_STREAM_H

#include <Arduino.h>

#if defined(ENABLE_FS)
#include <FS.h>
#endif

// Platform-specific flash access
#if defined(ARDUINO_ARCH_AVR) || defined(ESP8266)
#include <pgmspace.h>
#define RD_READ_BYTE(p) pgm_read_byte(p)
#elif defined(ESP32) || defined(ARDUINO_ARCH_STM32)
#define RD_READ_BYTE(p) (*(p)) // direct access
#else
#define RD_READ_BYTE(p) (*(p)) // fallback
#endif

enum src_data_type
{
    src_data_string,
    src_data_static,
    src_data_file
};

enum smtp_content_xenc
{
    xenc_none,
    /* rfc2045 section 2.7 */
    xenc_7bit,
    xenc_qp,
    xenc_qp_flowed, // extension: flowed text variant
    xenc_base64,
    /* rfc2045 section 2.8 */
    xenc_8bit,
    /* rfc2045 section 2.9 */
    xenc_binary
};

// Source data context
struct src_data_ctx
{
    const uint8_t *src = nullptr;
    size_t src_len = 0;
#if defined(ENABLE_FS)
    File fs;
    char buf[READYMAIL_FILEBUF_SIZE];
    int buf_len = 0;
    int buf_pos = 0;
#endif
    src_data_type type = src_data_string;
    bool valid = false;
    bool cid = false;
    bool nonascii = false;
    bool utf8 = false;
    bool flowed = false;
    bool is_progmem = false;

    smtp_content_xenc xenc = xenc_none;
    String transfer_encoding = "7bit";

    char c = 0;
    int index = 0;

    // Diagnostics
    int bytes_read = 0;
    int seeks = 0;
    int lines_encoded = 0;
    int max_line_length = 0;

    inline bool available() const
    {
        if (type <= src_data_static)
            return index < (int)size();
#if defined(ENABLE_FS)
        else if (fs)
            return (buf_pos < buf_len) || fs.available();
#endif
        return false;
    }

    inline size_t size() const
    {
        if (type == src_data_static)
            return src_len;
        if (type == src_data_string)
            return src ? (is_progmem ? strlen_P(reinterpret_cast<const char *>(src)) : strlen(reinterpret_cast<const char *>(src))) : 0;
#if defined(ENABLE_FS)
        else if (fs)
            return fs.size();
#endif
        return 0;
    }

    inline uint8_t read()
    {
        if (!available())
            return 0;
        bytes_read++;
        if (type <= src_data_static)
        {
            return is_progmem ? RD_READ_BYTE(src + index++) : src[index++];
        }
#if defined(ENABLE_FS)
        else if (fs)
        {
            if (buf_pos >= buf_len)
            {
                buf_len = fs.read((uint8_t *)buf, READYMAIL_FILEBUF_SIZE);
                buf_pos = 0;
            }
            index++;
            return (buf_pos < buf_len) ? static_cast<uint8_t>(buf[buf_pos++]) : 0;
        }
#endif
        return 0;
    }

    inline uint8_t peek()
    {
        if (type <= src_data_static)
        {
            return is_progmem ? RD_READ_BYTE(src + index) : src[index];
        }
#if defined(ENABLE_FS)
        else if (fs)
        {
            if (buf_pos >= buf_len)
            {
                buf_len = fs.read((uint8_t *)buf, READYMAIL_FILEBUF_SIZE);
                buf_pos = 0;
            }
            return (buf_pos < buf_len) ? static_cast<uint8_t>(buf[buf_pos]) : 0;
        }
#endif
        return 0;
    }

    inline uint8_t peekN(int n)
    {
        if (type <= src_data_static)
        {
            return is_progmem ? RD_READ_BYTE(src + index + n) : src[index + n];
        }
#if defined(ENABLE_FS)
        else if (fs)
        {
            int pos = buf_pos + n;
            if (pos >= buf_len)
                return 0;
            return static_cast<uint8_t>(buf[pos]);
        }
#endif
        return 0;
    }

    inline int readN(uint8_t *out, int n)
    {
        int count = 0;
        while (available() && count < n)
        {
            out[count++] = read();
        }
        return count;
    }

    inline String readN(int n)
    {
        String out;
        for (int i = 0; i < n && available(); i++)
        {
            out += static_cast<char>(read());
        }
        return out;
    }

    inline void seek(int i)
    {
        if (type <= src_data_static)
        {
            index = i;
        }
#if defined(ENABLE_FS)
        else if (fs)
        {
            fs.seek(i);
            index = i;
            buf_len = 0;
            buf_pos = 0;
            seeks++;
        }
#endif
    }

    inline void close()
    {
#if defined(ENABLE_FS)
        if (type == src_data_file && fs)
            fs.close();
#endif
    }

    inline void reset()
    {
        index = 0;
        cid = false;
        nonascii = false;
        utf8 = false;
        flowed = false;
        bytes_read = 0;
        seeks = 0;
        lines_encoded = 0;
        max_line_length = 0;
#if defined(ENABLE_FS)
        buf_len = 0;
        buf_pos = 0;
#endif
    }

    inline void init_static(const uint8_t *data, size_t len, bool progmem = false)
    {
        src = data;
        src_len = len;
        type = src_data_static;
        index = 0;
        valid = true;
        is_progmem = progmem;
    }

    inline void init_static_progmem(const __FlashStringHelper *data, size_t len)
    {
        src = reinterpret_cast<const uint8_t *>(data);
        src_len = len;
        type = src_data_static;
        index = 0;
        valid = true;
        is_progmem = true;
    }

    // Ex:
    // const char msg[] PROGMEM = "This is from flash!";
    // src_data_ctx ctx;
    // RD_INIT_PROGMEM(ctx, msg);

#define RD_INIT_PROGMEM(ctx, str) \
    (ctx).init_static_progmem(reinterpret_cast<const __FlashStringHelper *>(str), strlen_P(str))
};

// Source analysis for encoding decisions
static inline void rd_src_check(src_data_ctx &ctx)
{
    ctx.nonascii = false;
    ctx.cid = false;
    ctx.utf8 = false;
    ctx.flowed = false;
    bool saw_space = false;
    bool saw_newline = false;
    ctx.seek(0);
    while (ctx.available())
    {
        uint8_t c = ctx.read();

        // UTF-8 detection
        if (c >= 0xC2 && c <= 0xF4)
        {
            uint8_t next = ctx.read();
            if (next >= 0x80 && next <= 0xBF)
            {
                ctx.utf8 = true;
                ctx.nonascii = true;
            }
            else
            {
                ctx.seek(ctx.index - 1);
            }
        }

        // Non-ASCII detection
        if (c < 32 || c > 126)
        {
            ctx.nonascii = true;
        }

        // CID detection
        if (c == 'c')
        {
            uint8_t i = ctx.read();
            uint8_t d = ctx.read();
            uint8_t colon = ctx.read();
            if (i == 'i' && d == 'd' && colon == ':')
            {
                ctx.cid = true;
            }
            else
            {
                ctx.seek(ctx.index - 3);
            }
        }

        // Flowed detection
        if (c == ' ')
        {
            saw_space = true;
        }
        else if (c == '\n' || c == '\r')
        {
            if (saw_space)
            {
                ctx.flowed = true;
                ctx.nonascii = true;
            }
            saw_space = false;
            saw_newline = true;
        }
        else
        {
            saw_space = false;
        }

        if (ctx.nonascii && ctx.cid && ctx.flowed)
            break;
    }
    ctx.seek(0);
}

#endif // READY_STREAM_H
