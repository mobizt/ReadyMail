#ifndef SMTP_CLIENT_H
#define SMTP_CLIENT_H
#if defined(ENABLE_SMTP)
#include <Arduino.h>
#include "Common.h"
#include "SMTPMessage.h"
#include "SMTPConnection.h"
#include "SMTPSend.h"

using namespace ReadyMailSMTP;
using namespace ReadyMailNS;

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
        smtp_response_status_t status;
        smtp_context smtp_ctx;
        SMTPMessage amsg;

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
            amsg.clear();
            return code != function_return_failure;
        }

        bool authImpl(const String &email, const String &param, readymail_auth_type auth, bool await = true)
        {
            if (auth == readymail_auth_disabled)
            {
                smtp_ctx.server_status->authenticated = false;
                return true;
            }

            if (smtp_ctx.processing)
                return true;

            conn.setDebugState(smtp_state_authentication, "Authenticating...");

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
        SMTPClient(Client &client, TLSHandshakeCallback tlsCallback = NULL, bool startTLS = false)
        {
            server_status.start_tls = startTLS;
            smtp_ctx.client = &client;
            smtp_ctx.server_status = &server_status;
            smtp_ctx.status = &status;
            conn.begin(&smtp_ctx, tlsCallback, &res);
            sender.begin(&smtp_ctx, &res, &conn);
        }
        bool connect(const String &host, uint16_t port, const String &domain, SMTPResponseCallback responseCallback = NULL, bool ssl = true, bool await = true)
        {
            smtp_ctx.resp_cb = responseCallback;
            bool ret = conn.connect(host, port, domain, ssl);
            if (ret && await)
                return awaitLoop();
            return ret;
        }
        bool isAuthenticated() { return conn.isAuthenticated(); }
        bool authenticate(const String &email, const String &param, readymail_auth_type auth, bool await = true) { return authImpl(email, param, auth, await); }
        bool isConnected() { return conn.isConnected(); }
        bool send(SMTPMessage &message, bool await = true)
        {
            amsg.clear();
            if (!await)
                amsg = message;
            bool ret = sender.send(await ? message : amsg);
            if (ret && await)
                return awaitLoop();
            return ret;
        }
        void loop()
        {
            conn.loop();
            sender.loop();
        }
        void stop()
        {
            if (isConnected() && isAuthenticated())
                logout(true);
            conn.stop();
        }
        bool logout(bool await = true)
        {
            bool ret = sender.sendQuit();
            if (ret && await)
                return awaitLoop();
            return ret;
        }

        bool isComplete() { return smtp_ctx.status->complete; }

        smtp_state currentState() { return sender.cState(); }

        uint32_t contextAddr() { return reinterpret_cast<uint32_t>(&smtp_ctx); }
    };

}
#endif
#endif