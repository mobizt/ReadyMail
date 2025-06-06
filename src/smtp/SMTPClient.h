/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SMTP_CLIENT_H
#define SMTP_CLIENT_H
#if defined(ENABLE_SMTP)
#include <Arduino.h>
#include "Common.h"
#include "SMTPMessage.h"
#include "SMTPConnection.h"
#include "SMTPSend.h"

using namespace ReadyMailSMTP;
using namespace ReadyMailCallbackNS;

namespace ReadyMailSMTP
{

    class SMTPClient
    {
        friend class IMAPSend;

    private:
        SMTPConnection conn;
        SMTPResponse res;
        SMTPSend sender;
        smtp_server_status_t server_status;
        smtp_response_status_t resp_status;
        smtp_context smtp_ctx;

        bool awaitLoop()
        {
            smtp_function_return_code code = function_return_undefined;
            while (code != function_return_exit && code != function_return_failure)
            {
                code = conn.loop();
                if (code != function_return_failure)
                    code = sender.loop();
            }
            smtp_ctx.server_status->state_info.state = smtp_state_prompt;
            sender.local_msg.clear();
            return code != function_return_failure;
        }

        bool authImpl(const String &email, const String &param, readymail_auth_type auth, bool await = true)
        {
            if (auth == readymail_auth_disabled)
            {
                smtp_ctx.server_status->authenticated = false;
                return true;
            }

            if (smtp_ctx.options.processing)
                return true;
#if defined(ENABLE_DEBUG)
            conn.setDebugState(smtp_state_authentication, "Authenticating...");
#endif
            if (!conn.isInitialized())
                return false;

            if (!isConnected())
                return conn.setError(__func__, TCP_CLIENT_ERROR_NOT_CONNECTED, "", false);

            if (isAuthenticated())
                return true;

            bool ret = conn.auth(email, param, auth == readymail_auth_accesstoken);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

    public:
        /** SMTPClient class constructor.
         *  The SMTPClient::begin() is needed to start using the client.
         */
        SMTPClient()
        {
            smtp_ctx.server_status = &server_status;
            smtp_ctx.status = &resp_status;
            conn.begin(&smtp_ctx, NULL, &res);
            sender.begin(&smtp_ctx, &res, &conn);
        }

        /** SMTPClient class constructor.
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
        explicit SMTPClient(Client &client, TLSHandshakeCallback tlsCallback = NULL, bool startTLS = false) { begin(client, tlsCallback, startTLS); }

#if defined(READYCLIENT_SSL_CLIENT)
        /** SMTPClient class constructor.
         *
         * @param client The ReadyClient class object.
         *
         */
        explicit SMTPClient(ReadyClient &client) { begin(client); }
#endif

        /** SMTPClient class deconstructor.
         */
        ~SMTPClient() { stop(); };

        /** Start using SMTPClient.
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
        void begin(Client &client, TLSHandshakeCallback tlsCallback = NULL, bool startTLS = false)
        {
            smtp_ctx.options.use_auto_client = false;
            server_status.start_tls = startTLS;
            smtp_ctx.client = &client;
            smtp_ctx.server_status = &server_status;
            smtp_ctx.status = &resp_status;
            conn.begin(&smtp_ctx, tlsCallback, &res);
            sender.begin(&smtp_ctx, &res, &conn);
        }

#if defined(READYCLIENT_SSL_CLIENT)
        /** Start using SMTPClient.
         *
         * @param client The ReadyClient class object.
         *
         */
        void begin(ReadyClient &client)
        {
            server_status.start_tls = false;
            smtp_ctx.auto_client = &client;
            smtp_ctx.options.use_auto_client = true;
            smtp_ctx.client = &client.getClient();
            smtp_ctx.server_status = &server_status;
            smtp_ctx.status = &resp_status;
            conn.begin(&smtp_ctx, NULL, &res);
            sender.begin(&smtp_ctx, &res, &conn);
        }
#endif

        /** SMTP server connection.
         *
         * @param host The SMTP server host name to connect.
         * @param port The SMTP port to connect.
         * @param responseCallback Optional. The SMTPResponseCallback callback function that provides the instant status for the processing states for debugging.
         * @param ssl Optional. The boolean option to enable SSL connection (using in secure mode).
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the SMTPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing states.
         */
        bool connect(const String &host, uint16_t port, SMTPResponseCallback responseCallback = NULL, bool ssl = true, bool await = true) { return connect(host, port, "" /* use loopback IP */, responseCallback, ssl, await); }

        /** SMTP server connection.
         *
         * @param host The SMTP server host name to connect.
         * @param port The SMTP port to connect.
         * @param domain The host/domain or IP for client identity. Leave this empty to use loopback IP (127.0.0.1).
         * @param responseCallback Optional. The SMTPResponseCallback callback function that provides the instant status for the processing states for debugging.
         * @param ssl Optional. The boolean option to enable SSL connection (using in secure mode).
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the SMTPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing states.
         */
        bool connect(const String &host, uint16_t port, const String &domain, SMTPResponseCallback responseCallback = NULL, bool ssl = true, bool await = true)
        {
#if defined(ENABLE_READYCLIENT)
            if (smtp_ctx.auto_client)
                smtp_ctx.auto_client->configPort(port, ssl, smtp_ctx.server_status->start_tls);
#endif
            smtp_ctx.resp_cb = responseCallback;
            bool ret = conn.connect(host, port, domain, ssl);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        /** SMTP server connection for custom SMTP command.
         *
         * @param host The SMTP server host name to connect.
         * @param port The SMTP port to connect.
         * @param commandcallback The SMTPCustomComandCallback callback function that provides the server response.
         * @param responseCallback Optional. The SMTPResponseCallback callback function that provides the instant status for the processing states for debugging.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the SMTPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing states.
         */
        bool connect(const String &host, uint16_t port, SMTPCustomComandCallback commandcallback, SMTPResponseCallback responseCallback = NULL, bool await = true)
        {
            smtp_ctx.cmd_ctx.cb = commandcallback;
            smtp_ctx.resp_cb = responseCallback;
            smtp_ctx.cmd_ctx.resp.text.remove(0, smtp_ctx.cmd_ctx.resp.text.length());
#if defined(ENABLE_DEBUG)
            sender.setDebugState(smtp_state_connect_command, "Connecting to \"" + host + "\" via port " + String(port) + "...");
#endif
            if (!conn.isInitialized())
                return false;

            sender.serverStatus() = smtp_ctx.client->connect(host.c_str(), port);
            if (!sender.serverStatus())
                return false;

            sender.setState(smtp_state_connect_command, smtp_server_status_code_220);

            if (sender.serverStatus() && await)
                return awaitLoop();
            return sender.serverStatus();
        }

        /** Provides the SMTP server authentication status.
         * @return boolean status of authentication.
         */
        bool isAuthenticated() { return conn.isAuthenticated(); }

        /** SMTP server authentication.
         *
         * @param email The user Email to authenticate.
         * @param param The user password, app password or access token depending auth parameter.
         * @param auth The readymail_auth_type enum for authentication type that provides in param parameter e.g.
         * readymail_auth_password, readymail_auth_accesstoken and readymail_auth_disabled.
         * By providing readymail_auth_disabled means, using in non-authentication mode.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the SMTPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         */
        bool authenticate(const String &email, const String &param, readymail_auth_type auth, bool await = true) { return authImpl(email, param, auth, await); }

        /** Provides server connection status.
         * @return boolean status of server connection.
         */
        bool isConnected() { return conn.isConnected(); }

        /** Send Email.
         *
         * @param message The SMTPMessage class object. If the await parameter is false, the message will be coppied
         * for internal async mode processing.
         * @param notify Optional. The Delivery Status Notification on SUCCESS,FAILURE, and DELAY. Set with comma separated value i.e SUCCESS,FAILURE,DELAY.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the SMTPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         */
        bool send(SMTPMessage &message, const String &notify = "", bool await = true)
        {
            bool ret = sender.send(message, notify, await);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        /** Send command to SMTP server.
         *
         * @param cmd The command to send.
         * @param cb Optional. The IMAPCustomComandCallback callback function to get the server untagged response.
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the SMTPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         *
         * The following commands are not allowed.
         * DONE, LOGOUT, STARTTLS, IDLE, ID, CLOSE, AUTHENTICATE, LOGIN, SELECT and EXAMINE.
         */
        bool sendCommand(const String &cmd, SMTPCustomComandCallback cb, bool await = true)
        {
            smtp_ctx.cmd_ctx.cb = cb;
            bool ret = sender.sendCmd(cmd);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        bool sendData(const String &data, SMTPCustomComandCallback cb, bool await = true)
        {
            smtp_ctx.cmd_ctx.cb = cb;
            bool ret = sender.sendData(data);
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        /** Perform SMTPClient async processes.
         * This is required when await parameter is set false in the SMTPClient::connect(),
         * SMTPClient::authenticate(), SMTPClient::send() and SMTPClient::logout() functions.
         */
        void loop()
        {
            conn.loop();
            sender.loop();
        }

        /** Stop the server connection and release the allocated resources.
         */
        void stop()
        {
            if (isConnected() && isAuthenticated())
                logout(true);
            conn.stop();
        }

        /** De-authentication or Signing out.
         *
         * @param await Optional. The boolean option for using in await or blocking mode.
         * For async mode, set this parameter with false and calling the SMTPClient::loop() in the loop
         * to handle the async processes.
         * @return boolean status of processing state.
         */
        bool logout(bool await = true)
        {
            bool ret = sender.sendQuit();
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
            smtp_ctx.server_status->start_tls = value;
            conn.begin(&smtp_ctx, tlsCallback, &res);
        }

        /** Provides the SMTP status information.
         *
         * @return SMTPStatus class object.
         */
        SMTPStatus status() { return *smtp_ctx.status; }

        /** Provides the SMTP command response information.
         *
         * @return SMTPStatus class object.
         */
        SMTPCommandResponse commandResponse() { return smtp_ctx.cmd_ctx.resp; }

        // Private used by other classes.
        uint32_t contextAddr() { return rd_cast<uint32_t>(&smtp_ctx); }

        SMTPMessage &getMessage() { return sender.local_msg; }
    };
}
#endif
#endif