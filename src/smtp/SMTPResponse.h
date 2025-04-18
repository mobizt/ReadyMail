#ifndef SMTP_RESPONSE_H
#define SMTP_RESPONSE_H
#if defined(ENABLE_SMTP)
#include <Arduino.h>
#include "Common.h"
#include "SMTPBase.h"

namespace ReadyMailSMTP
{
    class SMTPResponse : public SMTPBase
    {
        friend class SMTPConnection;
        friend class SMTPSend;

    private:
        bool auth_caps[smtp_auth_cap_max_type];
        bool feature_caps[smtp_send_cap_max_type];
        String response;
        ReadyTimer resp_timer;
        bool complete = false;

        void begin(smtp_context *smtp_ctx)
        {
            beginBase(smtp_ctx);
            response.remove(0, response.length());
            complete = false;
            resp_timer.feed(smtp_ctx->read_timeout_ms / 1000);
        }

        smtp_function_return_code handleResponse()
        {
            cCode() = function_return_undefined;
            bool ret = false;
            String line;

            if (smtp_ctx->accumulate || smtp_ctx->imap_mode)
            {
                cCode() = function_return_success;
                return cCode();
            }

            bool canForward = smtp_ctx->canForward;
            smtp_ctx->canForward = false;

            sys_yield();
            int readLen = readLine(line);
            if (readTimeout())
            {
                cCode() = function_return_failure;
                goto exit;
            }

            if (readLen > 0)
            {
                response += line;
                getResponseStatus(line, smtp_ctx->server_status->state_info.status_code, 0, *smtp_ctx->status);

                if (cState() == smtp_state_greeting)
                    parseCaps(line);

                if (statusCode() == 0 && cState() == smtp_state_initial_state)
                {
                    cCode() = function_return_continue;
                    goto exit;
                }

                // get the status code again for unexpected return code
                if (cState() == smtp_state_start_tls || statusCode() == 0)
                    getResponseStatus(response, smtp_server_status_code_0, 0, *smtp_ctx->status);
#if defined(READYMAIL_CORE_DEBUG)
                if (statusCode() > 0 && statusCode() < 500)
                    setDebug(line, true);
#endif
                // positive completion
                if (statusCode() >= 200 && statusCode() < 300)
                    setReturn(true, complete, ret);
                // positive intermediate
                else if (statusCode() >= 300 && statusCode() < 400)
                {
                    if (statusCode() == smtp_server_status_code_334)
                    {
                        // base64 encoded server challenge message
                        char *decoded = rd_base64_decode((const char *)smtp_ctx->status->text.c_str());
                        if (decoded)
                        {
                            if (cState() == smtp_state_auth_xoauth2 && indexOf(decoded, "{\"status\":") > -1)
                            {
                                setReturn(false, complete, ret);
                                setError(__func__, AUTH_ERROR_AUTHENTICATION, decoded);
                            }
                            else if ((cState() == smtp_state_auth_login && indexOf(decoded, "Username:") > -1) ||
                                     (cState() == smtp_state_login_user && indexOf(decoded, "Password:") > -1))
                                setReturn(true, complete, ret);
                            rd_release(decoded);
                            decoded = nullptr;
                        }
                    }
                    else if (statusCode() == smtp_server_status_code_354)
                    {
                        setReturn(true, complete, ret);
                    }
                }
                else if (statusCode() >= smtp_server_status_code_500)
                {
                    setReturn(false, complete, ret);
                    statusCode() = 0;
                    setError(__func__, SMTP_ERROR_RESPONSE, smtp_ctx->status->text);
                }
                resp_timer.feed(smtp_ctx->read_timeout_ms / 1000);
            }

            if (readLen == 0 && cState() == smtp_state_send_body && statusCode() == smtp_server_status_code_0)
                setReturn(true, complete, ret);

            cCode() = !complete ? function_return_continue : (ret ? function_return_success : function_return_failure);

        exit:
            return cCode();
        }

        void setReturn(bool positive, bool &complete, bool &ret)
        {
            complete = true;
            ret = positive ? true : false;
        }

        bool readTimeout()
        {
            if (!resp_timer.isRunning())
                resp_timer.feed(smtp_ctx->read_timeout_ms / 1000);

            if (resp_timer.remaining() == 0)
            {
                resp_timer.feed(smtp_ctx->read_timeout_ms / 1000);
                setError(__func__, TCP_CLIENT_ERROR_READ_DATA);
                return true;
            }
            return false;
        }

        int readLine(String &buf)
        {
            int p = 0;
            while (tcpAvailable())
            {
                int res = tcpRead();
                if (res > -1)
                {
                    buf += (char)res;
                    p++;
                    if (res == '\n')
                        return p;
                }
            }
            return p;
        }

        int tcpAvailable() { return smtp_ctx->client ? smtp_ctx->client->available() : 0; }
        int tcpRead() { return smtp_ctx->client ? smtp_ctx->client->read() : -1; }
        void getResponseStatus(const String &resp, smtp_server_status_code statusCode, int beginPos, smtp_response_status_t &status)
        {
            if (statusCode > smtp_server_status_code_0)
            {
                int codeLength = 3;
                int textLength = resp.length() - codeLength - 1;

                if (resp.length() > codeLength && (resp[codeLength] == ' ' || resp[codeLength] == '-') && textLength > 0)
                {
                    int code = resp.substring(0, codeLength).toInt();
                    if (resp[codeLength] == ' ')
                        status.statusCode = code;

                    int i = codeLength + 1;

                    // We will collect the status text starting from status code 334 and 4xx
                    // The status text of 334 will be used for debugging display of the base64 server challenge
                    if (code == smtp_server_status_code_334 || code >= smtp_server_status_code_421)
                    {
                        // find the next sp
                        while (i < resp.length() && resp[i] != ' ')
                            i++;

                        // if sp found, set index to the next pos, otherwise set index to num length + 1
                        i = (i < resp.length() - 1) ? i + 1 : codeLength + 1;
                        status.text += resp.substring(i);
                    }
                    else
                        status.text += resp.substring(i);
                }
            }
        }

        void parseCaps(const String &line)
        {
            if (line.indexOf("AUTH ", 0) > -1)
            {
                for (int i = smtp_auth_cap_plain; i < smtp_auth_cap_max_type; i++)
                {
                    if (line.indexOf(smtp_auth_cap_token[i].text, 0) > -1)
                        auth_caps[i] = true;
                }
            }
            else if (line.indexOf(smtp_auth_cap_token[smtp_auth_cap_starttls].text, 0) > -1)
            {
                auth_caps[smtp_auth_cap_starttls] = true;
                return;
            }
            else
            {
                for (int i = smtp_send_cap_binary_mime; i < smtp_send_cap_max_type; i++)
                {
                    if (strlen(smtp_send_cap_token[i].text) > 0 && line.indexOf(smtp_send_cap_token[i].text, 0) > -1)
                    {
                        feature_caps[i] = true;
                        return;
                    }
                }
            }
        }

        void stop()
        {
            stopImpl();
            clear(response);
        }

    public:
        SMTPResponse() {}
    };
}
#endif
#endif