/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SMTP_RESPONSE_H
#define SMTP_RESPONSE_H
#if defined(ENABLE_SMTP)
#include <Arduino.h>
#include "Common.h"
#include "SMTPBase.h"
#include "SMTPConnection.h"

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
            resp_timer.feed(smtp_ctx->options.timeout.read / 1000);
        }

        smtp_function_return_code handleResponse()
        {
            sys_yield();
            if (smtp_ctx->options.accumulate || smtp_ctx->options.imap_mode || readTimeout() || !serverConnected())
            {
                cCode() = smtp_ctx->options.accumulate || smtp_ctx->options.imap_mode ? function_return_success : function_return_failure;
                return cCode();
            }

            if (cState() == smtp_state_send_data)
            {
                cCode() = function_return_success;
                return cCode();
            }

            cCode() = function_return_undefined;
            bool ret = false;
            String line;
            line.reserve(1024);
            int readLen = readLine(line);
            if (readLen > 0)
            {
                response += line;
                getResponseStatus(line, smtp_ctx->server_status->state_info.status_code, *smtp_ctx->status);

                if (cState() == smtp_state_connect_command || cState() == smtp_state_send_command)
                {
                    if (smtp_ctx->cmd_ctx.cb)
                        smtp_ctx->cmd_ctx.resp.text = line.substring(0, line.length() - 2);
                    else
                        smtp_ctx->cmd_ctx.resp.text += line;

                    if (smtp_ctx->cmd_ctx.cb)
                    {
                        if ((smtp_ctx->cmd_ctx.cmd.startsWith("EHLO") || smtp_ctx->cmd_ctx.cmd.startsWith("HELO")) && statusCode() == smtp_server_status_code_250)
                        {
                            smtp_ctx->server_status->server_greeting_ack = true;
                            parseCaps(line);
                        }
                        else if (statusCode() == smtp_server_status_code_235)
                            smtp_ctx->server_status->authenticated = true;
                        else if (statusCode() == smtp_server_status_code_221)
                        {
                            smtp_ctx->server_status->authenticated = false;
                            smtp_ctx->server_status->server_greeting_ack = false;
                        }

                        smtp_ctx->cmd_ctx.resp.command = cState() == smtp_state_connect_command ? "connect" : smtp_ctx->cmd_ctx.cmd;
                        smtp_ctx->cmd_ctx.resp.statusCode = statusCode();
                        smtp_ctx->cmd_ctx.resp.errorCode = statusCode() >= 400 ? SMTP_ERROR_RESPONSE : 0;
                        smtp_ctx->cmd_ctx.cb(smtp_ctx->cmd_ctx.resp);
                    }

                    if (statusCode() > 0)
                        cCode() = statusCode() >= 400 ? function_return_failure : function_return_success;

                    return cCode();
                }

                if (cState() == smtp_state_greeting)
                    parseCaps(line);

                if (statusCode() == 0 && cState() == smtp_state_initial_state)
                {
                    cCode() = function_return_continue;
                    return cCode();
                }

#if defined(ENABLE_CORE_DEBUG)
                if (statusCode() < 500 && cState() != smtp_state_send_command)
                    setDebug(line, true, "[receive]");
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
                        char *decoded = rd_b64_dec(rd_cast<const char *>(smtp_ctx->status->text.c_str()));
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
                            rd_free(&decoded);
                            decoded = nullptr;
                        }
                    }
                    else if (statusCode() == smtp_server_status_code_354)
                    {
                        setReturn(true, complete, ret);
                    }
                }
                else if (statusCode() >= 400)
                {
                    setReturn(false, complete, ret);
                    statusCode() = 0;
                    setError(__func__, SMTP_ERROR_RESPONSE, smtp_ctx->status->text);
                }
                resp_timer.feed(smtp_ctx->options.timeout.read / 1000);
            }

            if (readLen == 0 && cState() == smtp_state_send_body && statusCode() == smtp_server_status_code_0)
                setReturn(true, complete, ret);

            cCode() = !complete ? function_return_continue : (ret ? function_return_success : function_return_failure);

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
                resp_timer.feed(smtp_ctx->options.timeout.read / 1000);

            if (resp_timer.remaining() == 0)
            {
                resp_timer.feed(smtp_ctx->options.timeout.read / 1000);
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
        void getResponseStatus(const String &resp, smtp_server_status_code statusCode, smtp_response_status_t &status)
        {
            if (statusCode > smtp_server_status_code_0)
            {
                int codeLength = 3;
                int textLength = resp.length() - codeLength - 1;

                if ((int)resp.length() > codeLength && (resp[codeLength] == ' ' || resp[codeLength] == '-') && textLength > 0)
                {
                    int code = resp.substring(0, codeLength).toInt();
                    if (resp[codeLength] == ' ')
                        status.statusCode = code;

                    int i = codeLength + 1;

                    // We will collect the status text starting from status code 334 and 4xx
                    // The status text of 334 will be used for debugging display of the base64 server challenge
                    if (code < smtp_server_status_code_500 && (code == smtp_server_status_code_334 || code >= smtp_server_status_code_421))
                    {
                        // find the next sp
                        while (i < (int)resp.length() && resp[i] != ' ')
                            i++;

                        // if sp found, set index to the next pos, otherwise set index to num length + 1
                        i = (i < (int)resp.length() - 1) ? i + 1 : codeLength + 1;
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

        void stop(bool forceStop = false)
        {
            stopImpl(forceStop);
            clear(response);
        }

    public:
        SMTPResponse()
        {
            for (uint8_t i = 0; i < smtp_auth_cap_max_type; i++)
                auth_caps[i] = 0;
            for (uint8_t i = 0; i < smtp_send_cap_max_type; i++)
                feature_caps[i] = 0;
        }
    };
}
#endif
#endif