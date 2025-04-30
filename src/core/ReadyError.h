#ifndef READYMAIL_ERR_H
#define READYMAIL_ERR_H
#include <Arduino.h>

static String rd_err(int code)
{
    String msg;
#if defined(ENABLE_DEBUG) || defined(READYMAIL_CORE_DEBUG)
    switch (code)
    {
    case TCP_CLIENT_ERROR_CONNECTION:
        msg = "Connection failed";
        break;
    case TCP_CLIENT_ERROR_NOT_CONNECTED:
        msg = "Server was not yet connected";
        break;
    case TCP_CLIENT_ERROR_CONNECTION_TIMEOUT:
        msg = "Connection timed out, see http://bit.ly/437GkRA";
        break;
    case TCP_CLIENT_ERROR_TLS_HANDSHAKE:
        msg = "TLS handshake failed";
        break;
    case TCP_CLIENT_ERROR_SEND_DATA:
        msg = "Send data failed";
        break;
    case TCP_CLIENT_ERROR_READ_DATA:
        msg = "Read data failed";
        break;
    case AUTH_ERROR_UNAUTHENTICATE:
        msg = "Unauthented";
        break;
    case AUTH_ERROR_AUTHENTICATION:
        msg = "Authentication error";
        break;
    case AUTH_ERROR_OAUTH2_NOT_SUPPORTED:
        msg = "OAuth2.0 authentication was not supported";
        break;
    default:
        break;
    }
#endif
    return msg;
}
#endif