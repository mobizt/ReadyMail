#ifndef READY_CODEC_H
#define READY_CODEC_H
#include <Arduino.h>
#include <stdarg.h>
#include "QBDecoder.h"

#if defined(ENABLE_IMAP) || defined(ENABLE_SMTP)

static const unsigned char rd_base64_map[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void __attribute__((used)) sys_yield()
{
#if defined(ARDUINO_ESP8266_MAJOR) && defined(ARDUINO_ESP8266_MINOR) && defined(ARDUINO_ESP8266_REVISION) && ((ARDUINO_ESP8266_MAJOR == 3 && ARDUINO_ESP8266_MINOR >= 1) || ARDUINO_ESP8266_MAJOR > 3)
    esp_yield();
#else
    delay(0);
#endif
}

static void rd_release(void *buf)
{
    free(buf);
    buf = nullptr;
}

void rd_print_to(String &buff, int size, const char *format, ...)
{
    size += strlen(format) + 10;
    char *s = (char *)malloc(size);
    va_list va;
    va_start(va, format);
    vsnprintf(s, size, format, va);
    va_end(va);
    buff += (const char *)s;
    rd_release((void *)s);
}

static unsigned char rd_base64_lookup(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 71;
    if (c >= '0' && c <= '9')
        return c + 4;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

static void rd_a4_to_a3(unsigned char *a3, unsigned char *a4)
{
    a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4);
    a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2);
    a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
}

static uint8_t *rd_base64_decode_impl(const char *encoded, int &size)
{
    int encoded_len = strlen(encoded);
    int decoded_len = (encoded_len * 3) / 4;
    uint8_t *raw = (uint8_t *)malloc(decoded_len + 1);

    int i = 0, j = 0;
    int raw_len = 0;
    unsigned char a3[3];
    unsigned char a4[4];

    while (encoded_len--)
    {
        if (*encoded == '=')
            break;

        a4[i++] = *(encoded++);
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
                a4[i] = rd_base64_lookup(a4[i]);

            rd_a4_to_a3(a3, a4);
            for (i = 0; i < 3; i++)
                raw[raw_len++] = a3[i];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 4; j++)
            a4[j] = '\0';

        for (j = 0; j < 4; j++)
            a4[j] = rd_base64_lookup(a4[j]);

        rd_a4_to_a3(a3, a4);

        for (j = 0; j < i - 1; j++)
            raw[raw_len++] = a3[j];
    }
    size = raw_len;
    return raw;
}

static char *rd_base64_decode(const char *encoded)
{
    int len = 0;
    char *raw = (char *)rd_base64_decode_impl(encoded, len);
    raw[len] = '\0';
    return raw;
}

static char *rd_base64_encode(const unsigned char *raw, int len)
{
    uint8_t count = 0;
    char buffer[3];
    char *encoded = (char *)malloc(len * 4 / 3 + 4);
    int c = 0;
    for (int i = 0; i < len; i++)
    {
        buffer[count++] = raw[i];
        if (count == 3)
        {
            encoded[c++] = rd_base64_map[buffer[0] >> 2];
            encoded[c++] = rd_base64_map[((buffer[0] & 0x03) << 4) + (buffer[1] >> 4)];
            encoded[c++] = rd_base64_map[((buffer[1] & 0x0f) << 2) + (buffer[2] >> 6)];
            encoded[c++] = rd_base64_map[buffer[2] & 0x3f];
            count = 0;
        }
    }
    if (count > 0)
    {
        encoded[c++] = rd_base64_map[buffer[0] >> 2];
        if (count == 1)
        {
            encoded[c++] = rd_base64_map[(buffer[0] & 0x03) << 4];
            encoded[c++] = '=';
        }
        else if (count == 2)
        {
            encoded[c++] = rd_base64_map[((buffer[0] & 0x03) << 4) + (buffer[1] >> 4)];
            encoded[c++] = rd_base64_map[(buffer[1] & 0x0f) << 2];
        }
        encoded[c++] = '=';
    }
    encoded[c] = '\0';
    return encoded;
}

#if defined(ENABLE_SMTP)
static void rd_get_softbreak_index(const char *src, int index, int max_len, std::vector<int> &softbreak_index)
{
    int last_index = softbreak_index.size() ? softbreak_index[softbreak_index.size() - 1] + 1 : index;
    int i = last_index + max_len;
    if (i < (int)strlen(src))
    {
        while (i >= last_index)
        {
            if (src[i] == ' ' && i + 2 <= last_index + max_len)
            {
                softbreak_index.push_back(i);
                break;
            }
            i--;
        }
    }
}

static void rd_add_softbreak(String &buf, int index, std::vector<int> &softbreak_index, String &softbreak_buf, int line_max_len)
{
    for (size_t i = 0; i < softbreak_index.size(); i++)
    {
        if ((index == softbreak_index[i]))
        {
            softbreak_index.erase(softbreak_index.begin() + i);
            if ((int)buf.length() + 2 <= line_max_len)
                buf += "\r\n"; // to complete soft break
            else if ((int)buf.length() + 1 <= line_max_len)
            {
                buf += "\r";
                softbreak_buf = "\n";
            }
            else
                softbreak_buf = "\r\n";

            break;
        }
    }
}

#if defined(ENABLE_FS)

static void rd_get_softbreak_index_file(File &fs, int index, int max_len, std::vector<int> &softbreak_index)
{

    int last_index = softbreak_index.size() ? softbreak_index[softbreak_index.size() - 1] + 1 : index;
    int i = last_index + max_len;
    if (i < fs.size())
    {
        while (fs.available() && i >= last_index)
        {
            fs.seek(i);
            char c = fs.read();
            if (c == ' ' && i + 2 <= last_index + max_len)
            {
                softbreak_index.push_back(i);
                break;
            }
            i--;
        }
    }
}
#endif

static String rd_qp_encode_chunk(const char *src, int &index, bool flowed, int max_len, String &softbreak_buf, std::vector<int> &softbreak_index)
{

    String sbuf;
    int sindex = 0;
    int len = strlen(src);
    char *out = (char *)malloc(10);

    // Breaks the string at sp with soft linebreak for flowed text
    if (flowed)
    {
        rd_get_softbreak_index(src, index, max_len, softbreak_index);
        if (softbreak_buf.length())
            sbuf += softbreak_buf;
        softbreak_buf.remove(0, softbreak_buf.length());
    }

    for (int i = index; i < len; i++)
    {
        char c = src[i];
        char c1 = (i < len - 1) ? src[i + 1] : 0;

        if ((int)sbuf.length() >= max_len - 3 && c != 10 && c != 13)
        {
            sbuf += "=\r\n";
            goto out;
        }

        if (c == 10 || c == 13)
            sbuf += c;
        else if (c < 32 || c == 61 || c > 126)
        {
            memset(out, 0, 10);
            sprintf(out, "=%02X", (unsigned char)c);
            sbuf += out;
        }
        else if (c != 32 || (c1 != 10 && c1 != 13))
            sbuf += c;
        else
            sbuf += "=20";

        if (flowed)
            rd_add_softbreak(sbuf, i, softbreak_index, softbreak_buf, max_len - 3);

        sindex++;
    }
out:
    if (out)
        rd_release(out);
    index += sindex;
    return sbuf;
}

#if defined(ENABLE_FS)
static String rd_qp_encode_file(File &fs, int len, int &index, bool flowed, int max_len, String &softbreak_buf, std::vector<int> &softbreak_index)
{

    String sbuf;
    int sindex = 0;
    char *out = (char *)malloc(10);

    // Breaks the string at sp with soft linebreak for flowed text
    if (flowed)
    {
        rd_get_softbreak_index_file(fs, index, max_len, softbreak_index);
        if (softbreak_buf.length())
            sbuf += softbreak_buf;
        softbreak_buf.remove(0, softbreak_buf.length());
    }

    fs.seek(index);

    int c = 0, c1 = 0;

    while (fs.available() || c1 > 0)
    {
        c = c1 == 0 ? fs.read() : c1;
        c1 = fs.available() ? fs.read() : 0;

        if (sbuf.length() >= max_len - 3 && c != 10 && c != 13)
        {
            sbuf += "=\r\n";
            goto out;
        }

        if (c == 10 || c == 13)
            sbuf += (char)c;
        else if (c < 32 || c == 61 || c > 126)
        {
            memset(out, 0, 10);
            sprintf(out, "=%02X", (unsigned char)c);
            sbuf += out;
        }
        else if (c != 32 || (c1 != 10 && c1 != 13))
            sbuf += (char)c;
        else
            sbuf += "=20";

        if (flowed)
            rd_add_softbreak(sbuf, index + sindex, softbreak_index, softbreak_buf, max_len - 3);

        sindex++;
    }
out:
    if (out)
        rd_release(out);
    index += sindex;
    return sbuf;
}
#endif

static bool rd_is_non_ascii(const char *str)
{
    while (*str)
    {
        if ((unsigned char)*str > 127)
            return true;
        str++;
    }
    return false;
}

#if defined(ENABLE_FS)
static bool rd_is_non_ascii_file(File &fs)
{
    while (fs.available())
    {
        if ((unsigned char)fs.read() > 127)
        {
            fs.close();
            return true;
        }
    }
    fs.close();
    return false;
}
#endif

static String rd_qb_encode_chunk(const char *src, int &index, int mode, bool flowed, int max_len, String &softbreak_buf, std::vector<int> &softbreak_index)
{
    String line, buf;
    buf.reserve(100);
    line.reserve(100);
    int len = strlen(src), sindex = 0;
    if (mode == 2 /* xenc_qp */)
        line = rd_qp_encode_chunk(src, index, flowed, max_len, softbreak_buf, softbreak_index);
    else
    {
        // Breaks the string at sp with soft linebreak for flowed text
        if (flowed)
        {
            rd_get_softbreak_index(src, index, max_len, softbreak_index);
            if (softbreak_buf.length())
                buf += softbreak_buf;
            softbreak_buf.remove(0, softbreak_buf.length());
        }

        for (int i = index; i < len; i++)
        {
            if ((int)buf.length() < (mode == 3 /* xenc_base64 */ ? 57 : max_len))
            {
                buf += src[i];
                if (flowed)
                    rd_add_softbreak(buf, i, softbreak_index, softbreak_buf, (mode == 3 /* xenc_base64 */ ? 57 : max_len));
                continue;
            }
            sindex = i;
            break;
        }

        if (buf.length())
        {
            if (mode == 3 /* xenc_base64 */)
            {
                char *enc = rd_base64_encode((const unsigned char *)buf.c_str(), buf.length());
                line = enc;
                rd_release((void *)enc);
            }
            else
                line = buf;
            line += "\r\n";
            index = sindex > 0 ? sindex : len;
        }
    }
    return line;
}

#if defined(ENABLE_FS)
static String rd_qb_encode_file(File &fs, int len, int &index, int mode, bool flowed, int max_len, String &softbreak_buf, std::vector<int> &softbreak_index)
{
    String line, buf;
    buf.reserve(100);
    line.reserve(100);
    int sindex = 0;
    if (mode == 2 /* xenc_qp */)
        line = rd_qp_encode_file(fs, len, index, flowed, max_len, softbreak_buf, softbreak_index);
    else
    {
        // Breaks the string at sp with soft linebreak for flowed text
        if (flowed)
        {
            rd_get_softbreak_index_file(fs, index, max_len, softbreak_index);
            if (softbreak_buf.length())
                buf += softbreak_buf;
            softbreak_buf.remove(0, softbreak_buf.length());
        }

        fs.seek(index);
        while (fs.available())
        {
            if (buf.length() < (mode == 3 /* xenc_base64 */ ? 57 : max_len))
            {
                buf += (char)fs.read();
                if (flowed)
                    rd_add_softbreak(buf, index + sindex, softbreak_index, softbreak_buf, (mode == 3 /* xenc_base64 */ ? 57 : max_len));

                sindex++;
                continue;
            }
            break;
        }

        if (buf.length())
        {
            if (mode == 3 /* xenc_base64 */)
            {
                char *enc = rd_base64_encode((const unsigned char *)buf.c_str(), buf.length());
                line = enc;
                line += "\r\n";
                rd_release((void *)enc);
            }
            else
                line = buf;
            index = sindex > 0 ? index + sindex : len;
        }
    }

    return line;
}
#endif

#endif

String rd_enc_oauth(const String &email, const String &accessToken)
{
    String out;
    String raw;
    rd_print_to(raw, email.length() + accessToken.length() + 30, "user=%s\1auth=Bearer %s\1\1", email.c_str(), accessToken.c_str());
    char *enc = rd_base64_encode((const unsigned char *)raw.c_str(), raw.length());
    if (enc)
    {
        out = enc;
        rd_release((void *)enc);
    }
    return out;
}

String rd_enc_plain(const String &email, const String &password)
{
    // rfc4616
    String out;
    int len = email.length() + password.length() + 2;
    uint8_t *buf = (uint8_t *)malloc(len);
    memset(buf, 0, len);
    memcpy(buf + 1, email.c_str(), email.length());
    memcpy(buf + email.length() + 2, password.c_str(), password.length());
    char *enc = rd_base64_encode(buf, len);
    if (enc)
    {
        out = enc;
        rd_release((void *)enc);
    }
    rd_release((void *)buf);
    return out;
}

#if defined(ENABLE_IMAP)
static int rd_encode_unicode_utf8(char *out, uint32_t utf)
{
    if (utf <= 0x7F)
    {
        // Plain ASCII
        out[0] = (char)utf;
        out[1] = 0;
        return 1;
    }
    else if (utf <= 0x07FF)
    {
        // 2-byte unicode
        out[0] = (char)(((utf >> 6) & 0x1F) | 0xC0);
        out[1] = (char)(((utf >> 0) & 0x3F) | 0x80);
        out[2] = 0;
        return 2;
    }
    else if (utf <= 0xFFFF)
    {
        // 3-byte unicode
        out[0] = (char)(((utf >> 12) & 0x0F) | 0xE0);
        out[1] = (char)(((utf >> 6) & 0x3F) | 0x80);
        out[2] = (char)(((utf >> 0) & 0x3F) | 0x80);
        out[3] = 0;
        return 3;
    }
    else if (utf <= 0x10FFFF)
    {
        // 4-byte unicode
        out[0] = (char)(((utf >> 18) & 0x07) | 0xF0);
        out[1] = (char)(((utf >> 12) & 0x3F) | 0x80);
        out[2] = (char)(((utf >> 6) & 0x3F) | 0x80);
        out[3] = (char)(((utf >> 0) & 0x3F) | 0x80);
        out[4] = 0;
        return 4;
    }
    else
    {
        // error - use replacement character
        out[0] = (char)0xEF;
        out[1] = (char)0xBF;
        out[2] = (char)0xBD;
        out[3] = 0;
        return 0;
    }
}

static void rd_decode_tis620_utf8(char *out, const unsigned char *in, size_t len)
{
    // output is the 3-byte value UTF-8
    int j = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (in[i] < 0x80)
            out[j++] = in[i];
        else if ((in[i] >= 0xa0 && in[i] < 0xdb) || (in[i] > 0xde && in[i] < 0xfc))
        {
            int unicode = 0x0e00 + in[i] - 0xa0;
            char o[5];
            memset(o, 0, 5);
            int r = rd_encode_unicode_utf8(o, unicode);
            for (int x = 0; x < r; x++)
                out[j++] = o[x];
        }
    }
}

static int rd_decode_char(const char *s) { return 16 * hexval(*(s + 1)) + hexval(*(s + 2)); }

static void rd_decode_qp_utf8(const char *src, char *out)
{
    const char *tmp = "0123456789ABCDEF";
    int idx = 0;
    while (*src)
    {
        if (*src != '=')
            out[idx++] = *src++;
        else if (*(src + 1) == '\r' && *(src + 2) == '\n')
            src += 3;
        else if (*(src + 1) == '\n')
            src += 2;
        else if (!strchr(tmp, *(src + 1)))
            out[idx++] = *src++;
        else if (!strchr(tmp, *(src + 2)))
            out[idx++] = *src++;
        else
        {
            out[idx++] = rd_decode_char(src);
            src += 3;
        }
    }
}

static char *rd_decode_7bit_utf8(const unsigned char *src)
{
    String s;

    // only non NULL and 7-bit ASCII are allowed
    // rfc2045 section 2.7
    size_t len = src ? strlen((const char *)src) : 0;

    for (size_t i = 0; i < len; i++)
    {
        if (src[i] > 0 && src[i] < 128 && i < 998)
            s += src[i];
    }

    // some special chars can't send in 7bit unless encoded as queoted printable string
    char *decoded = (char *)malloc(s.length() + 10);
    memset(decoded, 0, s.length() + 10);
    rd_decode_qp_utf8(s.c_str(), decoded);
    return decoded;
}

static char *rd_decode_8bit_utf8(const char *src)
{
    String s;

    // only non NULL and less than 998 octet length are allowed
    // rfc2045 section 2.8
    size_t len = src ? strlen(src) : 0;

    for (size_t i = 0; i < len; i++)
    {
        if (src[i] > 0 && i < 998)
            s += src[i];
    }

    char *decoded = (char *)malloc(s.length() + 1);
    memset(decoded, 0, s.length() + 1);
    strcpy(decoded, s.c_str());
    s.remove(0, s.length());
    return decoded;
}

static int rd_decode_latin1_utf8(unsigned char *out, int *outlen, const unsigned char *in, int *inlen)
{
    unsigned char *outstart = out;
    const unsigned char *base = in;
    const unsigned char *processed = in;
    unsigned char *outend = out + *outlen;
    const unsigned char *inend;
    unsigned int c;
    int bits;

    inend = in + (*inlen);
    while ((in < inend) && (out - outstart + 5 < *outlen))
    {
        c = *in++;

        /* assertion: c is a single UTF-4 value */
        if (out >= outend)
            break;
        if (c < 0x80)
        {
            *out++ = c;
            bits = -6;
        }
        else
        {
            *out++ = ((c >> 6) & 0x1F) | 0xC0;
            bits = 0;
        }

        for (; bits >= 0; bits -= 6)
        {
            if (out >= outend)
                break;
            *out++ = ((c >> bits) & 0x3F) | 0x80;
        }
        processed = (const unsigned char *)in;
    }
    *outlen = out - outstart;
    *inlen = processed - base;
    return (0);
}
#endif
#endif
#endif