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
    public:
        void begin(imap_context *imap_ctx, TLSHandshakeCallback cb, IMAPResponse *res)
        {
            this->tls_cb = cb;
            this->res = res;
            beginBase(imap_ctx);
        }

        bool connect(const String &host, uint16_t port)
        {
            this->host = host;
            this->port = port;
            if (imap_ctx->client && imap_ctx->client->connected())
                imap_ctx->client->stop();
            return connectImpl();
        }

        bool connectImpl()
        {
            if (!imap_ctx->client || authenticating)
                return false;

            if (host.length() == 0)
                stop();

            if (isConnected())
                return true;

            authenticating = true;
            res->begin(imap_ctx);
            setDebugState(imap_state_initial_state, "Connecting to " + host + "...");
            serverStatus() = imap_ctx->client->connect(host.c_str(), port);
            if (!serverStatus())
            {
                stop();
                if (conn_timer.remaining() == 0 && imap_ctx->cb.resp)
                    setError(imap_ctx, __func__, TCP_CLIENT_ERROR_CONNECTION_TIMEOUT);
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
                stop();
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

        bool isConnected() { return imap_ctx->client && imap_ctx->client->connected() && imap_ctx->server_status->server_greeting_ack; }

        bool checkCap()
        {
            tcpSend(true, 3, imap_ctx->tag.c_str(), " ", "CAPABILITY");
            setState(imap_state_greeting);
            return true;
        }

        void authenticate(imap_function_return_code &ret)
        {
            switch (cState())
            {
            case imap_state_initial_state:
                if (ret == function_return_continue && !imap_ctx->server_status->start_tls)
                    tlsHandshake();
                else if (ret == function_return_success)
                {
                    checkCap();
                }
                break;

            case imap_state_greeting:
                if (ret == function_return_failure)
                {
                    exitState(ret, imap_ctx->options.processing);
                }
                else if (ret == function_return_success)
                {
                    if (imap_ctx->ssl_mode && (imap_ctx->auth_caps[imap_auth_cap_starttls] || imap_ctx->server_status->start_tls) && tls_cb && !imap_ctx->server_status->secured)
                        startTLS();
                    else if (imap_ctx->ssl_mode && (imap_ctx->server_status->start_tls) && tls_cb && !imap_ctx->server_status->secured)
                        startTLS();
                    else
                    {
                        imap_ctx->server_status->server_greeting_ack = true;
                        exitState(ret, imap_ctx->options.processing);
                        setDebugState(imap_state_greeting, "Service is ready\n");
                    }
                }
                break;

            case imap_state_start_tls:
                if (ret == function_return_success)
                    tlsHandshake();
                else if (ret == function_return_failure)
                    stop();
                break;

            case imap_state_start_tls_ack:
                checkCap();
                break;

            case imap_state_auth_login:
                if (ret == function_return_success)
                    authLogin(true);
                else if (ret == function_return_failure)
                    stop();
                break;

            case imap_state_login_user:
                if (ret == function_return_success)
                    authLogin(false);
                else if (ret == function_return_failure)
                    stop();
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
                {
                    sendId();
                }
                else if (ret == function_return_failure)
                    stop();
                break;

            case imap_state_id:
                if (ret == function_return_success)
                {
                    imap_ctx->server_status->authenticated = true;
                    exitState(ret, imap_ctx->options.processing);
                    setDebugState(imap_state_auth_plain, "The client is authenticated successfully\n");
                }
                else if (ret == function_return_failure)
                    stop();
                break;

            default:
                break;
            }
        }

        void authLogin(bool user)
        {
            char *enc = rd_base64_encode((const unsigned char *)(user ? email.c_str() : password.c_str()), user ? email.length() : password.length());
            if (enc)
            {
                tcpSend(true, 1, enc);
                rd_release((void *)enc);
            }
            // expected server challenge response
            setState(user ? imap_state_login_user : imap_state_login_psw);
        }

        void startTLS()
        {
            if (!serverStatus() || imap_ctx->server_status->secured)
                return;
            setDebugState(imap_state_start_tls, "Starting TLS...");
            tcpSend(true, 3, imap_ctx->tag.c_str(), " ", "STARTTLS");
            setState(imap_state_start_tls);
        }

        bool isAuthenticated() { return imap_ctx->server_status->authenticated; }

        void stop() { res->stop(); }

        void tlsHandshake()
        {
            if (tls_cb && !imap_ctx->server_status->secured)
            {
                setDebugState(imap_state_start_tls, "Performing TLS handshake...");
                tls_cb(imap_ctx->server_status->secured);
                if (imap_ctx->server_status->secured)
                    setDebugState(imap_state_start_tls_ack, "TLS handshake done");
                else
                {
                    stop();
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