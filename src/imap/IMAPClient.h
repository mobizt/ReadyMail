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
using namespace ReadyMailCallbackNS;

namespace ReadyMailIMAP
{
    class IMAPClient
    {
    public:
        /** Provides the list of mailboxes from IMAPClient::list() function.
         * Each item contains array of String represents attributes, delimiter and name properties of each mailbox.
         */
        std::vector<std::array<String, 3>> mailboxes;

        /** IMAPClient class constructor.
         *
         * @param client The Arduino client e.g. network client or SSL client.
         * @param tlsCallback Optional. The TLSHandshakeCallback callback function for performing the SSL/TLS handshake.
         * @param startTLS Optional. The boolean option to enable STARTTLS protocol upgrades.
         *
         * There are few usage scenarios.
         * 1. tlsCallback ❎ startTLS ❎, when no connection upgrade is required. The client can be any network client or SSL client.
         * 2. tlsCallback ✅ startTLS ✅, when connection upgrade is required (from non-encrypion to TLS using STARTTLS protocol).
         * 3. tlsCallback ✅ startTLS ❎, when connection upgrade is done without issuing the STARTTLS.
         * This scenario is special usage when you start using the SSL client in plain text mode for some network task that does not require SSL,
         * and start using it in encryption mode in this library by calling SMTPClient::connect().
         * 4. tlsCallback ❎ startTLS ✅, the same as scenario 1.
         * The SSL client using in scenario 2 and 3 should support protocol upgrades.
         */
        IMAPClient(Client &client, TLSHandshakeCallback tlsCallback = NULL, bool startTLS = false)
        {
            imap_ctx.auto_client = nullptr;
            imap_ctx.options.use_auto_client = false;
            server_status.start_tls = startTLS;
            imap_ctx.client = &client;
            imap_ctx.server_status = &server_status;
            imap_ctx.status = &resp_status;
            imap_ctx.mailboxes = &mailboxes;
            imap_ctx.auth_mode = true;
            conn.begin(&imap_ctx, tlsCallback, &res);
            sender.begin(&imap_ctx, &res, &conn);
        }

#if defined(READYCLIENT_SSL_CLIENT)
        /** SMTPClient class constructor.
         *
         * @param client The ReadyClient class object.
         *
         */
        IMAPClient(ReadyClient &client)
        {
            server_status.start_tls = false;
            imap_ctx.auto_client = &client;
            imap_ctx.options.use_auto_client = true;
            imap_ctx.client = &client.getClient();
            imap_ctx.server_status = &server_status;
            imap_ctx.status = &resp_status;
            imap_ctx.mailboxes = &mailboxes;
            imap_ctx.auth_mode = true;
            conn.begin(&imap_ctx, NULL, &res);
            sender.begin(&imap_ctx, &res, &conn);
        }
#endif

        /** IMAPClient class deconstructor.
         */
        ~IMAPClient() { stop(); };

        /** IMAP server connection.
         *
         * @param host The IMAP server host name to connect.
         * @param port The IMAP port to connect.
         * @param responseCallback Optional. The IMAPResponseCallback callback function that provides the instant status for the processing states for debugging.
         * @param ssl Optional. The boolean option to enable SSL connection (using in secure mode).
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing states.
         */
        bool connect(const String &host, uint16_t port, IMAPResponseCallback responseCallback = NULL, bool ssl = true, bool await = true)
        {
            imap_ctx.cb.resp = responseCallback;
            imap_ctx.ssl_mode = ssl;
            bool ret = conn.connect(host, port);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        /** Perform IMAPClient async processes and idling.
         * @param idling Optional. The boolean option to enable the idle process.
         * @param timeout Optional. The idling timeout in milliseconds. It should be less than 30 min and greater than 8 min.
         * This is required when await parameter is set false in the IMAPClient::connect(),
         * IMAPClient::authenticate(), IMAPClient::select(), IMAPClient::search(), IMAPClient::fetch(),
         * IMAPClient::fetchUID and IMAPClient::logout() functions or when idling process is required.
         */
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

        /** Stop the server connection and release the allocated resources.
         */
        void stop()
        {
            if (isConnected() && isAuthenticated())
                logout(true);
            conn.stop();
        }

        /** Provides the SMTP server authentication status.
         * @return boolean status of authentication.
         */
        bool isAuthenticated() { return conn.isAuthenticated(); }

        /** Provides server connection status.
         * @return boolean status of server connection.
         */
        bool isConnected() { return conn.isConnected(); }

        /** IMAP server authentication.
         *
         * @param email The user Email to authenticate.
         * @param param The user password, app password or access token depending auth parameter.
         * @param auth The readymail_auth_type enum for authentication type that provides in param parameter e.g.
         * readymail_auth_password, readymail_auth_accesstoken and readymail_auth_disabled.
         * By providing readymail_auth_disabled means, using in non-authentication mode.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         */
        bool authenticate(const String &email, const String &param, readymail_auth_type auth, bool await = true) { return authImpl(email, param, auth, await); }

        /** De-authentication or Signing out.
         *
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         */
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

        /** Set the option to enable STARTTLS.
         *
         * @param tlsCallback Optional. The TLSHandshakeCallback callback function for performing the SSL/TLS handshake.
         * @param value The value. True for enable STARTTLS, false to disable STARTTLS.
         */
        void setStartTLS(TLSHandshakeCallback tlsCallback, bool value)
        {
            imap_ctx.server_status->start_tls = value;
            conn.begin(&imap_ctx, tlsCallback, &res);
        }

        /** Send command to IMAP server.
         *
         * @param cmd The command to send.
         * @param cb Optional. The IMAPCustomComandCallback callback function to get the server untagged response.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         *
         * The following commands are not allowed.
         * DONE, LOGOUT, STARTTLS, IDLE, ID, CLOSE, AUTHENTICATE, LOGIN, SELECT and EXAMINE.
         */
        bool sendCommand(const String &cmd, IMAPCustomComandCallback cb, bool await = true)
        {
            validateMailboxesChange();

            imap_ctx.cb.command_response.text.remove(0, imap_ctx.cb.command_response.text.length());
            imap_ctx.cb.command_response.command.remove(0, imap_ctx.cb.command_response.command.length());
            imap_ctx.cb.command_response.errorCode = 0;
            imap_ctx.cb.command_response.isComplete = false;

            sender.setDebugState(imap_state_send_command, "Sending command...");
            sender.setDebugState(imap_state_send_command, cmd);

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

        /** Provides the idle changes status.
         *
         * @return boolean status of idle changes state.
         */
        bool available() { return imap_ctx.idle_available; }

        /** Provides the IMAP status information.
         *
         * @return IMAPStatus class object.
         */
        IMAPStatus status() { return *imap_ctx.status; }

        /** Provides the IMAP idle information.
         *
         * @return String of idle status.
         * The idle status provided here is in the following formats
         * [+] 123456 When the message number 123456 was added to the mailbox or new message is arrived.
         * [-] 123456 When the message number 123456 was removed or deleted from mailbox.
         * [=][/aaa /bbb ] 123456 When the message number 123456 status was changed as the existing flag /aaa and /bbb are assigned
         */
        String idleStatus() { return imap_ctx.idle_status; }

        /** Provides the current message index in the search result message list while fetching the message's envelope.
         *
         * @return number of index.
         */
        uint32_t currentMessage() { return imap_ctx.current_message; }

        /** List the mailboxes.
         *
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         *
         * The actual result stores IMAPClient::mailboxes list.
         */
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

        /** Provides the information of selected mailbox.
         * @return MailboxInfo object.
         */
        MailboxInfo getMailbox() { return res.mailbox_info; }

        /** Select the mailboxe.
         *
         * @param mailbox The name of folder/mailbox to select.
         * @param readOnly Optional. The boolean option for selecting the mailbox in read only mode.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         *
         * The name of folder/mailbox select here should be existed.
         */
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

        /** De-select or close the mailboxe.
         *
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         *
         * The client will be logged out after closed.
         */
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
        /** Add the message to the selected mailboxe.
         *
         * @param msg The SMTPMessage object to add.
         * @param flags The argument of flags.
         * @param date The RFC 2822 date of message e.g. "Fri, 18 Apr 2025 11:42:30 +0300".
         * @param lastAppend The boolean option set with true when the last message to append.
         * In case of MULTIAPPEND extension is supported, set this to false will append messages in single APPEND command.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         *
         * The name of folder/mailbox select here should be existed.
         */
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

        /** Serch the messages from the selected mailboxe.
         *
         * @param criteria The search criteria.
         * A search key can also be a parenthesized list of one or more search keys
         * (e.g., for use with the OR and NOT keys).
         *
         * Since IMAP protocol uses Polish notation, the search criteria which in the polish notation form can be.
         *
         * To search the message from "someone@email.com" with the subject "my subject" since 1 Jan 2021, your search criteria can be
         * UID SEARCH (OR SUBJECT "my subject" FROM "someone@email.com") SINCE "Fri, 1 Jan 2021 21:52:25 -0800"
         *
         * To search the message from "mail1@domain.com" or from "mail2@domain.com", the search criteria will be
         * UID SEARCH OR FROM mail1@domain.com FROM mail2@domain.com
         *
         * For more details on using parentheses, AND, OR and NOT search keys in search criteria.
         * https://www.limilabs.com/blog/imap-search-requires-parentheses
         *
         * Searching criteria consist of one or more search keys. When multiple keys are specified, the result is the intersection (AND function) of all the messages that match those keys.
         * Example:
         *
         * DELETED FROM "SMITH" SINCE 1-Feb-1994 refers to all deleted messages from Smith that were placed in the mailbox since February 1, 1994.
         *
         * A search key can also be a parenthesized list of one or more search keys (e.g., for use with the OR and NOT keys).
         * SINCE 10-Feb-2019 will search all messages that received since 10 Feb 2019
         * UID SEARCH ALL will seach all message which will return the message UID that can be use later for fetch one or more messages.
         *
         * The following keywords can be used for the search criteria.
         *
         * ALL - All messages in the mailbox; the default initial key for ANDing.
         *
         * ANSWERED - Messages with the \Answered flag set.
         *
         * BCC - Messages that contain the specified string in the envelope structure's BCC field.
         *
         * BEFORE - Messages whose internal date (disregarding time and timezone) is earlier than the specified date.
         *
         * BODY - Messages that contain the specified string in the body of the message.
         *
         * CC - Messages that contain the specified string in the envelope structure's CC field.
         *
         * DELETED - Messages with the \Deleted flag set.
         *
         * DRAFT - Messages with the \Draft flag set.
         *
         * FLAGGED - Messages with the \Flagged flag set.
         *
         * FROM - Messages that contain the specified string in the envelope structure's FROM field.
         *
         * HEADER - Messages that have a header with the specified field-name (as defined in [RFC-2822])
         * and that contains the specified string in the text of the header (what comes after the colon).
         *
         * If the string to search is zero-length, this matches all messages that have a header line with
         * the specified field-name regardless of the contents.
         *
         * KEYWORD - Messages with the specified keyword flag set.
         *
         * LARGER - Messages with an (RFC-2822) size larger than the specified number of octets.
         *
         * NEW - Messages that have the \Recent flag set but not the \Seen flag.
         * This is functionally equivalent to "(RECENT UNSEEN)".
         *
         * NOT - Messages that do not match the specified search key.
         *
         * OLD - Messages that do not have the \Recent flag set. This is functionally equivalent to
         * "NOT RECENT" (as opposed to "NOT NEW").
         *
         * ON - Messages whose internal date (disregarding time and timezone) is within the specified date.
         *
         * OR - Messages that match either search key.
         *
         * RECENT - Messages that have the \Recent flag set.
         *
         * SEEN - Messages that have the \Seen flag set.
         *
         * SENTBEFORE - Messages whose (RFC-2822) Date: header (disregarding time and timezone) is earlier than the specified date.
         *
         * SENTON - Messages whose (RFC-2822) Date: header (disregarding time and timezone) is within the specified date.
         *
         * SENTSINCE - Messages whose (RFC-2822) Date: header (disregarding time and timezone) is within or later than the specified date.
         *
         * SINCE - Messages whose internal date (disregarding time and timezone) is within or later than the specified date.
         *
         * SMALLER - Messages with an (RFC-2822) size smaller than the specified number of octets.
         *
         * SUBJECT - Messages that contain the specified string in the envelope structure's SUBJECT field.
         *
         * TEXT - Messages that contain the specified string in the header or body of the message.
         *
         * TO - Messages that contain the specified string in the envelope structure's TO field.
         *
         * UID - Messages with unique identifiers corresponding to the specified unique identifier set.
         *
         * Sequence set ranges are permitted.
         *
         * UNANSWERED - Messages that do not have the \Answered flag set.
         *
         * UNDELETED - Messages that do not have the \Deleted flag set.
         *
         * UNDRAFT - Messages that do not have the \Draft flag set.
         *
         * UNFLAGGED - Messages that do not have the \Flagged flag set.
         *
         * UNKEYWORD - Messages that do not have the specified keyword flag set.
         *
         * UNSEEN - Messages that do not have the \Seen flag set.
         *
         * @param searchLimit The maximum number of message (number or UID) that can store in the message list.
         * @param recentSort The boolean option for recent sort order.
         * @param dataCallback The IMAPDataCallback callback function that provides the instant information of processing state.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         */
        bool search(const String &criteria, uint32_t searchLimit, bool recentSort, IMAPDataCallback dataCallback, bool await = true)
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

            if (lcriteria.indexOf("modseq") > -1 && res.mailbox_info.highestModseq == 0 && res.mailbox_info.noModseq)
                return sender.setError(&imap_ctx, __func__, IMAP_ERROR_MODSEQ_WAS_NOT_SUPPORTED);

            imap_ctx.options.search_limit = searchLimit;
            imap_ctx.options.recent_sort = recentSort;
            imap_ctx.cb.data = dataCallback;

            bool ret = sender.search(criteria);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        /** Fetch the message in selected mailbox by UID.
         *
         * @param uid The message UID.
         * @param dataCallback The IMAPDataCallback callback function that provides the instant information of processing state.
         * @param fileCallback Optional. The FileCallback callback function that provides the file openning and removing operations for file/attachment download.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @param bodySizeLimit The maximum size of body part content that can be download or stream in bytes.
         * @param downloadFolder The name of folder that stores the downloaded files.
         * @return boolean status of processing state.
         */
        bool fetchUID(uint32_t uid, IMAPDataCallback dataCallback, FileCallback fileCallback = NULL, bool await = true, uint32_t bodySizeLimit = 5 * 1024 * 1024, const String &downloadFolder = "")
        {
            validateMailboxesChange();
#if defined(ENABLE_FS)
            imap_ctx.cb.file = fileCallback;
            imap_ctx.cb.download_path = downloadFolder;
#endif
            imap_ctx.cb.data = dataCallback;
            return fetchImpl(uid, true, await, bodySizeLimit);
        }

        /** Fetch the message in selected mailbox by number or message sequence.
         *
         * @param number The message number.
         * @param dataCallback The IMAPDataCallback callback function that provides the instant information of processing state.
         * @param fileCallback Optional. The FileCallback callback function that provides the file openning and removing operations for file/attachment download.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the IMAPClient::loop() in the loop
         * to handle the async processes.
         * @param bodySizeLimit The maximum size of body part content that can be download or stream in bytes.
         * @param downloadFolder The name of folder that stores the downloaded files.
         * @return boolean status of processing state.
         */
        bool fetch(uint32_t number, IMAPDataCallback dataCallback, FileCallback fileCallback = NULL, bool await = true, uint32_t bodySizeLimit = 5 * 1024 * 1024, const String &downloadFolder = "")
        {
            validateMailboxesChange();
#if defined(ENABLE_FS)
            imap_ctx.cb.file = fileCallback;
            imap_ctx.cb.download_path = downloadFolder;
#endif
            imap_ctx.cb.data = dataCallback;
            return fetchImpl(number, false, await, bodySizeLimit);
        }

        /** Provides the message list of number or UID from search.
         *
         * @return std::vector<uint32_t> list or array.
         */
        std::vector<uint32_t> &searchResult() { return sender.msgNumVec(); }

        /** Provides the command response when using IMAPClient::sendCommand().
         *
         * @return String of untagged response.
         */
        IMAPCommandResponse commandResponse() { return imap_ctx.cb.command_response; }

    private:
        IMAPConnection conn;
        IMAPResponse res;
        IMAPSend sender;
        imap_server_status_t server_status;
        imap_response_status_t resp_status;
        imap_context imap_ctx;

        void validateMailboxesChange()
        {
            // blocking, only occurred when sending create and delete commands
            if (imap_ctx.options.mailboxes_updated)
                list();
        }

        bool awaitLoop()
        {
            imap_function_return_code code = function_return_undefined;
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

        bool fetchImpl(int number, bool uidFetch, bool await, uint32_t bodySizeLimit)
        {
            imap_ctx.options.fetch_number = number;
            imap_ctx.options.uid_fetch = uidFetch;
            sender.setDebugState(imap_state_fetch_envelope, "Fetching message " + sender.getFetchString() + " envelope...");

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

            bool ret = sender.fetch(number, uidFetch, bodySizeLimit);
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