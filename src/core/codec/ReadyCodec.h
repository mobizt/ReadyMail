/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef READY_CODEC_H
#define READY_CODEC_H
#include <Arduino.h>
#include <stdarg.h>
#include "QBDecoder.h"

#if defined(ENABLE_IMAP) || defined(ENABLE_SMTP)

#ifdef USE_STATIC_VECTOR
#define vec Vector<int, MAX_SMTP_TEXT_SOFTBREAK>
#else
#define vec Vector<int>
#endif


static inline void __attribute__((used)) sys_yield()
{
#if defined(ARDUINO_ESP8266_MAJOR) && defined(ARDUINO_ESP8266_MINOR) && defined(ARDUINO_ESP8266_REVISION) && ((ARDUINO_ESP8266_MAJOR == 3 && ARDUINO_ESP8266_MINOR >= 1) || ARDUINO_ESP8266_MAJOR > 3)
    esp_yield();
#else
    delay(0);
#endif
}

static inline void rd_a4_to_a3(unsigned char *a3, unsigned char *a4)
{
    a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4);
    a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2);
    a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
}

// base64 decoder
static uint8_t *rd_b64_dec_impl(const char *encoded, int &size)
{
    int encoded_len = strlen(encoded);
    int decoded_len = (encoded_len * 3) / 4;
    uint8_t *raw = rd_mem<uint8_t *>(decoded_len + 1);
    int i = 0;
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
                a4[i] = rd_b64_lookup(a4[i]);

            rd_a4_to_a3(a3, a4);
            for (i = 0; i < 3; i++)
                raw[raw_len++] = a3[i];
            i = 0;
        }
    }

    if (i)
    {
        for (int j = i; j < 4; j++)
            a4[j] = '\0';

        for (int j = 0; j < 4; j++)
            a4[j] = rd_b64_lookup(a4[j]);

        rd_a4_to_a3(a3, a4);

        for (int j = 0; j < i - 1; j++)
            raw[raw_len++] = a3[j];
    }
    size = raw_len;
    return raw;
}

// base64 decoder
static char *rd_b64_dec(const char *encoded)
{
    int len = 0;
    char *raw = rd_cast<char *>(rd_b64_dec_impl(encoded, len));
    raw[len] = '\0';
    return raw;
}


#if defined(ENABLE_SMTP)

// soft break modifier
static bool rd_add_sb(String &buf, int index, vec &softbreak_index, String &softbreak_buf, int line_max_len)
{
    for (size_t i = 0; i < softbreak_index.size(); i++)
    {
        if ((index == softbreak_index[i]))
        {
            softbreak_index.erase(i);
            if ((int)buf.length() + 2 <= line_max_len)
                buf += "\r\n"; // to complete soft break
            else if ((int)buf.length() + 1 <= line_max_len)
            {
                buf += "\r";
                softbreak_buf = "\n";
            }
            else
                softbreak_buf = "\r\n";
            return true;
        }
        else if (index == -1 * softbreak_index[i])
        {
            softbreak_index.erase(i);
            return true;
        }
    }
    return false;
}

// soft break marking
static void rd_get_sb(src_data_ctx &src, int index, int max_len, vec &softbreak_index)
{
    int last_index = softbreak_index.size() ? softbreak_index[softbreak_index.size() - 1] + 1 : index;

    // conver index to positive
    if (last_index < 0)
        last_index = -1 * last_index;

    int i = last_index + max_len;
    if (i < (int)src.size())
    {
        src.seek(index);
        bool softbreak = false;
        bool hardbreak = false;
        int sbPos = 0;
        char c1 = 0;
        while (src.available() && i >= last_index)
        {
            src.seek(i);
            char c = src.read();
            if (c == ' ' && i + 2 <= last_index + max_len)
            {
                if (!softbreak)
                {
                    softbreak = true;
                    sbPos = i;
                }
            }

            // hard break? store keep index as the negative index
            if (!hardbreak && c1 == '\n')
            {
                sbPos = -1 * (i + 2);
                hardbreak = true;
                break;
            }

            c1 = c;
            i--;
        }

        if (softbreak || hardbreak)
            softbreak_index.push_back(sbPos);
    }
}

// quoted-printable encoder
static String rd_qp_encode_chunk(src_data_ctx &src, int &index, bool flowed, int max_len, String &softbreak_buf, vec &softbreak_index)
{
    String sbuf;
    int sindex = 0;
    char *out = rd_mem<char *>(10);

    // Breaks the string at sp with soft linebreak for flowed text
    if (flowed)
    {
        rd_get_sb(src, index, max_len, softbreak_index);
        if (softbreak_buf.length())
            sbuf += softbreak_buf;
        softbreak_buf.remove(0, softbreak_buf.length());
    }

    src.seek(index);
    int c1 = 0;
    while (src.available() || c1 > 0)
    {
        int c = c1 == 0 ? src.read() : c1;
        c1 = src.available() ? src.read() : 0;
        if ((int)sbuf.length() >= max_len - 3 && c != 10 && c != 13)
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

        if (flowed && rd_add_sb(sbuf, index + sindex, softbreak_index, softbreak_buf, max_len - 3))
        {
            sindex++; // skip space after soft break
            break;
        }
        sindex++;
    }
out:
    if (out)
        rd_free(&out);
    index += sindex;
    return sbuf;
}

// quoted-printable and base64 encoder
static String rd_qb_encode_chunk(src_data_ctx &src, int &index, int mode, bool flowed, int max_len, String &softbreak_buf, vec &softbreak_index)
{
    String line, buf;
    buf.reserve(100);
    line.reserve(100);
    int len = src.size();
    if (mode == 2 /* xenc_qp */)
        line = rd_qp_encode_chunk(src, index, flowed, max_len, softbreak_buf, softbreak_index);
    else
    {
        // Breaks the string at sp with soft linebreak for flowed text
        if (flowed)
        {
            rd_get_sb(src, index, max_len, softbreak_index);
            if (softbreak_buf.length())
                buf += softbreak_buf;
            softbreak_buf.remove(0, softbreak_buf.length());
        }

        src.seek(index);
        int sindex = 0;
        while (src.available())
        {
            if ((int)buf.length() < (mode == 3 /* xenc_base64 */ ? 57 : max_len))
            {
                buf += src.read();
                if (flowed && rd_add_sb(buf, index + sindex, softbreak_index, softbreak_buf, (mode == 3 /* xenc_base64 */ ? 57 : max_len)))
                {
                    sindex++; // skip space after soft break
                    if (mode != 3 /* xenc_base64 */)
                        break;
                }
                else
                    sindex++;
                continue;
            }
            break;
        }

        if (buf.length())
        {
            if (mode == 3 /* xenc_base64 */)
            {
                char *enc = rd_b64_enc(rd_cast<const unsigned char *>(buf.c_str()), buf.length());
                line = enc;
                rd_free(&enc);
            }
            else
                line = buf;

            if (mode != 1 /* xenc_7bit */)
                line += "\r\n";

            index = sindex > 0 ? index + sindex : len;
        }
    }
    return line;
}

#endif

static inline String rd_enc_oauth(const String &email, const String &accessToken)
{
    String out;
    String raw;
    rd_print_to(raw, email.length() + accessToken.length() + 30, "user=%s\1auth=Bearer %s\1\1", email.c_str(), accessToken.c_str());
    char *enc = rd_b64_enc(rd_cast<const unsigned char *>(raw.c_str()), raw.length());
    if (enc)
    {
        out = enc;
        rd_free(&enc);
    }
    return out;
}

static inline String rd_enc_plain(const String &email, const String &password)
{
    // rfc4616
    String out;
    int len = email.length() + password.length() + 2;
    uint8_t *buf = rd_mem<uint8_t *>(len, true);
    memcpy(buf + 1, email.c_str(), email.length());
    memcpy(buf + email.length() + 2, password.c_str(), password.length());
    char *enc = rd_b64_enc(buf, len);
    if (enc)
    {
        out = enc;
        rd_free(&enc);
    }
    rd_free(&buf);
    return out;
}

#if defined(READYMAIL_USE_STRSEP_IMPL)
#ifdef __AVR__
static inline char *rd_strsep(char **sp, const char *delim)
{
    if (!sp || !*sp)
        return NULL;
    char *start = *sp;
    char *p = start;
    while (*p)
    {
        const char *d = delim;
        while (*d)
        {
            if (*p == *d)
            {
                *p = '\0';
                *sp = p + 1;
                return start;
            }
            ++d;
        }
        ++p;
    }
    *sp = NULL;
    return start;
}
#else
static inline char *rd_strsep(char **stringp, const char *delim)
{
    char *rv = *stringp;
    if (rv)
    {
        *stringp += strcspn(*stringp, delim);
        if (**stringp)
            *(*stringp)++ = '\0';
        else
            *stringp = 0;
    }
    return rv;
}
#endif

#else
static inline char *rd_strsep(char **stringp, const char *delim) { return strsep(stringp, delim); }
#endif

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

static void rd_dec_tis620_utf8(char *out, const char *in, size_t len)
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

static inline int rd_dec_char(const char *s) { return 16 * hexval(*(s + 1)) + hexval(*(s + 2)); }

static void rd_dec_qp_utf8(const char *src, char *out)
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
            out[idx++] = rd_dec_char(src);
            src += 3;
        }
    }
}

// one line 7-bit decoder
static char *rd_dec_7bit_utf8(const char *src)
{
    String buf;

    // only non NULL and 7-bit ASCII are allowed
    // rfc2045 section 2.7
    size_t len = src ? strlen(rd_cast<const char *>(src)) : 0;
    if (len > 998) // max 998 octets per line
        len = 998;

    for (size_t i = 0; i < len; i++)
    {
        if (src[i] > 0 && src[i] < 128)
            buf += src[i];
    }

    // some special chars can't send in 7bit unless encoded as queoted printable string
    char *decoded = rd_mem<char *>(buf.length() + 10, true);
    rd_dec_qp_utf8(buf.c_str(), decoded);
    return decoded;
}

// one line 8-bit decoder
static char *rd_dec_8bit_utf8(const char *src)
{
    String s;

    // only non NULL and less than 998 octet length are allowed
    // rfc2045 section 2.8
    size_t len = src ? strlen(src) : 0;
    if (len > 998) // max 998 octets per line
        len = 998;

    for (size_t i = 0; i < len; i++)
    {
        if (src[i] > 0)
            s += src[i];
    }

    char *decoded = rd_mem<char *>(s.length() + 1, true);
    strcpy(decoded, s.c_str());
    s.remove(0, s.length());
    return decoded;
}

static int rd_dec_latin1_utf8(unsigned char *out, int *outlen, const unsigned char *in, int *inlen)
{
    unsigned char *outstart = out;
    const unsigned char *base = in;
    const unsigned char *processed = in;
    unsigned char *outend = out + *outlen;
    const unsigned char *inend;
    int bits;

    inend = in + (*inlen);
    while ((in < inend) && (out - outstart + 5 < *outlen))
    {
        unsigned int c = *in++;

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
        processed = rd_cast<const unsigned char *>(in);
    }
    *outlen = out - outstart;
    *inlen = processed - base;
    return (0);
}

#endif
#endif
#endif