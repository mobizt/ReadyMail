/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef IMAP_CONNECTION_H
#define IMAP_CONNECTION_H
#if defined(ENABLE_IMAP)
#include <Arduino.h>
#include "Common.h"
#include "IMAPBase.h"
#include "IMAPResponse.h"
#include "IMAPSend.h"

using namespace ReadyMailCallbackNS;

namespace ReadyMailIMAP
{

    class IMAPConnection : public IMAPBase
    {
        friend class IMAPClient;
        friend class IMAPSend;

    private:
        void begin(imap_context *imap_ctx, TLSHandshakeCallback cb, IMAPResponse *res)
        {
            this->tls_cb = cb;
            this->res = res;
            beginBase(imap_ctx);
            res->begin(imap_ctx);
        }

        bool connect(const String &host, uint16_t port)
        {
            this->host = host;
            this->port = port;

            if (serverConnected())
                stop(true);

            conn_timer.feed(imap_ctx->options.timeout.con / 1000);
            return connectImpl();
        }

        bool connectImpl()
        {
            if (authenticating)
                return false;

            if (serverConnected())
                stop(true);

            authenticating = true;
            res->begin(imap_ctx);
#if defined(ENABLE_DEBUG)
            setDebugState(imap_state_initial_state, "Connecting to \"" + host + "\" via port " + String(port) + "...");
#endif
            if (!isInitialized())
                return false;

            serverStatus() = imap_ctx->client->connect(host.c_str(), port);
            if (!serverStatus())
            {
                stop(conn_timer.remaining() == 0);
                if (imap_ctx->cb.resp)
                    setError(imap_ctx, __func__, conn_timer.remaining() == 0 ? TCP_CLIENT_ERROR_CONNECTION_TIMEOUT : TCP_CLIENT_ERROR_CONNECTION);
                authenticating = false;
                return false;
            }

            if (!isIdleState("connect"))
                return false;

            setProcessFlag(imap_ctx->options.processing);

            imap_ctx->client->setTimeout(imap_ctx->options.timeout.read);
            authenticating = false;
            setState(imap_state_initial_state);
            return true;
        }

        void storeCredentials(const String &email, const String &param, bool isToken)
        {
            has_credentials = true;
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
        }

        bool auth(const String &email, const String &param, bool isToken)
        {
            storeCredentials(email, param, isToken);
            setProcessFlag(imap_ctx->options.processing);
            return auth();
        }

        bool auth()
        {
            if (!isConnected())
            {
                stop(true);
                return setError(imap_ctx, __func__, TCP_CLIENT_ERROR_NOT_CONNECTED);
            }

            bool user = email.length() > 0 && password.length() > 0;
            bool sasl_auth_oauth = access_token.length() > 0 && imap_ctx->auth_caps[imap_auth_cap_xoauth2];
            bool sasl_login = imap_ctx->auth_caps[imap_auth_cap_login] && user;
            bool sasl_auth_plain = imap_ctx->auth_caps[imap_auth_cap_plain] && user;

            if (sasl_auth_oauth)
            {
                if (!imap_ctx->auth_caps[imap_auth_cap_xoauth2])
                    return setError(imap_ctx, __func__, AUTH_ERROR_OAUTH2_NOT_SUPPORTED);

                if (!tcpSend(true, 7, imap_ctx->tag.c_str(), " ", "AUTHENTICATE", " ", "XOAUTH2", imap_ctx->auth_caps[imap_auth_cap_sasl_ir] ? " " : "", imap_ctx->auth_caps[imap_auth_cap_sasl_ir] ? rd_enc_oauth(email, access_token).c_str() : ""))
                    return setError(imap_ctx, __func__, AUTH_ERROR_AUTHENTICATION);

                setState(imap_state_auth_xoauth2);
            }
            else if (sasl_auth_plain)
            {
                if (!tcpSend(true, 7, imap_ctx->tag.c_str(), " ", "AUTHENTICATE", " ", "PLAIN", imap_ctx->auth_caps[imap_auth_cap_sasl_ir] ? " " : "", imap_ctx->auth_caps[imap_auth_cap_sasl_ir] ? rd_enc_plain(email, password).c_str() : ""))
                    return setError(imap_ctx, __func__, AUTH_ERROR_AUTHENTICATION);
                setState(imap_state_auth_plain);
            }
            else if (sasl_login)
            {
                if (!tcpSend(true, 7, imap_ctx->tag.c_str(), " ", "LOGIN", " ", email.c_str(), " ", password.c_str()))
                    return setError(imap_ctx, __func__, AUTH_ERROR_AUTHENTICATION);
                setState(imap_state_auth_login);
            }
            else
                return setError(imap_ctx, __func__, AUTH_ERROR_AUTHENTICATION);
            return true;
        }

        bool isInitialized()
        {
            if (!imap_ctx->client)
                return setError(imap_ctx, __func__, TCP_CLIENT_ERROR_INITIALIZE);
            return true;
        }

        bool xoauth2SendNext()
        {
            if (!tcpSend(true, 1, rd_enc_oauth(email, access_token).c_str()))
                return setError(imap_ctx, __func__, AUTH_ERROR_AUTHENTICATION);

            setState(imap_state_auth_xoauth2);
            return true;
        }

        bool authPlainSendNext()
        {
            if (!tcpSend(true, 1, rd_enc_plain(email, password).c_str()))
                return setError(imap_ctx, __func__, AUTH_ERROR_AUTHENTICATION);

            setState(imap_state_auth_plain);
            return true;
        }

        bool sendId()
        {
            String buf;
            rd_print_to(buf, 50, " (\"name\" \"ReadyMail\" \"version\" \"%s\")", READYMAIL_VERSION);
            if (!tcpSend(true, 4, imap_ctx->tag.c_str(), " ", "ID", buf.c_str()))
                return setError(imap_ctx, __func__, AUTH_ERROR_AUTHENTICATION);

            setState(imap_state_id);
            return true;
        }

        void setState(imap_state state)
        {
            res->begin(imap_ctx);
            cState() = state;
        }

        imap_function_return_code loop()
        {
            if (cState() != imap_state_prompt)
            {
                if (!serverStatus())
                {
                    connectImpl();
                    if (!isAuthenticated() && has_credentials) // re-authenticate
                        auth();
                }
                else
                {
                    res->handleResponse();
                    authenticate(imap_ctx->server_status->ret);
                }
            }
            return imap_ctx->server_status->ret;
        }

        bool isConnected() { return serverConnected() && imap_ctx->server_status->server_greeting_ack; }

        bool isProcessing() { return imap_ctx->options.processing; }

        bool checkCap()
        {
            tcpSend(true, 3, imap_ctx->tag.c_str(), " ", "CAPABILITY");
            setState(imap_state_greeting);
            return true;
        }

        void authenticate(imap_function_return_code &ret)
        {
            if (ret == function_return_failure && (cState() == imap_state_start_tls || cState() == imap_state_auth_login || cState() == imap_state_login_user || cState() == imap_state_login_psw || cState() == imap_state_auth_xoauth2 || cState() == imap_state_auth_plain || cState() == imap_state_id))
            {
                stop(true);
                return;
            }

            switch (cState())
            {
            case imap_state_initial_state:
                if (ret == function_return_continue && !imap_ctx->server_status->start_tls)
                    tlsHandshake();
                else if (ret == function_return_success)
                    checkCap();
                break;

            case imap_state_greeting:
                if (ret == function_return_failure)
                    exitState(ret, imap_ctx->options.processing);
                else if (ret == function_return_success)
                {
                    imap_ctx->auth_caps[imap_auth_cap_login] = true;
                    cState() = imap_state_prompt;
                    if (imap_ctx->ssl_mode && imap_ctx->auth_caps[imap_auth_cap_starttls] && imap_ctx->server_status->start_tls && (tls_cb || imap_ctx->options.use_auto_client) && !imap_ctx->server_status->secured)
                        startTLS();
                    else
                    {
                        imap_ctx->server_status->server_greeting_ack = true;
                        exitState(ret, imap_ctx->options.processing);
#if defined(ENABLE_DEBUG)
                        setDebugState(imap_state_greeting, "Service is ready\n");
#endif
                    }
                }
                break;

            case imap_state_start_tls:
                if (ret == function_return_success)
                    tlsHandshake();
                break;

            case imap_state_start_tls_ack:
                checkCap();
                break;

            case imap_state_auth_login:
            case imap_state_login_user:
                if (ret == function_return_success)
                    authLogin(cState() == imap_state_auth_login);
                break;

            case imap_state_auth_plain_next:
                authPlainSendNext();
                break;

            case imap_state_auth_xoauth2_next:
                xoauth2SendNext();
                break;

            case imap_state_login_psw:
            case imap_state_auth_xoauth2:
            case imap_state_auth_plain:
                if (ret == function_return_success)
                    sendId();
                break;

            case imap_state_id:
                if (ret == function_return_success)
                {
                    imap_ctx->server_status->authenticated = true;
                    exitState(ret, imap_ctx->options.processing);
                    cState() = imap_state_prompt;
#if defined(ENABLE_DEBUG)
                    setDebugState(imap_state_auth_plain, "The client is authenticated successfully\n");
#endif
                }
                break;

            default:
                break;
            }
        }

        void authLogin(bool user)
        {
            char *enc = rd_b64_enc(rd_cast<const unsigned char *>((user ? email.c_str() : password.c_str())), user ? email.length() : password.length());
            if (enc)
            {
                tcpSend(true, 1, enc);
                rd_free(&enc);
            }
            // expected server challenge response
            setState(user ? imap_state_login_user : imap_state_login_psw);
        }

        bool startTLS()
        {
            if (!serverStatus() || imap_ctx->server_status->secured)
                return false;
#if defined(ENABLE_DEBUG)
            setDebugState(imap_state_start_tls, "Starting TLS...");
#endif
            if (!tcpSend(true, 3, imap_ctx->tag.c_str(), " ", "STARTTLS"))
                return setError(imap_ctx, __func__, TCP_CLIENT_ERROR_STARTTLS);
            setState(imap_state_start_tls);
            return true;
        }

        bool isAuthenticated() { return imap_ctx->server_status->authenticated; }

        void stop(bool forceStop = false) { res->stop(forceStop); }

        void tlsHandshake()
        {
            if ((tls_cb || imap_ctx->options.use_auto_client) && !imap_ctx->server_status->secured)
            {
#if defined(ENABLE_DEBUG)
                setDebugState(imap_state_start_tls, "Performing TLS handshake...");
#endif
#if defined(ENABLE_READYCLIENT)
                if (imap_ctx->auto_client && imap_ctx->options.use_auto_client)
                    imap_ctx->server_status->secured = imap_ctx->auto_client->connectSSL();
                else if (tls_cb)
                    tls_cb(imap_ctx->server_status->secured);
#else
                if (tls_cb)
                    tls_cb(imap_ctx->server_status->secured);
#endif

                if (imap_ctx->server_status->secured)
                {
                    setState(imap_state_start_tls_ack);
#if defined(ENABLE_DEBUG)
                    setDebugState(imap_state_start_tls_ack, "TLS handshake done");
#endif
                }
                else
                {
                    stop(true);
                    setError(imap_ctx, __func__, TCP_CLIENT_ERROR_TLS_HANDSHAKE);
                }
            }
        }

    private:
        IMAPResponse *res = nullptr;
        TLSHandshakeCallback tls_cb = NULL;
        String host, email, password, access_token;
        bool has_credentials = false;
        uint16_t port = 0;
        ReadyTimer conn_timer;
        bool authenticating = false;
    };
}
#endif
#endif