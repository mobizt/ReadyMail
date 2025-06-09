/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SMTP_CONNECTION_H
#define SMTP_CONNECTION_H
#if defined(ENABLE_SMTP)
#include <Arduino.h>
#include "Common.h"
#include "SMTPResponse.h"

using namespace ReadyMailCallbackNS;

namespace ReadyMailSMTP
{
    class SMTPConnection : public SMTPBase
    {
        friend class SMTPClient;
        friend class SMTPSend;

    private:
        SMTPResponse *res = nullptr;
        TLSHandshakeCallback tls_cb = NULL;
        String host, domain = "127.0.0.1", email, password, access_token;
        uint16_t port = 0;
        ReadyTimer conn_timer;
        bool authenticating = false;

        bool connectImpl()
        {
            if (authenticating)
                return false;

            if (serverConnected())
                stop(true);

            authenticating = true;
#if defined(ENABLE_DEBUG)
            setDebugState(smtp_state_initial_state, "Connecting to \"" + host + "\" via port " + String(port) + "...");
#endif

            if (!isInitialized())
                return false;

            smtp_ctx->options.processing = true;
            serverStatus() = smtp_ctx->client->connect(host.c_str(), port);
            if (!serverStatus())
            {
                stop(conn_timer.remaining() == 0);
                if (smtp_ctx->resp_cb)
                    setError(__func__, conn_timer.remaining() == 0 ? TCP_CLIENT_ERROR_CONNECTION_TIMEOUT : TCP_CLIENT_ERROR_CONNECTION);
                authenticating = false;
                return false;
            }
            smtp_ctx->client->setTimeout(smtp_ctx->options.timeout.read);
            authenticating = false;
            setState(smtp_state_initial_state, smtp_server_status_code_220);
            return true;
        }

        bool auth()
        {
            if (!isConnected())
                return setError(__func__, TCP_CLIENT_ERROR_NOT_CONNECTED);

            smtp_ctx->options.processing = true;
            bool user = email.length() > 0 && password.length() > 0;
            bool sasl_auth_oauth = access_token.length() > 0 && res->auth_caps[smtp_auth_cap_xoauth2];
            bool sasl_login = res->auth_caps[smtp_auth_cap_login] && user;
            bool sasl_auth_plain = res->auth_caps[smtp_auth_cap_plain] && user;

            if (sasl_auth_oauth)
            {
                if (!res->auth_caps[smtp_auth_cap_xoauth2])
                    return setError(__func__, AUTH_ERROR_OAUTH2_NOT_SUPPORTED);

                if (!tcpSend(true, 3, "AUTH ", "XOAUTH2 ", rd_enc_oauth(email, access_token).c_str()))
                    return setError(__func__, TCP_CLIENT_ERROR_SEND_DATA);

                setState(smtp_state_auth_xoauth2, smtp_server_status_code_235);
            }
            else if (sasl_auth_plain)
            {
                if (!tcpSend(true, 3, "AUTH ", "PLAIN ", rd_enc_plain(email, password).c_str()))
                    return setError(__func__, TCP_CLIENT_ERROR_SEND_DATA);

                setState(smtp_state_auth_plain, smtp_server_status_code_235);
            }
            else if (sasl_login)
            {
                if (!tcpSend(true, 2, "AUTH ", "LOGIN"))
                    return setError(__func__, TCP_CLIENT_ERROR_SEND_DATA);

                setState(smtp_state_auth_login, smtp_server_status_code_334);
            }
            else
                return setError(__func__, AUTH_ERROR_AUTHENTICATION);
            return true;
        }

        void authLogin(bool user)
        {
            char *enc = rd_b64_enc(rd_cast<const unsigned char *>((user ? email.c_str() : password.c_str())), user ? email.length() : password.length());
            if (enc)
            {
                if (!tcpSend(true, 1, enc))
                    setError(__func__, TCP_CLIENT_ERROR_SEND_DATA);

                rd_free(&enc);
            }
            // expected server challenge response
            setState(user ? smtp_state_login_user : smtp_state_login_psw, user ? smtp_server_status_code_334 : smtp_server_status_code_235);
        }

        void stop(bool forceStop = false)
        {
            res->stop(forceStop);
            clearCreds();
        }

        void begin(smtp_context *smtp_ctx, TLSHandshakeCallback tlsCallback, SMTPResponse *res)
        {
            this->tls_cb = tlsCallback;
            this->res = res;
            beginBase(smtp_ctx);
            res->begin(smtp_ctx);
        }

        bool connect(const String &host, uint16_t port, const String &domain, bool ssl = true)
        {
            this->host = host;
            this->port = port;
            smtp_ctx->options.ssl_mode = ssl;
            // Reset the isComplete status.
            smtp_ctx->status->isComplete = false;

            if (serverConnected())
                stop(true);

            IPChecker checker;
            this->domain = checker.isValidHost(domain.c_str()) ? domain : READYMAIL_LOOPBACK_IPV4;
            conn_timer.feed(smtp_ctx->options.timeout.con / 1000);
            return connectImpl();
        }

        bool isConnected() { return serverConnected() && smtp_ctx->server_status->server_greeting_ack; }

        bool isProcessing() { return smtp_ctx->options.processing; }

        smtp_function_return_code loop()
        {
            if (cState() != smtp_state_prompt)
            {
                if (!serverStatus())
                    connectImpl();
                else
                {
                    res->handleResponse();
                    authenticate(cCode());
                }
            }
            return cCode();
        }

        void authenticate(smtp_function_return_code &ret)
        {
            if (ret == function_return_failure && (cState() == smtp_state_start_tls || (cState() >= smtp_state_login_user && cState() <= smtp_state_login_psw)))
            {
                stop(true);
                return;
            }

            switch (cState())
            {
            case smtp_state_initial_state:
                if (ret == function_return_continue && !smtp_ctx->server_status->start_tls)
                    tlsHandshake();
                else if (ret == function_return_success)
                    sendGreeting("EHLO ", true);
                break;

            case smtp_state_greeting:
                if (ret == function_return_failure)
                    sendGreeting("HELO ", false);
                else if (ret == function_return_success)
                {
                    res->feature_caps[smtp_send_cap_esmtp] = smtp_ctx->server_status->state_info.esmtp;
                    res->auth_caps[smtp_auth_cap_login] = true;
                    cState() = smtp_state_prompt;
                    // start TLS when needed, rfc3207
                    if (smtp_ctx->options.ssl_mode && res->auth_caps[smtp_auth_cap_starttls] && smtp_ctx->server_status->start_tls && (tls_cb || smtp_ctx->options.use_auto_client) && !smtp_ctx->server_status->secured)
                        startTLS();
                    else
                    {
                        smtp_ctx->server_status->server_greeting_ack = true;
                        ret = function_return_exit;
                        smtp_ctx->options.processing = false;
#if defined(ENABLE_DEBUG)
                        setDebug("Service is ready\n");
#endif
                    }
                }
                break;

            case smtp_state_start_tls:
                if (ret == function_return_success)
                    tlsHandshake();
                break;

            case smtp_state_start_tls_ack:
                cState() = smtp_state_prompt;
                if (statusCode() == smtp_server_status_code_220)
                {
                    ret = function_return_continue;
                    sendGreeting("EHLO ", true);
                }
                else
                {
                    ret = function_return_exit;
                    smtp_ctx->options.processing = false;
                }
                break;

            case smtp_state_auth_login:
            case smtp_state_login_user:
                if (ret == function_return_success)
                    authLogin(cState() == smtp_state_auth_login);
                break;

            case smtp_state_login_psw:
            case smtp_state_auth_xoauth2:
            case smtp_state_auth_plain:
                if (ret == function_return_success)
                {
                    smtp_ctx->server_status->authenticated = true;
                    ret = function_return_exit;
                    smtp_ctx->options.processing = false;
                    cState() = smtp_state_prompt;
                    clearCreds();
#if defined(ENABLE_DEBUG)
                    setDebug("The client is authenticated successfully\n");
#endif
                }
                break;

            default:
                break;
            }
        }

        void setState(smtp_state state, smtp_server_status_code status_code)
        {
            res->begin(smtp_ctx);
            cState() = state;
            smtp_ctx->server_status->state_info.status_code = status_code;
        }

        void sendGreeting(const String &helo, bool esmtp)
        {
#if defined(ENABLE_DEBUG)
            setDebugState(smtp_state_greeting, "Sending greeting...");
#endif
            res->auth_caps[smtp_auth_cap_login] = false;
            if (!tcpSend(true, 2, helo.c_str(), domain.c_str()))
            {
                setError(__func__, TCP_CLIENT_ERROR_SEND_DATA);
                return;
            }
            setState(smtp_state_greeting, smtp_server_status_code_250);
            smtp_ctx->server_status->state_info.esmtp = esmtp;
        }

        bool startTLS()
        {
            if (!serverStatus() || smtp_ctx->server_status->secured)
                return false;
#if defined(ENABLE_DEBUG)
            setDebugState(smtp_state_start_tls, "Starting TLS...");
#endif
            if (!tcpSend(true, 1, "STARTTLS"))
                return setError(__func__, TCP_CLIENT_ERROR_SEND_DATA);

            setState(smtp_state_start_tls, smtp_server_status_code_250);
            return true;
        }

        void tlsHandshake()
        {
            if ((tls_cb || smtp_ctx->options.use_auto_client) && !smtp_ctx->server_status->secured)
            {
#if defined(ENABLE_DEBUG)
                setDebugState(smtp_state_start_tls, "Performing TLS handshake...");
#endif
#if defined(ENABLE_READYCLIENT)
                if (smtp_ctx->auto_client && smtp_ctx->options.use_auto_client)
                    smtp_ctx->server_status->secured = smtp_ctx->auto_client->connectSSL();
                else if (tls_cb)
                    tls_cb(smtp_ctx->server_status->secured);
#else
                if (tls_cb)
                    tls_cb(smtp_ctx->server_status->secured);
#endif

                if (smtp_ctx->server_status->secured)
                {
                    cState() = smtp_state_start_tls_ack;
#if defined(ENABLE_DEBUG)
                    setDebug("TLS handshake done");
#endif
                }
                else
                    setError(__func__, TCP_CLIENT_ERROR_TLS_HANDSHAKE);
            }
        }

        bool isAuthenticated() { return smtp_ctx->server_status->authenticated; }

        bool auth(const String &email, const String &param, bool isToken)
        {
            this->email = email;
            if (isToken)
            {
                clear(password);
                this->access_token = param;
            }
            else
            {
                clear(access_token);
                this->password = param;
            }
            return auth();
        }

        void clearCreds()
        {
            clear(access_token);
            clear(email);
            clear(password);
        }

        bool isInitialized()
        {
            if (!smtp_ctx->client)
                return setError(__func__, TCP_CLIENT_ERROR_INITIALIZE);
            return true;
        }

    public:
        SMTPConnection() {}
    };
}
#endif
#endif