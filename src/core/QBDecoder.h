/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef QB_DECODER_H
#define QB_DECODER_H

#include <Arduino.h>

#if defined(ENABLE_IMAP) || defined(ENABLE_SMTP)

// Renesas devices
#if defined(ARDUINO_UNOWIFIR4) || defined(ARDUINO_MINIMA) || defined(ARDUINO_PORTENTA_C33)
#define XMAILER_STRSEP strsepImpl
#define XMAILER_USE_STRSEP_IMPL
#else
#define XMAILER_STRSEP strsep
#endif

// Re-interpret cast
template <typename To, typename From>
static To rd_cast(From frm)
{
  return reinterpret_cast<To>(frm);
}

static void rd_set(void *m, int size) { memset(m, 0, size); }

template <typename T = void *>
static T rd_mem(int len, bool set = false)
{
  T buf = rd_cast<T>(malloc(len));
  if (set)
    rd_set(buf, len);
  return buf;
}

// we have to set null, pass the pointer instead
static void rd_free(void *ptr)
{
    void **p = rd_cast<void **>(ptr);
    if (*p)
    {
        free(*p);
        *p = 0;
    }
}

#define strfcpy(A, B, C) strncpy(A, B, C), *(A + (C) - 1) = 0

__attribute__((used)) static int Index_base64[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1};

__attribute__((used)) static int Index_hex[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

#if defined(XMAILER_USE_STRSEP_IMPL)
// This is strsep implementation because strdup may not available in some platform.
static char *__attribute__((used)) strsepImpl(char **stringp, const char *delim)
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

#define IsPrint(c) (isprint((unsigned char)(c)) || \
                    ((unsigned char)(c) >= 0xa0))

#define hexval(c) Index_hex[(unsigned int)(c)]
#define base64val(c) Index_base64[(unsigned int)(c)]

// Quoted-printable and base64 decoder
class QBDecoder
{

public:
  QBDecoder() {};
  ~QBDecoder() {};
  void decode(char *d, const char *s, size_t dlen)
  {
    const char *q;
    size_t n;
    int found_encoded = 0;

    dlen--; /* save room for the terminal nul */

    while (*s && dlen > 0)
    {
      const char *p;
      if ((p = strstr(s, "=?")) == NULL ||
          (q = strchr(p + 2, '?')) == NULL ||
          (q = strchr(q + 1, '?')) == NULL ||
          (q = strstr(q + 1, "?=")) == NULL)
      {
        /* no encoded words */
        if (d != s)
          strfcpy(d, s, dlen + 1);
        return;
      }

      if (p != s)
      {
        n = (size_t)(p - s);
        /* ignore spaces between encoded words */
        if (!found_encoded || strspn(s, " \t\r\n") != n)
        {
          if (n > dlen)
            n = dlen;
          if (d != s)
            memcpy(d, s, n);
          d += n;
          dlen -= n;
        }
      }

      decodeWord(d, p, dlen);
      found_encoded = 1;
      s = q + 2;
      n = strlen(d);
      dlen -= n;
      d += n;
    }
    *d = 0;
  }

private:
  enum
  {
    ENCOTHER,
    ENC7BIT,
    ENC8BIT,
    ENCQUOTEDPRINTABLE,
    ENCBASE64,
    ENCBINARY
  };

  void decodeWord(char *d, const char *s, size_t dlen)
  {
    char *p = safe_strdup(s);
    char *pp = p;
    char *end = p;
    char *pd = d;
    size_t len = dlen;
    int enc = 0, filter = 0, count = 0, c1, c2, c3, c4;

    while (pp != NULL)
    {
      // See RFC2047.h
      XMAILER_STRSEP(&end, "?");
      count++;
      switch (count)
      {
      case 2:
        if (strcasecmp(pp, "utf-8") != 0)
        {
          filter = 1;
        }
        break;
      case 3:
        if (toupper(*pp) == 'Q')
          enc = ENCQUOTEDPRINTABLE;
        else if (toupper(*pp) == 'B')
          enc = ENCBASE64;
        else
          return;
        break;
      case 4:
        if (enc == ENCQUOTEDPRINTABLE)
        {
          while (*pp && len > 0)
          {
            if (*pp == '_')
            {
              *pd++ = ' ';
              len--;
            }
            else if (*pp == '=')
            {
              *pd++ = (hexval(pp[1]) << 4) | hexval(pp[2]);
              len--;
              pp += 2;
            }
            else
            {
              *pd++ = *pp;
              len--;
            }
            pp++;
          }
          *pd = 0;
        }
        else if (enc == ENCBASE64)
        {
          while (*pp && len > 0)
          {
            c1 = base64val(pp[0]);
            c2 = base64val(pp[1]);
            *pd++ = (c1 << 2) | ((c2 >> 4) & 0x3);
            if (--len == 0)
              break;

            if (pp[2] == '=')
              break;

            c3 = base64val(pp[2]);
            *pd++ = ((c2 & 0xf) << 4) | ((c3 >> 2) & 0xf);
            if (--len == 0)
              break;

            if (pp[3] == '=')
              break;

            c4 = base64val(pp[3]);
            *pd++ = ((c3 & 0x3) << 6) | c4;
            if (--len == 0)
              break;

            pp += 4;
          }
          *pd = 0;
        }
        break;
      }

      pp = end;
    }

    rd_free(&p);

    if (filter)
    {
      pd = d;
      while (*pd)
      {
        if (!IsPrint(*pd))
          *pd = '?';
        pd++;
      }
    }
    return;
  }
  char *safe_strdup(const char *s)
  {
    char *p;
    size_t l;

    if (!s || !*s)
      return 0;
    l = strlen(s) + 1;
    p = rd_mem<char *>(l);
    memcpy(p, s, l);
    return (p);
  }
};
#endif
#endif // QB_DECODER_H