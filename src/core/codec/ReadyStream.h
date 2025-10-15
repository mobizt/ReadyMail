#ifndef READY_STREAM_H
#define READY_STREAM_H

#include <Arduino.h>

#if defined(ENABLE_FS)
#include <FS.h>
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
    xenc_base64,
    /* rfc2045 section 2.8 */
    xenc_8bit,
    /* rfc2045 section 2.9 */
    xenc_binary
};

struct src_data_ctx
{
    const char *str = nullptr;

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
    bool flowed = false;  // detected format=flowed

    smtp_content_xenc xenc = xenc_none; // base64, qp, 7bit
    String transfer_encoding = "7bit";  // fallback


    char c = 0;
    int index = 0;

    // Diagnostics
    int bytes_read = 0;
    int seeks = 0;
    int lines_encoded = 0;   // total number of encoded lines
    int max_line_length = 0; // longest line seen during encoding

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
        if (type <= src_data_static)
            return strlen(str);
#if defined(ENABLE_FS)
        else if (fs)
            return fs.size();
#endif
        return 0;
    }

    inline char read()
    {
        if (!available())
            return 0;

        bytes_read++;

        if (type <= src_data_static)
        {
            c = str[index++];
        }
#if defined(ENABLE_FS)
        else if (fs)
        {
            if (buf_pos >= buf_len)
            {
                buf_len = fs.read((uint8_t *)buf, READYMAIL_FILEBUF_SIZE);
                buf_pos = 0;
            }
            c = (buf_pos < buf_len) ? buf[buf_pos++] : 0;
            index++;
        }
#endif
        return c;
    }

    inline char peek()
    {
        if (type <= src_data_static)
        {
            return str[index];
        }
#if defined(ENABLE_FS)
        else if (fs)
        {
            if (buf_pos >= buf_len)
            {
                buf_len = fs.read((uint8_t *)buf, READYMAIL_FILEBUF_SIZE);
                buf_pos = 0;
            }
            return (buf_pos < buf_len) ? buf[buf_pos] : 0;
        }
#endif
        return 0;
    }

    inline char peekN(int n)
    {
        if (type <= src_data_static)
        {
            return str[index + n];
        }
#if defined(ENABLE_FS)
        else if (fs)
        {
            int pos = buf_pos + n;
            if (pos >= buf_len)
                return 0;
            return buf[pos];
        }
#endif
        return 0;
    }

    inline String readN(int n)
    {
        String out;
        for (int i = 0; i < n && available(); i++)
        {
            out += read();
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
        bytes_read = 0;
        seeks = 0;
        lines_encoded = 0;
        max_line_length = 0;
#if defined(ENABLE_FS)
        buf_len = 0;
        buf_pos = 0;
#endif
    }
};

// Content scanner for auto-encoding
static inline void rd_src_check(src_data_ctx &ctx) {
    ctx.nonascii = false;
    ctx.cid = false;
    ctx.utf8 = false;
    ctx.flowed = false;

    bool saw_space = false;
    bool saw_newline = false;

    ctx.seek(0);
    while (ctx.available()) {
        char c = ctx.read();

        // UTF-8 detection
        if ((uint8_t)c >= 0xC2 && (uint8_t)c <= 0xF4) {
            char next = ctx.read();
            if ((uint8_t)next >= 0x80 && (uint8_t)next <= 0xBF) {
                ctx.utf8 = true;
                ctx.nonascii = true;
            } else {
                ctx.seek(ctx.index - 1);
            }
        }

        // Non-ASCII detection
        if (c < 32 || c > 126) {
            ctx.nonascii = true;
        }

        // CID detection
        if (c == 'c') {
            char i = ctx.read();
            char d = ctx.read();
            char colon = ctx.read();
            if (i == 'i' && d == 'd' && colon == ':') {
                ctx.cid = true;
            } else {
                ctx.seek(ctx.index - 3);
            }
        }

        // Flowed detection: space before newline
        if (c == ' ') {
            saw_space = true;
        } else if (c == '\n' || c == '\r') {
            if (saw_space) {
                ctx.flowed = true;
                ctx.nonascii = true;  // force encoding
            }
            saw_space = false;
            saw_newline = true;
        } else {
            saw_space = false;
        }

        if (ctx.nonascii && ctx.cid && ctx.flowed) break;
    }

    ctx.seek(0);
}


#endif // READY_STREAM_H
