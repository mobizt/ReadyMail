#ifndef IMAP_SEND_H
#define IMAP_SEND_H
#if defined(ENABLE_IMAP)
#include <Arduino.h>
#include "Common.h"
#include "IMAPBase.h"
#include "IMAPConnection.h"
#include "IMAPResponse.h"

namespace ReadyMailIMAP
{
    class IMAPSend : public IMAPBase
    {
        friend class IMAPClient;

    private:
        IMAPConnection *conn = nullptr;
        IMAPResponse *res = nullptr;
        NumString numString;

        void begin(imap_context *imap_ctx, IMAPResponse *res, IMAPConnection *conn)
        {
            this->conn = conn;
            this->res = res;
            beginBase(imap_ctx);
        }

        imap_function_return_code loop()
        {
            if (cState() != imap_state_prompt && serverStatus())
            {
                switch (cCode())
                {
                case function_return_failure:
                    setError(imap_ctx, __func__, IMAP_ERROR_RESPONSE);
                    break;

                case function_return_success:
                    process();
                    break;

#if defined(ENABLE_IMAP_APPEND)
                case function_return_continue:
                    if (imap_ctx->smtp && cState() == imap_state_append_last)
                    {
                        imap_ctx->smtp->loop();
                        return cCode();
                    }
                    break;
#endif
                default:
                    break;
                }
            }
            return cCode();
        }

        void process()
        {
            String buf;
            switch (cState())
            {
            case imap_state_send_command:
                setDebug(imap_ctx, "The command is sent successfully\n");
                exitState(cCode(), imap_ctx->options.processing);
                break;

            case imap_state_logout:
                setDebug(imap_ctx, "The client is loged out successfully\n");
                deAuthenticate();
                exitState(cCode(), imap_ctx->options.processing);
                break;

            case imap_state_list:
                setDebug(imap_ctx, "The mailboxes are listed successfully\n");
                exitState(cCode(), imap_ctx->options.processing);
                imap_ctx->options.mailboxes_updated = false;
                break;

            case imap_state_examine:
            case imap_state_select:
                setDebug(imap_ctx, "The \"" + imap_ctx->current_mailbox + "\" is selected successfully\n");
                exitState(cCode(), imap_ctx->options.processing);
                break;

            case imap_state_search:
                if (imap_ctx->cb_data.searchCount)
                    rd_print_to(buf, 100, "The \"%s\" is searched successfully, found %d messages\n", imap_ctx->current_mailbox.c_str(), imap_ctx->cb_data.searchCount);
                else
                    rd_print_to(buf, 100, "The \"%s\" is searched successfully, no messages found\n", imap_ctx->current_mailbox.c_str());
                setDebug(imap_ctx, buf);
                messagesVec().clear();
                cMsgIndex() = 0;
                if (imap_ctx->cb.data)
                {
                    if (cMsgNum() > 0)
                    {
                        imap_ctx->options.fetch_number = cMsgNum();
                        imap_ctx->options.uid_fetch = imap_ctx->options.uid_search;
                        sendFetch(imap_fetch_envelope);
                    }
                    else
                        exitState(cCode(), imap_ctx->options.searching);
                }
                else
                    exitState(cCode(), imap_ctx->options.searching);
                break;

            case imap_state_fetch_envelope:
                setDebug(imap_ctx, "The message " + getFetchString() + " envelope is fetched successfully\n");
                if (imap_ctx->options.searching)
                {
                    if (cMsgIndex() >= (int)msgNumVec().size() - 1)
                    {
                        exitState(cCode(), imap_ctx->options.searching);
                        exitState(cCode(), imap_ctx->options.processing);
                        cMsgIndex() = 0;
                    }
                    else
                    {
                        cMsgIndex()++;
                        imap_ctx->options.fetch_number = cMsgNum();
                        imap_ctx->options.uid_fetch = imap_ctx->options.uid_search;
                        sendFetch(imap_fetch_envelope);
                    }
                }
                else
                    sendFetch(imap_fetch_body_structure);
                break;

            case imap_state_fetch_body_structure:
                setDebug(imap_ctx, "The message body structure is fetched successfully\n");
                sendFetch(imap_fetch_body_part);
                break;

            case imap_state_fetch_body_part:
                setDebug(imap_ctx, "The message body[" + cPart().section + "] is fetched successfully\n");
                cPartIndex()++;
                if (cPartIndex() == (int)cMsg().parts.size())
                {
                    setDebug(imap_ctx, "The message " + getFetchString() + " is fetched successfully\n");
                    exitState(cCode(), imap_ctx->options.processing);
                    cMsgIndex() = 0;
                }
                else
                    sendFetch(imap_fetch_body_part);
                break;

#if defined(ENABLE_IMAP_APPEND)
            case imap_state_append_init:

                imap_ctx->smtp_ctx->options.imap_mode = true;
                imap_ctx->smtp->send(imap_ctx->msg, imap_ctx->smtp_ctx->options.notify, imap_ctx->options.await);
                setState(imap_state_append_last);
                break;

            case imap_state_append_last:
                cMsgIndex() = 0;
                releaseSMTP();
                setDebug(imap_ctx, "The message is appended to selected mailbox successfully\n");
                exitState(cCode(), imap_ctx->options.processing);
                break;
#endif
            case imap_state_idle:
                setDebug(imap_ctx, "The mailbox idling is started successfully\n");
                break;

            case imap_state_done:
                setDebug(imap_ctx, "The mailbox idling is stopped successfully\n");
                exitState(cCode(), imap_ctx->options.idling);
                imap_ctx->options.processing = false;
                break;

            case imap_state_close:
                deAuthenticate();
                exitState(cCode(), imap_ctx->options.processing);
                setDebug(imap_ctx, "The \"" + imap_ctx->current_mailbox + "\" is closed successfully\n");
                imap_ctx->options.mailbox_selected = false;
                imap_ctx->current_mailbox.remove(0, imap_ctx->current_mailbox.length());
                break;

            default:
                break;
            }
        }

        String getFetchString() { return imap_ctx->options.uid_fetch ? "UID" : "" + numString.get(imap_ctx->options.fetch_number); }

        bool sendFetch(imap_fetch_mode mode)
        {
            String buf;
            imap_state state = imap_state_fetch_envelope;
            setProcessFlag(imap_ctx->options.processing);

            if (mode == imap_fetch_envelope)
            {
                state = imap_state_fetch_envelope;
                imap_ctx->options.skipping = false;

                if (imap_ctx->options.searching)
                    setDebugState(state, "Fetching message " + getFetchString() + " envelope...");

                // Fetching full for ENVELOPE and BODY to count attachment.
                rd_print_to(buf, 200, " %sFETCH %d FULL", imap_ctx->options.uid_fetch ? "UID " : "", imap_ctx->options.fetch_number);
            }
            else if (mode == imap_fetch_body_structure)
            {
                state = imap_state_fetch_body_structure;
                setDebugState(state, "Fetching message " + getFetchString() + " body...");
                rd_print_to(buf, 200, " %sFETCH %d BODYSTRUCTURE", imap_ctx->options.uid_fetch ? "UID " : "", imap_ctx->options.fetch_number);
            }
            else if (mode == imap_fetch_body_part)
            {
                state = imap_state_fetch_body_part;
                String name = cPart().name;
                while (name == "mixed" || name == "alternative" || name == "related")
                {
                    cPartIndex()++;
                    if (cPartIndex() >= (int)cMsg().parts.size())
                        break;
                    name = cPart().name;
                }
                uint32_t sz = numString.toNum(res->parser.getField(cPart(), non_multipart_field_size).c_str());
                bool sizeLimit = sz > imap_ctx->options.part_size_limit;
                String section = cPart().section.c_str();
                setDebugState(state, "Fetching message body[" + cPart().section + "]...");

                if (sizeLimit)
                {
                    String msg;
                    rd_print_to(msg, 200, "Body part size exceeds the limit (%s/%d), skipping...", res->parser.getField(cPart(), non_multipart_field_size).c_str(), imap_ctx->options.part_size_limit);
                    setDebugState(state, msg);
                    imap_ctx->options.skipping = true;
                }

                if (!sizeLimit && cPartIndex() < (int)cMsg().parts.size())
                    rd_print_to(buf, 200, " %sFETCH %d BODY%s[%s]", imap_ctx->options.uid_fetch ? "UID " : "", imap_ctx->options.fetch_number, imap_ctx->options.read_only_mode ? ".PEEK" : "", section.c_str());
            }

            if (buf.length() && !tcpSend(true, 2, imap_ctx->tag.c_str(), buf.c_str()))
                return false;

            setState(state);
            return true;
        }

        bool sendLogout()
        {
            if (!tcpSend(true, 3, imap_ctx->tag.c_str(), " ", "LOGOUT"))
                return false;
            setState(imap_state_logout);
            return true;
        }

        bool sendCmd(const String &cmd)
        {
            imap_ctx->cmd = cmd;
            if (!tcpSend(true, 3, imap_ctx->tag.c_str(), " ", cmd.c_str()))
                return false;
            setState(imap_state_send_command);
            return true;
        }

        bool sendIdle()
        {
            if (cState() != imap_state_done && imap_ctx->options.idling && res->idle_timer.remaining() == 0)
                return sendDone();

            if (imap_ctx->options.idling)
                return true;

            err_timer.stop();

            setDebugState(imap_state_idle, "Starting the mailbox idling...");

            res->idle_timer.feed(imap_ctx->options.timeout.idle / 1000);
            if (!tcpSend(true, 3, imap_ctx->tag.c_str(), " ", "IDLE"))
                return false;

            setProcessFlag(imap_ctx->options.idling);
            setState(imap_state_idle);
            return true;
        }

        bool sendDone()
        {
            if (imap_ctx->options.processing)
                return true;

            setDebugState(imap_state_done, "Stopping the mailbox idling...");
            imap_ctx->options.processing = true;

            if (!tcpSend(true, 1, "DONE"))
                return false;
            setState(imap_state_done);
            return true;
        }

        void setState(imap_state state)
        {
            res->begin(imap_ctx);
            cState() = state;
            cCode() = function_return_undefined;
        }

        bool list()
        {
            imap_ctx->mailboxes->clear();
            if (!tcpSend(true, 3, imap_ctx->tag.c_str(), " ", "LIST \"\" *"))
                return false;
            setState(imap_state_list);
            return true;
        }

        bool isCondStoreSupported() { return imap_ctx->feature_caps[imap_read_cap_condstore]; }

        bool isModseqSupported() { return isCondStoreSupported() && !res->mailbox_info.noModseq; }

        bool select(const String &mailbox, imap_mailbox_mode mode)
        {
            bool exists = false;

            // The SELECT/EXAMINE command automatically deselects any currently selected mailbox
            // before attempting the new selection (RFC3501 p.33)
            // mailbox name should not close for re-selection otherwise the server returned * BAD Command Argument Error. 12

            // guards 3 seconds to prevent accidently frequently select the same mailbox with the same mode
            if (!imap_ctx->options.mailbox_selected && imap_ctx->current_mailbox == mailbox && millis() - imap_ctx->options.timeout.mailbox_selected < 3000)
            {
                if ((imap_ctx->options.read_only_mode && mode == mailbox_mode_examine) || (!imap_ctx->options.read_only_mode && mode == mailbox_mode_select))
                    return true;
            }

            if (mailbox.length() > 0)
            {
                for (int i = 0; i < (int)imap_ctx->mailboxes->size(); i++)
                {
                    if (mailbox == (*imap_ctx->mailboxes)[i][2])
                    {
                        exists = true;
                        if (imap_ctx->current_mailbox != mailbox)
                            imap_ctx->current_mailbox = mailbox;
                    }
                }
            }

            if (!exists && imap_ctx->mailboxes->size()) // Skip mailbox checking if list is not executed.
                return setError(imap_ctx, __func__, IMAP_ERROR_MAILBOX_NOT_EXISTS);

            if (imap_ctx->current_mailbox != mailbox)
                imap_ctx->current_mailbox = mailbox;

            res->mailbox_info.name = imap_ctx->current_mailbox;

            setProcessFlag(imap_ctx->options.processing);
            clearMailboxInfo();

            String buf;
            rd_print_to(buf, 120, " \"%s\"%s", mailbox.c_str(), isCondStoreSupported() ? " (CONDSTORE)" : "");
            if (!tcpSend(true, 4, imap_ctx->tag.c_str(), " ", mode == mailbox_mode_examine ? "EXAMINE" : "SELECT", buf.c_str()))
                return setError(imap_ctx, __func__, TCP_CLIENT_ERROR_SEND_DATA);

            setState(mode == mailbox_mode_examine ? imap_state_examine : imap_state_select);
            imap_ctx->options.timeout.mailbox_selected = millis();
            imap_ctx->options.read_only_mode = mode == mailbox_mode_examine;
            imap_ctx->options.mailbox_selected = true;
            return true;
        }

        bool close()
        {
            String buf;
            if (!tcpSend(true, 2, imap_ctx->tag.c_str(), " CLOSE"))
                return setError(imap_ctx, __func__, TCP_CLIENT_ERROR_SEND_DATA);

            setState(imap_state_close);
            return true;
        }

        bool search(const String &criteria)
        {
            msgNumVec().clear();
            imap_ctx->options.uid_search = criteria.indexOf("UID") > -1;
            imap_ctx->cb_data.searchCount = 0;

            if (!tcpSend(true, 3, imap_ctx->tag.c_str(), " ", criteria.c_str()))
                return setError(imap_ctx, __func__, TCP_CLIENT_ERROR_SEND_DATA);

            setProcessFlag(imap_ctx->options.searching);
            setState(imap_state_search);
            return true;
        }
#if defined(ENABLE_IMAP_APPEND)
        bool append(SMTPMessage &msg, const String &flags, const String &date, bool lastAppend)
        {
            String buf;
            releaseSMTP();
            imap_ctx->smtp = new SMTPClient(*imap_ctx->client);
            imap_ctx->msg = msg;
            imap_ctx->smtp_ctx = reinterpret_cast<smtp_context *>(imap_ctx->smtp->contextAddr());
            imap_ctx->smtp_ctx->client = imap_ctx->client;
            imap_ctx->smtp_ctx->server_status->connected = true;
            imap_ctx->smtp_ctx->options.accumulate = true;
            imap_ctx->smtp->send(imap_ctx->msg);
            imap_ctx->smtp_ctx->options.accumulate = false;
            imap_ctx->smtp_ctx->options.last_append = !imap_ctx->feature_caps[imap_read_cap_multiappend] ? true : lastAppend;

            String fla, dt;
            if (flags.length())
                rd_print_to(fla, 100, " (%s)", flags.c_str());
            if (date.length())
                rd_print_to(dt, 100, " \"%s\"", date.c_str());

            rd_print_to(buf, 100, "APPEND %s%s%s {%d}", imap_ctx->current_mailbox.c_str(), fla.c_str(), dt.c_str(), imap_ctx->smtp_ctx->options.data_len);

            setProcessFlag(imap_ctx->options.processing);

            if (!tcpSend(true, 3, imap_ctx->tag.c_str(), " ", buf.c_str()))
                return setError(imap_ctx, __func__, TCP_CLIENT_ERROR_SEND_DATA);

            setState(imap_state_append_init);
            return true;
        }

#endif

        bool fetch(uint32_t number, bool uid_fetch, uint32_t bodySizeLimit)
        {
            if (number == 0 || (uid_fetch && number > res->mailbox_info.nextUID) || (!uid_fetch && number > res->mailbox_info.msgCount))
                return setError(imap_ctx, __func__, IMAP_ERROR_MESSAGE_NOT_EXISTS);

            imap_ctx->options.part_size_limit = bodySizeLimit;
            messagesVec().clear();
            cMsgIndex() = 0;
            imap_ctx->options.fetch_number = number;
            imap_ctx->options.uid_fetch = uid_fetch;
            return sendFetch(imap_fetch_envelope);
        }

        void clearMailboxInfo()
        {
            res->mailbox_info.flags.clear();
            res->mailbox_info.permanentFlags.clear();
            res->mailbox_info.msgCount = 0;
            imap_ctx->cb_data.searchCount = 0;
        }
    };
}
#endif
#endif