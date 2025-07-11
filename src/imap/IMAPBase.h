/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef IMAP_BASE_H
#define IMAP_BASE_H
#if defined(ENABLE_IMAP)
#include <Arduino.h>
#include "Common.h"

namespace ReadyMailIMAP
{
    class IMAPBase
    {
        friend class IMAPClient;

    public:
#if defined(ENABLE_DEBUG)
        static void setDebug(imap_context *imap_ctx, const String &info, bool core = false)
        {
            if (imap_ctx->status)
            {
                char *p = rd_mem<char *>(info.length() + 1);
                strcpy(p, info.c_str());
                char *pp = p;
                char *end = p;
                while (pp != NULL)
                {
                    rd_strsep(&end, "\r\n");
                    if (strlen(pp) > 0)
                    {
                        if (strlen(pp) == 1 && pp[0] == 32)
                        {
                            pp = end;
                            continue;
                        }
                        else
                        {
                            imap_ctx->status->text = (core ? "[core] " : " ");
                            imap_ctx->status->text += pp;
                            print(imap_ctx);
                        }
                    }
                    pp = end;
                }
                rd_free(&p);
            }
        }
#endif
    protected:
        imap_context *imap_ctx = nullptr;
        ReadyTimer err_timer;

        void beginBase(imap_context *imap_ctx)
        {
            this->imap_ctx = imap_ctx;
            imap_ctx->status->errorCode = 0;
            imap_ctx->status->text.remove(0, imap_ctx->status->text.length());
        }

        int indexOf(const char *str, const char *find)
        {
            const char *s = strstr(str, find);
            return (s) ? (int)(s - str) : -1;
        }

        int indexOf(const char *str, char find)
        {
            const char *s = strchr(str, find);
            return (s) ? (int)(s - str) : -1;
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
#if defined(ENABLE_CORE_DEBUG)
            setDebug(imap_ctx, data, true);
#endif
            data += crlf ? "\r\n" : "";
            return tcpSend(rd_cast<const uint8_t *>(data.c_str()), data.length()) == data.length();
        }

        size_t tcpSend(const uint8_t *data, size_t len) { return imap_ctx->client ? imap_ctx->client->write(data, len) : 0; }

        bool setError(imap_context *imap_ctx, const char *func, int code, const String &msg = "")
        {
            if (imap_ctx->status)
            {
                imap_ctx->status->errorCode = code;
                String buf;
                rd_print_to(buf, 100, "[%d] %s(): %s\n", code, func, (msg.length() ? msg.c_str() : errMsg(code).c_str()));
                imap_ctx->status->text = buf;
                if (!err_timer.isRunning() || err_timer.remaining() == 0)
                {
                    err_timer.feed(2);
                    print(imap_ctx);
                }
            }
            resetProcessFlag();
            releaseSMTP();
            imap_ctx->server_status->ret = function_return_failure;
            return false;
        }

        void setProcessFlag(bool &flag) { flag = true; }

        void clearAllProcessFlags()
        {
            imap_ctx->options.searching = false;
            imap_ctx->options.idling = false;
            imap_ctx->options.processing = false;
        }

        void resetProcessFlag()
        {
            if ((cState() == imap_state_fetch_envelope && imap_ctx->options.searching) || (cState() != imap_state_search && cState() != imap_state_done && cState() != imap_state_idle))
                imap_ctx->options.processing = false;

            if (cState() == imap_state_search)
                imap_ctx->options.searching = false;

            if (cState() == imap_state_done || cState() == imap_state_idle)
                imap_ctx->options.idling = false;
        }

        void releaseSMTP()
        {
#if defined(ENABLE_IMAP_APPEND)
            if (imap_ctx->smtp)
                delete imap_ctx->smtp;
            imap_ctx->smtp = nullptr;
            imap_ctx->msg.clear();
#endif
        }

        bool isIdleState(const char *func)
        {
            if (imap_ctx->options.processing || imap_ctx->options.searching)
                return setError(imap_ctx, func, IMAP_ERROR_PROCESSING);
            return true;
        }

        void deAuthenticate() { imap_ctx->server_status->authenticated = false; }

        void exitState(imap_function_return_code &ret, bool &flag)
        {
            ret = function_return_exit;
            flag = false;
        }

#if defined(ENABLE_DEBUG)
        void setDebugState(imap_state state, const String &msg)
        {
            err_timer.stop();
            tState() = state;
            setDebug(imap_ctx, msg);
        }
#endif

        bool serverConnected()
        {
#if defined(ESP32)
            // In ESP32 v3.x, if WiFiClientSecure was used, it can hang when.
            // 1. connect() in ssl or plain
            // 2. stop()
            // 3. setPlainStart()
            // 4. connected() <--- hung because of peek_net_receive() until timeed out

            // The work around is to define the fdset for write in the lwIP's select function
            // https://github.com/espressif/arduino-esp32/blob/master/libraries/NetworkClientSecure/src/ssl_client.cpp#L455

            // Since we don't know the type of Arduino Client that is assigned to the SMTPClient,
            // the status flag (serverStatus) will be used instead of calling Client's
            // connected() directly.
            return serverStatus();
#else
            return imap_ctx->client && imap_ctx->client->connected();
#endif
        }

        void stopImpl(bool forceStop = false)
        {
            if (forceStop || serverConnected())
                imap_ctx->client->stop();

            serverStatus() = false;
            imap_ctx->server_status->secured = false;
            imap_ctx->server_status->server_greeting_ack = false;
            imap_ctx->server_status->authenticated = false;
            clearAllProcessFlags();
            cState() = imap_state_prompt;
        }

        // current message index
        int &cMsgIndex() { return imap_ctx->cur_msg_index; }

        // current message
        imap_msg_ctx &cMsg()
        {
            if (cMsgIndex() >= (int)messagesVec().size())
            {
                imap_msg_ctx d;
                messagesVec().push_back(d);
            }
            return messagesVec()[cMsgIndex()];
        }

        // current file index
        int &cFileIndex() { return cMsg().cur_file_index; }

        // current message number
        uint32_t cMsgNum() { return msgNumVec().size() > 0 ? msgNumVec()[cMsgIndex()] : 0; }

        // Search result
        std::vector<uint32_t> &msgNumVec() { return imap_ctx->cb_data.msgNums; }

        // messages list
        std::vector<imap_msg_ctx> &messagesVec() { return imap_ctx->messages; }

        // current state
        imap_state &cState() { return imap_ctx->server_status->state_info.state; }

        // response type
        imap_response_types &cType() { return imap_ctx->resp_type; }

        // target state
        imap_state &tState() { return imap_ctx->server_status->state_info.target; }

        // current function return code
        imap_function_return_code &cCode() { return imap_ctx->server_status->ret; }

        bool &serverStatus() { return imap_ctx->server_status->connected; }

        static void print(imap_context *imap_ctx)
        {
            imap_ctx->status->state = (imap_ctx->server_status->state_info.target > 0 ? imap_ctx->server_status->state_info.target : imap_ctx->server_status->state_info.state);
            if (imap_ctx->cb.resp)
                imap_ctx->cb.resp(*imap_ctx->status);
        }

        String errMsg(int code)
        {
            String msg;
#if defined(ENABLE_DEBUG) || defined(ENABLE_CORE_DEBUG)
            msg = rd_err(code);
            if (msg.length() == 0)
            {
                switch (code)
                {
                case IMAP_ERROR_RESPONSE:
                    msg = "server returning error";
                    break;
                case IMAP_ERROR_NO_MAILBOX:
                    msg = "No mailbox selected";
                    break;
                case IMAP_ERROR_INVALID_SEARCH_CRITERIA:
                    msg = "Invalid search criteria";
                    break;
                case IMAP_ERROR_MODSEQ_WAS_NOT_SUPPORTED:
                    msg = "Modsequences does not support";
                    break;
                case IMAP_ERROR_IDLE_NOT_SUPPORTED:
                    msg = "Mailbox idling does not support";
                    break;
                case IMAP_ERROR_MESSAGE_NOT_EXISTS:
                    msg = "Message does not exist";
                    break;
                case IMAP_ERROR_PROCESSING:
                    msg = "The last process does not yet finished";
                    break;
                case IMAP_ERROR_MAILBOX_NOT_EXISTS:
                    msg = "The selected mailbox does not exist";
                    break;
                case IMAP_ERROR_NO_CALLBACK:
                    msg = "No FileCallback and DataCallback are assigned";
                    break;
                case IMAP_ERROR_COMMAND_NOT_ALLOW:
                    msg = "This command is not allowed";
                    break;
                case IMAP_ERROR_FETCH_MESSAGE:
                    msg = "Fetch message failed";
                    break;
                default:
                    msg = "Unknown";
                    break;
                }
            }
#else
            (void)code;
#endif
            return msg;
        }
    };
}
#endif
#endif