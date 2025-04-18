#ifndef IMAP_SESSION_H
#define IMAP_SESSION_H
#if defined(ENABLE_IMAP)
#include <Arduino.h>
#include "Common.h"
#include "IMAPSend.h"
#include "IMAPConnection.h"
#include "IMAPResponse.h"
#include "Parser.h"

using namespace ReadyMailIMAP;
using namespace ReadyMailNS;

namespace ReadyMailIMAP
{
    class IMAPClient
    {
    public:
        std::vector<std::array<String, 3>> mailboxes;
        IMAPClient(Client &client, TLSHandshakeCallback cb = NULL, bool startTLS = false)
        {
            server_status.start_tls = startTLS;
            imap_ctx.client = &client;
            imap_ctx.server_status = &server_status;
            imap_ctx.status = &status;
            imap_ctx.mailboxes = &mailboxes;
            imap_ctx.auth_mode = true;
            conn.begin(&imap_ctx, cb, &res);
            sender.begin(&imap_ctx, &res, &conn);
        }

        ~IMAPClient() {};

        bool connect(const String &host, uint16_t port, IMAPResponseCallback cb = NULL, bool ssl = true, bool await = true)
        {
            imap_ctx.cb.resp = cb;
            imap_ctx.ssl_mode = ssl;
            bool ret = conn.connect(host, port);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        void loop(bool idling = false, uint32_t timeout = DEFAULT_IDLE_TIMEOUT)
        {
            imap_ctx.idle_available = false;
            conn.loop();
            sender.loop();
            if (idling && (!imap_ctx.auth_mode || isAuthenticated()))
            {
                sendIdle();
                if (imap_ctx.options.timeout.idle != timeout && timeout > DEFAULT_IDLE_TIMEOUT)
                {
                    imap_ctx.options.timeout.idle = timeout;
                    res.idle_timer.feed(imap_ctx.options.timeout.idle / 1000);
                }
            }
        }

        void stop()
        {
            if (isConnected() && isAuthenticated())
                logout(true);
            conn.stop();
        }

        bool isAuthenticated() { return conn.isAuthenticated(); }

        bool isConnected() { return conn.isConnected(); }

        bool authenticate(const String &email, const String &param, readymail_auth_type auth, bool await = true) { return authImpl(email, param, auth, await); }

        bool logout(bool await = true)
        {
            sender.setDebugState(imap_state_logout, "Logging out...");
            if (!conn.isIdleState(__func__))
                return false;

            bool ret = sender.sendLogout();
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        void validateMailboxesChange()
        {
            // blocking, only occurred when sending create and delete commands
            if (imap_ctx.options.mailboxes_updated)
                list();
        }

        bool sendCommand(const String &cmd, IMAPCustomComandCallback cb, bool await = true)
        {
            validateMailboxesChange();

            sender.setDebugState(imap_state_custom_command, "Sending custom command \"" + cmd + "\"...");

            if (!conn.isIdleState(__func__))
                return false;

            if (!ready(__func__, false))
                return false;

            String lcmd = " " + cmd;
            lcmd.toLowerCase();

            if (lcmd.indexOf(" done") > -1 || lcmd.indexOf(" logout") > -1 || lcmd.indexOf(" starttls") > -1 || lcmd.indexOf(" idle") > -1 || lcmd.indexOf(" id ") > -1 || lcmd.indexOf(" close") > -1 || lcmd.indexOf(" authenticate") > -1 || lcmd.indexOf(" login") > -1 || lcmd.indexOf(" select") > -1 || lcmd.indexOf(" examine") > -1)
                return sender.setError(&imap_ctx, __func__, IMAP_ERROR_COMMAND_NOT_ALLOW);

            imap_ctx.cb.cmd = cb;

            if (lcmd.indexOf(" create") > -1 || lcmd.indexOf(" delete") > -1)
                imap_ctx.options.mailboxes_updated = true;

            bool ret = sender.sendCmd(cmd);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        int available() { return imap_ctx.idle_available; }

        imap_state currentState() { return sender.cState(); }

        String idleStatus() { return imap_ctx.idle_status; }

        uint32_t currentMessage() { return imap_ctx.current_message; }

        bool list(bool await = true)
        {
            sender.setDebugState(imap_state_list, "Listing mailboxes...");
            if (!ready(__func__, false))
                return false;

            bool ret = sender.list();
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        MailboxInfo getMailbox() { return res.mailbox_info; }

        bool select(const String &mailbox, bool readOnly = true, bool await = true)
        {
            validateMailboxesChange();

            sender.setDebugState(readOnly ? imap_state_examine : imap_state_select, "Selecting \"" + mailbox + "\"...");

            if (!conn.isIdleState(__func__))
                return false;

            if (!ready(__func__, false))
                return false;

            if (imap_ctx.options.idling)
                sendDone();

            bool ret = sender.select(mailbox, readOnly ? mailbox_mode_examine : mailbox_mode_select);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        bool close(bool await = true)
        {
            if (imap_ctx.current_mailbox.length() > 0)
                sender.setDebugState(imap_state_close, "Closing \"" + imap_ctx.current_mailbox + "\"...");
            else
                sender.setDebugState(imap_state_close, "Closing mailbox...");

            if (!conn.isIdleState(__func__))
                return false;

            if (!ready(__func__, true))
                return false;

            if (imap_ctx.options.idling)
                sendDone();

            bool ret = sender.close();
            if (ret && await)
                return awaitLoop();
            return ret;
        }

#if defined(ENABLE_IMAP_APPEND)
        bool append(SMTPMessage &msg, const String &flags, const String &date, bool lastAppend, bool await = true)
        {
            sender.setDebugState(imap_state_append, "Appending message...");
            if (!conn.isIdleState(__func__))
                return false;

            if (!ready(__func__, true))
                return false;

            imap_ctx.options.await = await;
            bool ret = sender.append(msg, flags, date, lastAppend);
            if (ret && await)
                return awaitLoop();
            return ret;
        }
#endif

        bool search(const String &criteria, uint32_t searchLimit, bool recentSort, DataCallback dataCallback, bool await = true)
        {
            if (imap_ctx.current_mailbox.length() > 0)
                sender.setDebugState(imap_state_search, "Searching \"" + imap_ctx.current_mailbox + "\"...");
            else
                sender.setDebugState(imap_state_search, "Searching mailbox...");

            if (!conn.isIdleState(__func__))
                return false;

            if (!ready(__func__, true))
                return false;

            String lcriteria = criteria;
            lcriteria.toLowerCase();

            if (lcriteria.length() == 0 || lcriteria.indexOf("fetch ") > -1 || lcriteria.indexOf("search ") == -1 || lcriteria.indexOf("uid ") > 0 || (lcriteria.indexOf("uid ") == -1 && lcriteria.indexOf("search ") > 0))
                return sender.setError(&imap_ctx, __func__, IMAP_ERROR_INVALID_SEARCH_CRITERIA);

            if (lcriteria.indexOf("modseq") > -1 && !res.mailbox_info.highestModseq.length() && res.mailbox_info.noModseq)
                return sender.setError(&imap_ctx, __func__, IMAP_ERROR_MODSEQ_WAS_NOT_SUPPORTED);

            imap_ctx.options.search_limit = searchLimit;
            imap_ctx.options.recent_sort = recentSort;
            imap_ctx.cb.data = dataCallback;

            bool ret = sender.search(criteria);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        bool fetchUID(int uid, DataCallback dataCallback, FileCallback fileCallback = NULL, bool await = true, uint32_t bodySizeLimit = 5 * 1024 * 1024, const String &downloadFolder = "")
        {
            validateMailboxesChange();
#if defined(ENABLE_FS)
            imap_ctx.cb.file = fileCallback;
            imap_ctx.cb.download_path = downloadFolder;
#endif
            imap_ctx.cb.data = dataCallback;
            return fetchImpl(uid, true, await, bodySizeLimit);
        }

        bool fetch(int number, DataCallback dataCallback, FileCallback fileCallback = NULL, bool await = true, uint32_t bodySizeLimit = 5 * 1024 * 1024, const String &downloadFolder = "")
        {
            validateMailboxesChange();
#if defined(ENABLE_FS)
            imap_ctx.cb.file = fileCallback;
            imap_ctx.cb.download_path = downloadFolder;
#endif
            imap_ctx.cb.data = dataCallback;
            return fetchImpl(number, false, await, bodySizeLimit);
        }

        std::vector<uint32_t> &searchResult() { return sender.msgNumVec(); }

    private:
        IMAPConnection conn;
        IMAPResponse res;
        IMAPSend sender;
        imap_server_status_t server_status;
        imap_response_status_t status;
        imap_context imap_ctx;

        bool awaitLoop()
        {
            imap_function_return_code code;
            while (code != function_return_exit && code != function_return_failure)
            {
                code = conn.loop();
                if (code != function_return_failure)
                    code = sender.loop();
            }
            imap_ctx.server_status->state_info.state = imap_state_prompt;
            return code != function_return_failure;
        }

        bool authImpl(const String &email, const String &param, readymail_auth_type auth, bool await = true)
        {
            if (auth == readymail_auth_disabled)
            {
                imap_ctx.server_status->authenticated = false;
                imap_ctx.auth_mode = false;
                return true;
            }
            else
                imap_ctx.auth_mode = true;

            if (imap_ctx.options.processing)
                return true;

            if (!isAuthenticated())
                sender.setDebugState(imap_state_authentication, "Authenticating...");

            if (!conn.isIdleState("authenticate"))
                return false;

            conn.storeCredentials(email, param, auth == readymail_auth_accesstoken);

            if (!isConnected())
            {
                stop();
                return conn.setError(&imap_ctx, __func__, TCP_CLIENT_ERROR_NOT_CONNECTED);
            }

            if (isAuthenticated())
                return true;

            bool ret = conn.auth(email, param, auth == readymail_auth_accesstoken);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        bool fetchImpl(int number, bool fetchUID, bool await, uint32_t bodySizeLimit)
        {
            sender.setFechNumber(number, fetchUID);
            sender.setDebugState(imap_state_fetch_envelope, "Fetching message " + (imap_ctx.options.fetch_uid.length() ? imap_ctx.options.fetch_uid : imap_ctx.options.fetch_number) + " envelope...");

            if (!conn.isIdleState(__func__))
                return false;

            if (!ready(__func__, true))
                return false;
#if defined(ENABLE_FS)
            if (!imap_ctx.cb.data && !imap_ctx.cb.file)
                return sender.setError(&imap_ctx, __func__, IMAP_ERROR_NO_CALLBACK);
#else
            if (!imap_ctx.cb.data)
                return sender.setError(&imap_ctx, __func__, IMAP_ERROR_NO_CALLBACK);
#endif

            if (imap_ctx.options.idling)
            {
                sender.sendDone();
                awaitLoop();
            }

            bool ret = sender.fetch(number, fetchUID, bodySizeLimit);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        bool ready(const char *func, bool checkMailbox)
        {
            if (!isConnected())
            {
                stop();
                return sender.setError(&imap_ctx, func, TCP_CLIENT_ERROR_NOT_CONNECTED);
            }

            if (imap_ctx.auth_mode && !isAuthenticated())
                return sender.setError(&imap_ctx, func, AUTH_ERROR_UNAUTHENTICATE);

            if (checkMailbox && imap_ctx.current_mailbox.length() == 0)
                return sender.setError(&imap_ctx, func, IMAP_ERROR_NO_MAILBOX);

            return true;
        }

        bool sendIdle()
        {
            validateMailboxesChange();

            if (!ready(__func__, true) || imap_ctx.options.processing)
                return false;

            if (!imap_ctx.feature_caps[imap_read_cap_idle])
                return sender.setError(&imap_ctx, __func__, IMAP_ERROR_IDLE_NOT_SUPPORTED);

            return sender.sendIdle();
        }

        bool sendDone(bool await = true)
        {
            bool ret = sender.sendDone();
            if (ret && await)
                return awaitLoop();
            return ret;
        }
    };
}

#endif
#endif