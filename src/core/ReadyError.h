/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef READY_ERROR_H
#define READY_ERROR_H
#include <Arduino.h>

#if defined(ENABLE_IMAP) || defined(ENABLE_SMTP)
#if defined(ENABLE_DEBUG) || defined(ENABLE_CORE_DEBUG)
static String rd_err(int code)
{
    String msg;
    switch (code)
    {
    case TCP_CLIENT_ERROR_INITIALIZE:
        msg = "Client is not yet initialized";
        break;
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
    return msg;
}
#endif
#endif
#endif