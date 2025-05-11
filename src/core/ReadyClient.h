#pragma once

#ifndef READY_CLIENT_H
#define READY_CLIENT_H

#include <Arduino.h>
#if defined(ENABLE_READYCLIENT) && defined(READYCLIENT_SSL_CLIENT)

class ReadyClient
{
private:
    std::vector<readymail_port_function> ports;
    READYCLIENT_SSL_CLIENT *ssl_client = nullptr;

public:
    /** ReadyClient class constructor.
     *
     * @param ssl_client The SSL client to use with ReadyClient. Currently supports ESP_SSLClient and ESP32 v3.x NeteorkClientSecure
     */
    ReadyClient(READYCLIENT_SSL_CLIENT &ssl_client) { this->ssl_client = &ssl_client; }

    /** Add port and protocol.
     *
     * @param port The server port.
     * @param proto The readymail_protocol enums e.g. readymail_protocol_plain_text, readymail_protocol_ssl and readymail_protocol_tls.
     * The readymail_protocol_tls is the STARTTLS protocols
     */
    void addPort(uint16_t port, readymail_protocol proto)
    {
        readymail_port_function port_func;
        port_func.port = port;
        port_func.protocol = proto;
        ports.push_back(port_func);
    }
    /** Clear all ports.
     *
     */
    void clearPorts() { ports.clear(); }
    ~ReadyClient() {}

    // Internal used functions.
    READYCLIENT_SSL_CLIENT &getClient() { return *ssl_client; }

    // Internal used functions.
    void configPort(uint16_t port, bool &ssl, bool &startTLS)
    {
        for (size_t i = 0; i < ports.size(); i++)
        {
            if (port == ports[i].port)
            {
                ssl = !ports[i].protocol == readymail_protocol_plain_text ? true : false;
#if defined(READYCLIENT_TYPE_1)
                ssl_client->enableSSL(ports[i].protocol == readymail_protocol_ssl);
#elif defined(READYCLIENT_TYPE_2)
                if (ports[i].protocol == readymail_protocol_plain_text || ports[i].protocol == readymail_protocol_tls)
                    ssl_client->setPlainStart();
#endif
                if (ports[i].protocol == readymail_protocol_tls)
                    startTLS = true;
                else
                    startTLS = false;
            }
        }
    }

    // Internal used functions.
    bool connectSSL()
    {
#if defined(READYCLIENT_TYPE_1)
        return ssl_client->connectSSL();
#elif defined(READYCLIENT_TYPE_2)
        return ssl_client->startTLS();
#endif
        return false;
    }
};

#endif
#endif