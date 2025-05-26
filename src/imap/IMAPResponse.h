/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef IMAP_RESPONSE_H
#define IMAP_RESPONSE_H
#if defined(ENABLE_IMAP)
#include <Arduino.h>
#include "Common.h"
#include "IMAPBase.h"
#include "IMAPConnection.h"
#include "IMAPSend.h"
#include "./core/ReadyTimer.h"
#include "./core/QBDecoder.h"
#include "Parser.h"

namespace ReadyMailIMAP
{
    class IMAPResponse : public IMAPBase
    {
        friend class IMAPConnection;
        friend class IMAPSend;
        friend class IMAPClient;

    public:
        void begin(imap_context *imap_ctx)
        {
            beginBase(imap_ctx);
            line.remove(0, line.length());
            complete = false;
            resp_timer.feed(imap_ctx->options.timeout.read / 1000);
            line.reserve(limit + 128);
        }

        imap_function_return_code handleResponse()
        {
            sys_yield();
            bool err = !serverConnected() || (!imap_ctx->options.idling && cState() != imap_state_idle && cState() != imap_state_done && readTimeout());
            if (err || (!imap_ctx->options.searching && (cState() == imap_state_fetch_envelope || cState() == imap_state_fetch_body_part) && cMsg().files.size() && !cMsg().files[cFileIndex()].fetch))
            {
                cCode() = err ? function_return_failure : function_return_success;
                return cCode();
            }

            cCode() = function_return_undefined;
            if (!imap_ctx->options.multiline)
                clear(line);

            int readLen = readLine(line);
            if (readLen > 0)
            {
#if defined(ENABLE_CORE_DEBUG)
                if (cState() != imap_state_search && cState() != imap_state_fetch_envelope && cState() != imap_state_fetch_body_part && !imap_ctx->options.multiline)
                    setDebug(imap_ctx, line, true);
#endif

                getResponseStatus(line, cState() == imap_state_initial_state ? "*" : imap_ctx->tag, *imap_ctx->status);

                if (cType() == imap_response_ok)
                    cCode() = function_return_success;

                if (cType() == imap_response_bad || cType() == imap_response_no)
                {
                    cCode() = function_return_failure;
                    setError(imap_ctx, __func__, IMAP_ERROR_RESPONSE, imap_ctx->status->text);
                }

                if ((cState() == imap_state_fetch_envelope || cState() == imap_state_fetch_body_part) && line.indexOf(imap_ctx->tag) == 0 && !cMsg().exists)
                {
                    cCode() = function_return_failure;
                    setError(imap_ctx, __func__, IMAP_ERROR_MESSAGE_NOT_EXISTS);
                }

                switch (cState())
                {
                case imap_state_greeting:
                case imap_state_auth_login:
                case imap_state_auth_plain:
                case imap_state_auth_xoauth2:

                    if (line[0] == '*')
                        parser.parseCaps(line, imap_ctx);

                    if (cState() == imap_state_auth_xoauth2 || cState() == imap_state_auth_plain)
                    {
                        if (cState() == imap_state_auth_xoauth2)
                        {
                            char *decoded = rd_b64_dec(rd_cast<const char *>(imap_ctx->status->text.c_str()));
                            if (decoded)
                            {
                                if (indexOf(decoded, "{\"status\":") > -1)
                                {
                                    cCode() = function_return_failure;
                                    setError(imap_ctx, __func__, AUTH_ERROR_AUTHENTICATION, decoded);
                                }
                                rd_free(&decoded);
                                decoded = nullptr;
                            }
                        }

                        // In case SASL-IR extension does not support, check for initial zero-length server challenge first "+ "
                        if (!imap_ctx->auth_caps[imap_auth_cap_sasl_ir] && line.indexOf("+ ") == 0)
                        {
                            cCode() = function_return_success;
                            cState() = cState() == imap_state_auth_xoauth2 ? imap_state_auth_xoauth2_next : imap_state_auth_plain_next;
                        }
                    }
                    break;

                case imap_state_list:
                    if (line[0] == '*')
                    {
                        std::array<String, 3> buf;
                        parser.parseMailbox(line, buf);
                        if (buf[2].length())
                            imap_ctx->mailboxes->push_back(buf);
                    }
                    break;

                case imap_state_examine:
                case imap_state_select:
                    if (line[0] == '*')
                        parser.parseExamine(line, mailbox_info, imap_ctx);
                    break;

                case imap_state_search:
                    parser.parseSearch(line, imap_ctx, msgNumVec());
                    break;

                case imap_state_fetch_envelope:
                case imap_state_fetch_body_part:
                    parser.parseFetch(line, imap_ctx, cMsg(), cState(), cMsg().files[cFileIndex()]);
                    break;

                case imap_state_append_init:
                case imap_state_idle:
                    if (line[0] == '+')
                    {
                        cCode() = function_return_success;
                        return cCode();
                    }
                    else if (line[0] == '*' && cState() == imap_state_idle)
                        parser.parseIdle(line, mailbox_info, imap_ctx);
                    break;

                case imap_state_send_command:
                    if (cCode() != function_return_failure && cCode() != function_return_success)
                    {
                        if (imap_ctx->cb.cmd)
                            imap_ctx->cb.command_response.text = line.substring(0, line.length() - 2);
                        else
                            imap_ctx->cb.command_response.text += line;

                        imap_ctx->cb.command_response.command = imap_ctx->cmd;
                        imap_ctx->cb.command_response.errorCode = 0;
                        imap_ctx->cb.command_response.isComplete = false;
                    }
                    else
                    {
                        if (imap_ctx->cb.cmd)
                            imap_ctx->cb.command_response.text.remove(0, imap_ctx->cb.command_response.text.length());
                        imap_ctx->cb.command_response.command = imap_ctx->cmd;
                        imap_ctx->cb.command_response.errorCode = cCode() == function_return_failure ? IMAP_ERROR_RESPONSE : 0;
                        imap_ctx->cb.command_response.isComplete = true;
                    }

                    if (imap_ctx->cb.cmd)
                        imap_ctx->cb.cmd(imap_ctx->cb.command_response);

                    break;

                default:
                    break;
                }
            }

            if (cType() == imap_response_undefined && cState() == imap_state_initial_state)
                cCode() = function_return_continue;

            return cCode();
        }

        void getResponseStatus(String &line, const String &tag, imap_response_status_t &status)
        {
            cType() = imap_response_undefined;

            if (line.indexOf(tag) == 0 && line.length() >= 2 && line[line.length() - 2] != '\r' && line[line.length() - 1] != '\n')
            {
                imap_ctx->options.multiline = true;
                return;
            }

            if (line.indexOf(tag) == 0)
            {
                if (line.indexOf("OK") > 0)
                    cType() = imap_response_ok;
                else if (line.indexOf("NO") > 0)
                    cType() = imap_response_no;
                else if (line.indexOf("BAD") > 0)
                    cType() = imap_response_bad;

                if (cType() != imap_response_undefined)
                {
                    imap_ctx->options.multiline = false;
                    status.text = line.substring(tag.length() + (cType() == imap_response_bad ? 5 : 4), line.indexOf("\r\n"));
                }
            }
        }

        bool readTimeout()
        {
            if (!resp_timer.isRunning())
                resp_timer.feed(imap_ctx->options.timeout.read / 1000);

            if (resp_timer.remaining() == 0)
            {
                resp_timer.feed(imap_ctx->options.timeout.read / 1000);
                setError(imap_ctx, __func__, TCP_CLIENT_ERROR_READ_DATA);
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
                    if (res == '\n' || (p >= (int)limit && res == ' '))
                        return p;
                }
            }
            return p;
        }

        int tcpAvailable() { return imap_ctx->client ? imap_ctx->client->available() : 0; }
        int tcpRead() { return imap_ctx->client ? imap_ctx->client->read() : -1; }
        void stop(bool forceStop = false)
        {
            stopImpl(forceStop);
            clear(line);
        }

    private:
        bool complete = false;
        String line;
        ReadyTimer resp_timer, idle_timer;
        MailboxInfo mailbox_info;
        size_t limit = 2048;
        IMAPParser parser;
    };
}
#endif
#endif