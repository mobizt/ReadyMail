#ifndef SMTP_BASE_H
#define SMTP_BASE_H
#if defined(ENABLE_SMTP)
#include <Arduino.h>
#include "Common.h"

namespace ReadyMailSMTP
{
    class SMTPBase
    {
        friend class SMTPClient;

    protected:
        smtp_context *smtp_ctx = nullptr;

        void beginBase(smtp_context *smtp_ctx)
        {
            this->smtp_ctx = smtp_ctx;
            smtp_ctx->status->errorCode = 0;
            statusCode() = 0;
            smtp_ctx->status->text.remove(0, smtp_ctx->status->text.length());
        }
        int indexOf(const char *str, const char *find)
        {
            char *s = strstr(str, find);
            return (s) ? (int)(s - str) : -1;
        }
        int indexOf(const char *str, char find)
        {
            char *s = strchr(str, find);
            return (s) ? (int)(s - str) : -1;
        }
        bool isValidEmail(const char *email)
        {
            int apos = -1, dpos = -1;
            int len = strlen(email);

            for (int i = 0; i < len; i++)
            {
                if (email[i] == '@')
                {
                    if (apos != -1)
                        return false; // multiple @
                    apos = i;
                }
                else if (email[i] == '.')
                    dpos = i;
                else if (!isalnum(email[i]) && email[i] != '_' && email[i] != '-')
                    return false; // invalid character
            }

            // @ should appear before the last '.', and neither should be first or last
            if (apos < 1 || dpos < apos + 2 || dpos >= len - 1)
                return false;

            return true;
        }

        void clear(String &s) { s.remove(0, s.length()); }
        bool tcpSend(bool crlf, uint8_t argLen, ...)
        {
            String data;
            data.reserve(512);
            va_list args;
            va_start(args, argLen);
            for (int i = 0; i < argLen; ++i)
                data += va_arg(args, const char *);
            va_end(args);
#if defined(READYMAIL_CORE_DEBUG)
            if (!smtp_ctx->options.accumulate)
                setDebug(data, true);
#endif
            data += crlf ? "\r\n" : "";
            return tcpSend((uint8_t *)data.c_str(), data.length()) == data.length();
        }
        size_t tcpSend(uint8_t *data, size_t len)
        {
            if (smtp_ctx->options.accumulate)
            {
                smtp_ctx->options.data_len += len;
                return len;
            }
            else
                return smtp_ctx->client ? smtp_ctx->client->write(data, len) : 0;
        }

        void setDebugState(smtp_state state, const String &msg)
        {
            tState() = state;
#if defined(ENABLE_DEBUG) || defined(READYMAIL_CORE_DEBUG)
            setDebug(msg);
#endif
        }

        void setDebug(const String &info, bool core = false)
        {
#if defined(ENABLE_DEBUG) || defined(READYMAIL_CORE_DEBUG)
            if (smtp_ctx->status)
            {
                int i = 0, j = 0;
                while (j < (int)info.length())
                {
                    while (info[j] != '\r' && info[j] != '\n' && j < (int)info.length())
                        j++;

                    smtp_ctx->status->text = (core ? "[core] " : " ");
                    smtp_ctx->status->text += info.substring(i, j);
                    if (info[j] == '\n' && j == (int)info.length() - 1)
                        smtp_ctx->status->text += "\n";

                    print();
                    j += 2;
                    i = j;
                }
            }
#endif
        }

        bool setError(const char *func, int code, const String &msg = "", bool close = true)
        {
            if (smtp_ctx->status)
            {
                smtp_ctx->status->errorCode = code;
                statusCode() = 0;
                String buf;
                rd_print_to(buf, 100, "[%d] %s(): %s\n", code, func, (msg.length() ? msg.c_str() : errMsg(code).c_str()));
                smtp_ctx->status->text = buf;
                print();
            }
            smtp_ctx->options.processing = false;
            if (close)
                stopImpl();
            return false;
        }

        bool &serverStatus() { return smtp_ctx->server_status->connected; }

        smtp_state &cState() { return smtp_ctx->server_status->state_info.state; }

        // target state
        smtp_state &tState() { return smtp_ctx->server_status->state_info.target; }

        smtp_function_return_code &cCode() { return smtp_ctx->server_status->ret; }

        Attachment &cAttach(SMTPMessage &msg) { return msg.attachments[msg.attachments_idx]; }

        std::vector<smtp_mail_address_t> recpVec(const SMTPMessage &msg) { return msg.recipients; }

        std::vector<smtp_mail_address_t> ccVec(const SMTPMessage &msg) { return msg.ccs; }

        std::vector<smtp_mail_address_t> bccVec(const SMTPMessage &msg) { return msg.bccs; }

        int &statusCode() { return smtp_ctx->status->statusCode; }

        bool isIdleState(const char *func)
        {
            if (smtp_ctx->options.processing)
                return setError(func, SMTP_ERROR_PROCESSING);
            return true;
        }

        bool serverConnected() { return smtp_ctx->client && smtp_ctx->client->connected(); }

        void stopImpl(bool forceStop = false)
        {
            if (forceStop || serverConnected())
                smtp_ctx->client->stop();
            serverStatus() = false;
            smtp_ctx->server_status->secured = false;
            smtp_ctx->server_status->server_greeting_ack = false;
            smtp_ctx->server_status->authenticated = false;
            cState() = smtp_state_prompt;
        }

        void print()
        {
            smtp_ctx->status->state = (smtp_ctx->server_status->state_info.target > 0 ? smtp_ctx->server_status->state_info.target : smtp_ctx->server_status->state_info.state);
            if (smtp_ctx->resp_cb)
                smtp_ctx->resp_cb(*smtp_ctx->status);
            else
                serialPrint();
        }

        void serialPrint()
        {
            if (smtp_ctx->status->progressUpdated)
                printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", smtp_ctx->status->state, smtp_ctx->status->filename.c_str(), smtp_ctx->status->progress);
            else
                printf("ReadyMail[smtp][%d]%s\n", smtp_ctx->status->state, smtp_ctx->status->text.c_str());
        }

        void printf(const char *format, ...)
        {
#if defined(MAIL_CLIENT_PRINTF_BUFFER)
            const int size = MAIL_CLIENT_PRINTF_BUFFER;
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

        String getDateTimeString(time_t ts, const char *format)
        {
            char tbuf[100];
            strftime(tbuf, 100, format, localtime(&ts));
            return tbuf;
        }

        time_t getTimestamp(int year, int mon, int date, int hour, int mins, int sec)
        {
            struct tm timeinfo;
            timeinfo.tm_year = year - 1900;
            timeinfo.tm_mon = mon - 1;
            timeinfo.tm_mday = date;
            timeinfo.tm_hour = hour;
            timeinfo.tm_min = mins;
            timeinfo.tm_sec = sec;
            time_t ts = mktime(&timeinfo);
            return ts;
        }

        time_t getTimestamp(const String &timeString, bool gmt = false)
        {
#if defined(ESP8266) && defined(ESP32)
            struct tm timeinfo;
            if (strstr(timeString, ",") != NULL)
                strptime(timeString, "%a, %d %b %Y %H:%M:%S %z", &timeinfo);
            else
                strptime(timeString, "%d %b %Y %H:%M:%S %z", &timeinfo);

            time_t ts = mktime(&timeinfo);
            return ts;
#else
            time_t ts = 0;
            String out[7];
            int len = split(timeString, " ", out, 7);
            int day = 0, mon = 0, year = 0, hr = 0, mins = 0, sec = 0, tz_h = 0, tz_m = 0;

            int index = len == 5 ? -1 : 0; // No week days?

            // some response may include (UTC) and (ICT)
            if (len >= 5)
            {
                day = atoi(out[index + 1].c_str());
                for (size_t i = 0; i < 12; i++)
                {
                    if (strcmp(smtp_months[i], out[index + 2].c_str()) == 0)
                        mon = i;
                }

                // RFC 822 year to RFC 2822
                if (out[index + 3].length() == 2)
                    out[index + 3] = "20" + out[index + 3];

                year = atoi(out[index + 3].c_str());

                String out2[3];
                len = split(out[index + 4], ":", out2, 3);
                if (len == 3)
                {
                    hr = atoi(out2[0].c_str());
                    mins = atoi(out2[1].c_str());
                    sec = atoi(out2[2].c_str());
                }

                ts = getTimestamp(year, mon + 1, day, hr, mins, sec);

                if (out[index + 5].length() == 5 && gmt)
                {
                    char tmp[6];
                    memset(tmp, 0, 6);
                    strncpy(tmp, out[index + 5].c_str() + 1, 2);
                    tz_h = atoi(tmp);

                    memset(tmp, 0, 6);
                    strncpy(tmp, out[index + 5].c_str() + 3, 2);
                    tz_m = atoi(tmp);

                    time_t tz = tz_h * 60 * 60 + tz_m * 60;
                    if (out[index + 5][0] == '+')
                        ts -= tz; // remove time zone offset
                    else
                        ts += tz;
                }
            }
            return ts;
#endif
        }

#if defined(READYMAIL_USE_STRSEP_IMPL)
        char *strsepImpl(char **stringp, const char *delim)
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
#else
        char *strsepImpl(char **stringp, const char *delim) { return strsep(stringp, delim); }
#endif

        int split(const String &token, const char *sep, String *out, int len)
        {
            char *p = (char *)malloc(token.length() + 1);
            strcpy(p, token.c_str());
            char *pp = p;
            char *end = p;
            int i = 0;
            while (pp != NULL)
            {
                strsepImpl(&end, sep);
                if (strlen(pp) > 0)
                {
                    if (out && i < len)
                        out[i] = pp;
                    i++;
                }
                pp = end;
            }
            free(p);
            p = nullptr;
            return i;
        }

        String errMsg(int code)
        {
            String msg;
#if defined(ENABLE_DEBUG) || defined(READYMAIL_CORE_DEBUG)
            msg = rd_err(code);
            if (msg.length() == 0)
            {
                switch (code)
                {
#if defined(ENABLE_SMTP)
                case SMTP_ERROR_INVALID_SENDER_EMAIL:
                    msg = "Sender Email address is not valid";
                    break;
                case SMTP_ERROR_INVALID_RECIPIENT_EMAIL:
                    msg = "Recipient Email address is not valid";
                    break;
                case SMTP_ERROR_SEND_HEADER:
                    msg = "Send header failed";
                    break;
                case SMTP_ERROR_SEND_BODY:
                    msg = "Send body failed";
                    break;
                case SMTP_ERROR_SEND_DATA:
                    msg = "Send data failed";
                    break;
                case SMTP_ERROR_PROCESSING:
                    msg = "The last process does not yet finished";
                    break;
#endif
                default:
                    msg = "Unknown";
                    break;
                }
            }
#endif
            return msg;
        }

    public:
        SMTPBase() {}
    };
}
#endif
#endif