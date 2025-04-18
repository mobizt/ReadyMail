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
            if (!smtp_ctx->client || authenticating)
                return false;

            if (isConnected())
                return true;

            authenticating = true;
            res->begin(smtp_ctx);
            setDebugState(smtp_state_initial_state, "Connecting to " + host + "...");
            smtp_ctx->processing = true;
            serverStatus() = smtp_ctx->client->connect(host.c_str(), port);
            if (!serverStatus())
            {
                if (conn_timer.remaining() == 0 && smtp_ctx->resp_cb)
                    setError(__func__, TCP_CLIENT_ERROR_CONNECTION_TIMEOUT);
                authenticating = false;
                return false;
            }
            smtp_ctx->client->setTimeout(smtp_ctx->read_timeout_ms);
            authenticating = false;
            setState(smtp_state_initial_state, smtp_server_status_code_220);
            return true;
        }

        bool auth()
        {
            if (!isConnected())
                return setError(__func__, TCP_CLIENT_ERROR_NOT_CONNECTED);

            smtp_ctx->processing = true;
            bool user = email.length() > 0 && password.length() > 0;
            bool sasl_auth_oauth = access_token.length() > 0 && res->auth_caps[smtp_auth_cap_xoauth2];
            bool sasl_login = res->auth_caps[smtp_auth_cap_login] && user;
            bool sasl_auth_plain = res->auth_caps[smtp_auth_cap_plain] && user;

            if (sasl_auth_oauth)
            {
                if (!res->auth_caps[smtp_auth_cap_xoauth2])
                    return setError(__func__, AUTH_ERROR_OAUTH2_NOT_SUPPORTED);

                if (!tcpSend(true, 3, "AUTH ", "XOAUTH2 ", rd_enc_oauth(email, access_token).c_str()))
                    return setError(__func__, AUTH_ERROR_AUTHENTICATION);

                setState(smtp_state_auth_xoauth2, smtp_server_status_code_235);
            }
            else if (sasl_auth_plain)
            {
                if (!tcpSend(true, 3, "AUTH ", "PLAIN ", rd_enc_plain(email, password).c_str()))
                    return setError(__func__, AUTH_ERROR_AUTHENTICATION);
                setState(smtp_state_auth_plain, smtp_server_status_code_235);
            }
            else if (sasl_login)
            {
                if (!tcpSend(true, 2, "AUTH ", "LOGIN"))
                    return setError(__func__, AUTH_ERROR_AUTHENTICATION);
                setState(smtp_state_auth_login, smtp_server_status_code_334);
            }
            else
                return setError(__func__, AUTH_ERROR_AUTHENTICATION);
            return true;
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
            setState(user ? smtp_state_login_user : smtp_state_login_psw, user ? smtp_server_status_code_334 : smtp_server_status_code_235);
        }

        void stop()
        {
            res->stop();
            clearCreds();
        }

        void begin(smtp_context *smtp_ctx, TLSHandshakeCallback tlsCallback, SMTPResponse *res)
        {
            this->tls_cb = tlsCallback;
            this->res = res;
            beginBase(smtp_ctx);
        }

        bool connect(const String &host, uint16_t port, const String &domain, bool ssl = true)
        {
            this->host = host;
            this->port = port;
            smtp_ctx->ssl_mode = ssl;

            if (smtp_ctx->client)
                smtp_ctx->client->stop();

            if (domain.length())
                this->domain = domain;
            conn_timer.feed(smtp_ctx->read_timeout_ms / 1000);
            return connectImpl();
        }

        bool isConnected() { return smtp_ctx->client && smtp_ctx->client->connected() && smtp_ctx->server_status->server_greeting_ack; }

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
                    if (smtp_ctx->ssl_mode && (res->auth_caps[smtp_auth_cap_starttls] || smtp_ctx->server_status->start_tls) && tls_cb && !smtp_ctx->server_status->secured)
                        startTLS();
                    else
                    {
                        smtp_ctx->server_status->server_greeting_ack = true;
                        ret = function_return_exit;
                        smtp_ctx->processing = false;
                        setDebug("Service is ready\n");
                    }
                }
                break;

            case smtp_state_start_tls:
                if (ret == function_return_success)
                    tlsHandshake();
                else if (ret == function_return_failure)
                    stop();
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
                    smtp_ctx->processing = false;
                }
                break;

            case smtp_state_auth_login:
                if (ret == function_return_success)
                    authLogin(true);
                else if (ret == function_return_failure)
                    stop();
                break;

            case smtp_state_login_user:
                if (ret == function_return_success)
                    authLogin(false);
                else if (ret == function_return_failure)
                    stop();
                break;

            case smtp_state_login_psw:
            case smtp_state_auth_xoauth2:
            case smtp_state_auth_plain:
                if (ret == function_return_success)
                {
                    smtp_ctx->server_status->authenticated = true;
                    ret = function_return_exit;
                    smtp_ctx->processing = false;
                    clearCreds();
                    setDebug("The client is authenticated successfully\n");
                }
                else if (ret == function_return_failure)
                    stop();
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
            setDebugState(smtp_state_greeting, "Sending greeting...");
            res->auth_caps[smtp_auth_cap_login] = false;
            tcpSend(true, 2, helo.c_str(), domain.c_str());
            setState(smtp_state_greeting, smtp_server_status_code_250);
            smtp_ctx->server_status->state_info.esmtp = esmtp;
        }
        void startTLS()
        {
            if (!serverStatus() || smtp_ctx->server_status->secured)
                return;
            setDebugState(smtp_state_start_tls, "Starting TLS...");
            tcpSend(true, 1, "STARTTLS");
            setState(smtp_state_start_tls, smtp_server_status_code_250);
        }
        void tlsHandshake()
        {
            if (tls_cb && !smtp_ctx->server_status->secured)
            {
                setDebugState(smtp_state_start_tls, "Performing TLS handshake...");
                tls_cb(smtp_ctx->server_status->secured);
                if (smtp_ctx->server_status->secured)
                {
                    cState() = smtp_state_start_tls_ack;
                    setDebug("TLS handshake done");
                }
                else
                    setError(__func__, TCP_CLIENT_ERROR_TLS_HANDSHAKE);
            }
        }
        bool isAuthenticated() { return smtp_ctx->server_status->authenticated; }
        bool auth(const String &email, const String &param, bool IsToken)
        {
            this->email = email;
            if (IsToken)
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

    public:
        SMTPConnection() {}
    };
}
#endif
#endif