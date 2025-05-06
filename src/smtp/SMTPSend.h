#ifndef SMTP_SEND_H
#define SMTP_SEND_H
#if defined(ENABLE_SMTP)
#include <Arduino.h>
#include "Common.h"
#include "SMTPBase.h"
#include "SMTPMessage.h"
#include "SMTPConnection.h"
#include "SMTPResponse.h"

namespace ReadyMailSMTP
{
    class SMTPSend : public SMTPBase
    {
        friend class SMTPClient;

    private:
        SMTPConnection *conn = nullptr;
        SMTPResponse *res = nullptr;
        SMTPMessage *msg_ptr = nullptr;
        uint32_t root_msg_addr = 0;

        void begin(smtp_context *smtp_ctx, SMTPResponse *res, SMTPConnection *conn)
        {
            this->conn = conn;
            this->res = res;
            beginBase(smtp_ctx);
        }

        bool checkEmail(SMTPMessage &msg)
        {
            bool sender = false, recp = false;
            for (size_t i = 0; i < msg.headers.size(); i++)
            {
                if (msg.headers[i].type == rfc822_from || msg.headers[i].type == rfc822_sender)
                    sender = true;
                else if (msg.headers[i].type >= rfc822_to || msg.headers[i].type <= rfc822_bcc)
                    recp = true;

                if ((msg.headers[i].type >= rfc822_from && msg.headers[i].type <= rfc822_bcc) && !isValidEmail(msg.headers[i].value.c_str()))
                    return setError(__func__, msg.headers[i].type == rfc822_from || msg.headers[i].type == rfc822_sender ? SMTP_ERROR_INVALID_SENDER_EMAIL : SMTP_ERROR_INVALID_RECIPIENT_EMAIL, "", false);
            }

            if (!sender || !recp)
                return setError(__func__, !sender ? SMTP_ERROR_INVALID_SENDER_EMAIL : SMTP_ERROR_INVALID_RECIPIENT_EMAIL, "", false);

            return true;
        }

        bool send(SMTPMessage &msg, const String &notify)
        {
            if (!smtp_ctx->options.accumulate && !smtp_ctx->options.imap_mode)
            {
                setDebugState(smtp_state_send_header_sender, "Sending E-mail...");

                smtp_ctx->options.notify = notify;

                if (conn && !conn->isConnected())
                    return setError(__func__, TCP_CLIENT_ERROR_NOT_CONNECTED, "", false);

                if (!isIdleState("send"))
                    return false;

                if (!checkEmail(msg))
                    return false;
            }
            return sendImpl(msg);
        }

        bool sendImpl(SMTPMessage &msg)
        {
            smtp_ctx->options.processing = true;
            msg_ptr = &msg;
            root_msg_addr = reinterpret_cast<uint32_t>(&msg);
            msg.recipient_index = 0;
            msg.cc_index = 0;
            msg.bcc_index = 0;
            msg.send_recipient_complete = false;
            smtp_ctx->status->isComplete = false;
            clear(msg.buf);
            clear(msg.header);
            setXEnc(msg);

            for (size_t i = 0; i < msg.headers.size(); i++)
            {
                if ((msg.headers[i].type == rfc822_from || msg.headers[i].type == rfc822_sender))
                    rd_print_to(msg.header, 250, "%s: %s<%s>\r\n", rfc822_headers[msg.headers[i].type].text, msg.headers[i].name.c_str(), msg.headers[i].value.c_str());
            }

            return startTransaction(msg);
        }

        bool sendCmd(const String &cmd)
        {
            setDebugState(smtp_state_send_command, "Sending command...");
            setDebugState(smtp_state_send_command, cmd);
            smtp_ctx->cmd_ctx.resp.text.remove(0, smtp_ctx->cmd_ctx.resp.text.length());
            smtp_ctx->cmd_ctx.cmd = cmd;
            setState(smtp_state_send_command, smtp_server_status_code_220);
            return sendBuffer(cmd + "\r\n");
        }

        bool sendData(const String &data)
        {
            setDebugState(smtp_state_send_data, "Sending data...");
            setDebugState(smtp_state_send_data, data);
            setState(smtp_state_send_data, smtp_server_status_code_0);
            return sendBuffer(data);
        }

        size_t headerSize(SMTPMessage &msg, rfc822_header_types type)
        {
            int count = 0;
            for (size_t i = 0; i < msg.headers.size(); i++)
            {
                if (msg.headers[i].type == type)
                    count++;
            }
            return count;
        }

        smtp_header_item getHeader(SMTPMessage &msg, rfc822_header_types type, int index)
        {
            smtp_header_item hdr;
            int count = 0;
            for (size_t i = 0; i < msg.headers.size(); i++)
            {
                if (msg.headers[i].type == type)
                {
                    if (index == count)
                        return msg.headers[i];
                    count++;
                }
            }
            return hdr;
        }

        bool startTransaction(SMTPMessage &msg)
        {
            if (!smtp_ctx->options.accumulate && !smtp_ctx->options.imap_mode)
            {
                setDebugState(smtp_state_send_header_sender, "Sending envelope...");

                String sender;
                for (size_t i = 0; i < msg.headers.size(); i++)
                {
                    if ((msg.headers[i].type == rfc822_from || msg.headers[i].type == rfc822_sender))
                    {
                        sender = msg.headers[i].value;
                        break;
                    }
                }

                rd_print_to(msg.buf, 250, "MAIL FROM:<%s>", sender.c_str());
                if ((msg.text.xenc == xenc_binary || msg.html.xenc == xenc_binary) && res->feature_caps[smtp_send_cap_binary_mime])
                    msg.buf += " BODY=BINARYMIME";
                else if ((msg.text.xenc == xenc_8bit || msg.html.xenc == xenc_8bit) && res->feature_caps[smtp_send_cap_8bit_mime])
                    msg.buf += " BODY=8BITMIME";
                msg.buf += "\r\n";

                if (!sendBuffer(msg.buf))
                    return false;
            }

            setState(smtp_state_send_header_sender, smtp_server_status_code_250);
            return true;
        }

        bool sendRecipient(SMTPMessage &msg)
        {
            // Construct 'To' header fields.
            String email, name;
            bool is_recipient = false, is_cc_bcc = false;
            int i = 0;

            int to_size = headerSize(msg, rfc822_to);
            int cc_size = headerSize(msg, rfc822_cc);
            int bcc_size = headerSize(msg, rfc822_bcc);

            if (msg.recipient_index < to_size)
            {
                i = msg.recipient_index;
                smtp_header_item hdr = getHeader(msg, rfc822_to, msg.recipient_index);
                email = hdr.value;
                name = hdr.name;
                msg.recipient_index++;
                is_recipient = true;
                rd_print_to(msg.header, 250, "%s%s<%s>%s", i == 0 ? "To: " : ",", name.c_str(), email.c_str(), i == to_size - 1 ? "\r\n" : "");
            }
            else if (msg.cc_index < cc_size)
            {
                i = msg.cc_index;
                smtp_header_item hdr = getHeader(msg, rfc822_cc, msg.cc_index);
                email = hdr.value;
                name = hdr.name;
                msg.cc_index++;
                is_cc_bcc = true;
                rd_print_to(msg.header, 250, "%s<%s>%s", i == 0 ? "Cc: " : ",", email.c_str(), i == cc_size - 1 ? "\r\n" : "");
            }
            else if (msg.bcc_index < bcc_size)
            {
                smtp_header_item hdr = getHeader(msg, rfc822_bcc, msg.bcc_index);
                email = hdr.value;
                name = hdr.name;
                msg.bcc_index++;
                is_cc_bcc = true;
            }

            if (smtp_ctx->options.accumulate || smtp_ctx->options.imap_mode)
            {
                if (msg_ptr)
                    startData(*msg_ptr);
                return true;
            }

            if (is_recipient || is_cc_bcc)
            {
                // only address
                clear(msg.buf);
                rd_print_to(msg.buf, 250, "RCPT TO:<%s>", email.c_str());

                // rfc3461, rfc3464
                if (is_recipient && res->feature_caps[smtp_send_cap_dsn] && (smtp_ctx->options.notify.indexOf("SUCCESS") > -1 || smtp_ctx->options.notify.indexOf("FAILURE") > -1 || smtp_ctx->options.notify.indexOf("DELAY") > -1))
                    msg.buf += " NOTIFY=" + smtp_ctx->options.notify;

                msg.buf += "\r\n";

                if (!sendBuffer(msg.buf))
                    return false;

                if (msg.recipient_index == to_size && msg.cc_index == cc_size && msg.bcc_index == bcc_size)
                    msg.send_recipient_complete = true;

                setState(smtp_state_send_header_recipient, smtp_server_status_code_250);
                return true;
            }
            return true;
        }

        bool sendBodyData(SMTPMessage &msg)
        {
            switch (msg.send_state)
            {
            case smtp_send_state_body_data:

                setDebugState(smtp_state_send_header_recipient, "Sending headers...");
                if (!sendBuffer(msg.header))
                    return false;

                setState(smtp_state_send_body, smtp_server_status_code_0);
                msg.send_state++;
                break;

            case smtp_send_state_body_header:
                return sendHeader(msg);
                break;

            case smtp_send_state_message_data:
                setMIMEList(msg);
                return initSendState(msg);
                break;

            case smtp_send_state_body_type_1:
            case smtp_send_state_body_type_5_1:
            case smtp_send_state_body_type_5P_1:
                return sendTextMessage(msg, 0 /* text or html */, msg.html.content.length() > 0, false);
                break;

            case smtp_send_state_body_type_2:  // mixed
            case smtp_send_state_body_type_2P: // mixed
            case smtp_send_state_body_type_3:  // mixed
            case smtp_send_state_body_type_3P: // mixed
            case smtp_send_state_body_type_4:  // mixed
            case smtp_send_state_body_type_4P: // mixed
            case smtp_send_state_body_type_5:  // mixed
            case smtp_send_state_body_type_5P: // mixed
            case smtp_send_state_body_type_6:  // alternative
            case smtp_send_state_body_type_7:  // related
            case smtp_send_state_body_type_8:  // alternative
                sendMultipartHeader(msg, -1, 0);
                break;

            case smtp_send_state_body_type_2_1:  // mixed, alternative
            case smtp_send_state_body_type_2P_1: // mixed, alternative
            case smtp_send_state_body_type_3_1:  // mixed, related
            case smtp_send_state_body_type_3P_1: // mixed, related
            case smtp_send_state_body_type_4_1:  // mixed, alternative
            case smtp_send_state_body_type_4P_1: // mixed, alternative
            case smtp_send_state_body_type_6_2:  // alternative, related
                sendMultipartHeader(msg, 0, 1);
                break;

            case smtp_send_state_body_type_2_2:
            case smtp_send_state_body_type_2P_2:
            case smtp_send_state_body_type_4_2:
            case smtp_send_state_body_type_4P_2:
                return sendTextMessage(msg, 1 /* alternative */, false, false);
                break;

            case smtp_send_state_body_type_2_3:
            case smtp_send_state_body_type_2P_3:
                sendMultipartHeader(msg, 1, 2); // alternative, related
                break;

            case smtp_send_state_body_type_2_4:
            case smtp_send_state_body_type_2P_4:
                return sendTextMessage(msg, 2 /* related */, true, false);
                break;

            case smtp_send_state_body_type_2_5:
            case smtp_send_state_body_type_2P_5:

                return sendAttachment(msg, attach_type_inline, 2 /* related */, true /* close alternative */);
                break;

            case smtp_send_state_body_type_2_6:
            case smtp_send_state_body_type_3_4:
            case smtp_send_state_body_type_4_4:
            case smtp_send_state_body_type_5_2:
            case smtp_send_state_body_type_2P_8:
            case smtp_send_state_body_type_3P_6:
            case smtp_send_state_body_type_4P_6:
            case smtp_send_state_body_type_5P_4:

                if (msg.rfc822.size() && msg.rfc822_idx < (int)msg.rfc822.size())
                    return sendRFC822Message(msg);
                else if (msg.attachments.exists(attach_type_attachment))
                    return sendAttachment(msg, attach_type_attachment, 0 /* mixed */, false);
                else
                {
                    if (!sendEndBoundary(msg, 0))
                        return false;
                    return terminateData(msg);
                }
                break;

            case smtp_send_state_body_type_2P_6:
                sendMultipartHeader(msg, 0, 3); // mixed, parallel
                break;

            case smtp_send_state_body_type_2P_7:
                return sendAttachment(msg, attach_type_parallel, 3 /* parallel */, false);
                break;

            case smtp_send_state_body_type_3_2:
            case smtp_send_state_body_type_3P_2:
            case smtp_send_state_body_type_6_3:
                return sendTextMessage(msg, 1 /* related */, true, false);
                break;

            case smtp_send_state_body_type_3_3:
            case smtp_send_state_body_type_3P_3:
                return sendAttachment(msg, attach_type_inline, 1 /* related */, false);
                break;

            case smtp_send_state_body_type_3P_4:
            case smtp_send_state_body_type_4P_4:
            case smtp_send_state_body_type_5P_2:
                sendMultipartHeader(msg, 0, 2); // mixed, parallel
                break;

            case smtp_send_state_body_type_3P_5:
            case smtp_send_state_body_type_4P_5:
            case smtp_send_state_body_type_5P_3:
                return sendAttachment(msg, attach_type_parallel, 2 /* parallel */, false);
                break;

            case smtp_send_state_body_type_4_3:
            case smtp_send_state_body_type_4P_3:
                return sendTextMessage(msg, 1 /* alternative */, true, true);
                break;

            case smtp_send_state_body_type_6_1:
            case smtp_send_state_body_type_8_1:
                return sendTextMessage(msg, 0 /* alternative */, false, false);
                break;

            case smtp_send_state_body_type_6_4:
                return sendAttachment(msg, attach_type_inline, 1 /* related */, true);
                break;

            case smtp_send_state_body_type_7_1:
                return sendTextMessage(msg, 0 /* related */, true, false);
                break;

            case smtp_send_state_body_type_7_2:
                return sendAttachment(msg, attach_type_inline, 0 /* related */, true);
                break;

            case smtp_send_state_body_type_8_2:
                return sendTextMessage(msg, 0 /* alternative */, true, true);
                break;

            default:
                break;
            }
            return true;
        }

        bool sendRFC822Header(SMTPMessage &msg)
        {
            String buf;
            rd_print_to(buf, 250, "\r\n--%s\r\nContent-Type: message/rfc822; Name=\"%s\"\r\nContent-Disposition: attachment; filename=\"%s\";\r\n\r\n", msg.parent->content_types[0].boundary.c_str(), msg.rfc822_name.c_str(), msg.rfc822_filename);
            return sendBuffer(buf);
        }

        bool sendRFC822Message(SMTPMessage &msg)
        {
            msg_ptr = &msg.rfc822[msg.rfc822_idx];
            if (!sendRFC822Header(*msg_ptr))
                return false;

            setSendState(*msg_ptr, smtp_send_state_body_header);
            setState(smtp_state_send_body, smtp_server_status_code_0);
            return true;
        }

        bool startData(SMTPMessage &msg)
        {
            if (!smtp_ctx->options.accumulate && !smtp_ctx->options.imap_mode)
            {
                if (!sendBuffer("DATA\r\n"))
                    return false;
            }
            setState(smtp_state_wait_data, smtp_server_status_code_354);
            return true;
        }

        bool sendMultipartHeader(SMTPMessage &msg, int parent, int current)
        {
            if (!sendMultipartHeaderMIME(msg, parent, current))
                return false;
            msg.send_state++;
            return true;
        }

        bool sendEndBoundary(SMTPMessage &msg, int content_type_index)
        {
            String buf;
            rd_print_to(buf, 250, "\r\n--%s--%s", msg.content_types[content_type_index].boundary.c_str(), content_type_index == 0 && !msg.parent ? "" : "\r\n");
            return sendBuffer(buf);
        }

        bool sendBuffer(const String &buf) { return tcpSend(false, 1, buf.c_str()); }

        bool isDateSet(SMTPMessage &msg)
        {
            // Check Date header
            bool valid = false;
            smtp_ctx->ts = msg.timestamp;
            for (uint8_t k = 0; k < msg.headers.size(); k++)
            {
                if (msg.headers[k].name == "Date")
                {
                    smtp_ctx->ts = getTimestamp(msg.headers[k].value, true);
                    valid = smtp_ctx->ts > READYMAIL_TIMESTAMP;
                    break;
                }
            }

            // Set the timestamp
            if (!valid)
            {
#if defined(ESP32) || defined(ESP8266)
                if (smtp_ctx->ts < READYMAIL_TIMESTAMP)
                    smtp_ctx->ts = time(nullptr);
#endif
                // Apply default value if the timestamp is not valid.
                if (smtp_ctx->ts < READYMAIL_TIMESTAMP)
                    smtp_ctx->ts = READYMAIL_TIMESTAMP;
            }

            return valid;
        }

        bool sendHeader(SMTPMessage &msg)
        {
            String buf;
            buf.reserve(1024);

            for (uint8_t k = 0; k < msg.headers.size(); k++)
            {
                if (msg.headers[k].type < rfc822_from || msg.headers[k].type > rfc822_bcc)
                    rd_print_to(buf, msg.headers[k].name.length() + msg.headers[k].value.length(), "%s: %s\r\n", msg.headers[k].name.c_str(), msg.headers[k].value.c_str());
            }

            if (!isDateSet(msg)) // If Date header does not set, set it from timestamp
                rd_print_to(buf, 250, "%s: %s\r\n", rfc822_headers[rfc822_date].text, getDateTimeString(smtp_ctx->ts, "%a, %d %b %Y %H:%M:%S %z").c_str());

            buf += "MIME-Version: 1.0\r\n";

            if (!sendBuffer(buf))
                return false;

            setState(smtp_state_send_body, smtp_server_status_code_0);
            msg.send_state++;
            return true;
        }

        String getRandomUID()
        {
            char buf[36];
            sprintf(buf, "%d", (int)random(2000, 4000));
            return buf;
        }

        bool sendTextMessage(SMTPMessage &msg, int content_type_index, bool html, bool close_boundary)
        {
            msg.text.transfer_encoding.toLowerCase();
            msg.html.transfer_encoding.toLowerCase();

            if (html && msg.html.content.length() == 0)
            {
                msg.html.data_size = -1;
                msg.html.data_index = -1;
            }
            else if (!html && msg.text.content.length() == 0)
            {
                msg.text.data_size = -1;
                msg.text.data_index = -1;
            }

            if ((html && msg.html.data_size == 0) || (!html && msg.text.data_size == 0))
            {
                setDebugState(smtp_state_send_body, "Sending text/" + String((html ? "html" : "plain")) + " body...");
                String buf, ct_prop;
                getTextContent(msg, html);
                bool embed = (html && msg.html.embed.enable) || (!html && msg.text.embed.enable);
                bool embed_inline = embed && ((html && msg.html.embed.type == embed_message_type_inline) || (!html && msg.text.embed.type == embed_message_type_inline));
                rd_print_to(ct_prop, 250, "; charset=\"%s\";%s%s", html ? msg.html.charSet.c_str() : msg.text.charSet.c_str(), html ? "" : (msg.text.flowed ? " format=\"flowed\"; delsp=\"no\";" : ""), msg.html.embed.enable ? (html ? " Name=\"msg.html\";" : " Name=\"msg.txt\";") : "");
                setContentTypeHeader(buf, msg.content_types[content_type_index].boundary, html ? msg.html.content_type.c_str() : msg.text.content_type.c_str(), ct_prop, html ? msg.html.transfer_encoding : msg.text.transfer_encoding, embed ? (embed_inline ? "inline" : "attachment") : "", html ? msg.html.embed.filename : msg.text.embed.filename, 0, html ? msg.html.embed.filename : msg.text.embed.filename, embed_inline ? getRandomUID() : "");

                if (!sendBuffer(buf))
                    return false;
            }
            else if ((html && msg.html.data_index < msg.html.data_size) || (!html && msg.text.data_index < msg.text.data_size))
            {
                if (!sendChunk(html ? msg.html.enc_text : msg.text.enc_text, html ? msg.html.data_index : msg.text.data_index, html ? msg.html.data_size : msg.text.data_size, html ? msg.html.transfer_encoding : msg.text.transfer_encoding))
                    return false;
            }

            if ((html && msg.html.data_index == msg.html.data_size) || (!html && msg.text.data_index == msg.text.data_size))
            {
                if (close_boundary && !sendEndBoundary(msg, content_type_index))
                    return false;

                msg.send_state++;
                if (content_type_index == 0 && msg.content_types[content_type_index].mime != "mixed" && msg.content_types[content_type_index].mime != "related" && (msg.content_types[content_type_index].mime != "alternative" || (html && msg.content_types[content_type_index].mime == "alternative")))
                    return terminateData(msg);
            }
            return true;
        }

        bool sendChunk(const String &buf, int &index, int &len, const String &enc)
        {
            int remaining = len - index;
            int chunkSize = enc.indexOf("base64") > -1 ? BASE64_CHUNKED_LEN : 128;
            int toSend = remaining > chunkSize ? chunkSize : remaining;
            remaining -= toSend;

            String sendBuf = buf.substring(index, index + toSend);
            if (remaining == 0 || chunkSize == BASE64_CHUNKED_LEN)
                sendBuf += "\r\n";

            if (!sendBuffer(sendBuf))
                return false;

            index += toSend;
            return true;
        }

        void getTextContent(SMTPMessage &msg, bool html)
        {
            String buf = html ? msg.html.content : msg.text.content;
            if (html)
            {
                for (uint8_t i = 0; i < msg.attachments.size(); i++)
                {
                    if (msg.attachments[i].type == attach_type_inline)
                    {
                        String fnd, rep;
                        rd_print_to(fnd, 250, "\"%s\"", msg.attachments[i].filename.substring(msg.attachments[i].filename.lastIndexOf("/") > -1 ? msg.attachments[i].filename.lastIndexOf("/") + 1 : 0).c_str());
                        rd_print_to(rep, 250, "cid:\"%s\"", msg.attachments[i].content_id.length() > 0 ? msg.attachments[i].content_id.c_str() : msg.attachments[i].cid.c_str());
                        buf.replace(fnd, rep);
                    }
                }
            }
            else if (msg.text.flowed)
                formatFlowedText(buf);

            rd_encode_qb(html ? msg.html.enc_text : msg.text.enc_text, buf, html ? msg.html.transfer_encoding : msg.text.transfer_encoding);
            clear(buf);

            if (html)
                msg.html.data_size = msg.html.enc_text.length();
            else
                msg.text.data_size = msg.text.enc_text.length();
        }

        void formatFlowedText(String &content)
        {
            int count = 0;
            String qms;
            int size = split(content, "\r\n", nullptr, 0);
            String *buf = new String[size];
            size = split(content, "\r\n", buf, size);
            clear(content);
            for (int i = 0; i < size; i++)
            {
                if (buf[i].length() > 0)
                {
                    int j = 0;
                    qms.remove(0, qms.length());
                    while (buf[i][j] == '>')
                    {
                        qms += '>';
                        j++;
                    }
                    softBreak(buf[i], qms.c_str());
                    if (count > 0)
                        content += "\r\n";
                    content += buf[i];
                }
                else if (count > 0)
                    content += "\r\n";
                count++;
            }
            delete[] buf;
            buf = nullptr;
        }

        void softBreak(String &content, const char *quoteMarks)
        {
            size_t len = 0;
            int size = split(content, " ", nullptr, 0);
            String *buf = new String[size];
            size = split(content, " ", buf, size);
            clear(content);
            for (int i = 0; i < size; i++)
            {
                if (buf[i].length() > 0)
                {
                    if (len + buf[i].length() + 3 > FLOWED_TEXT_LEN)
                    {
                        /* insert soft crlf */
                        content += " \r\n";

                        /* insert quote marks */
                        if (strlen(quoteMarks) > 0)
                            content += quoteMarks;
                        content += buf[i];
                        len = buf[i].length();
                    }
                    else
                    {
                        if (len > 0)
                        {
                            content += " ";
                            len++;
                        }
                        content += buf[i];
                        len += buf[i].length();
                    }
                }
            }
            delete[] buf;
            buf = nullptr;
        }

        void setContentTypeHeader(String &buf, const String &boundary, const String &mime, const String &ct_prop, const String &transfer_encoding, const String &dispos_type, const String &filename, int size, const String &location, const String &cid)
        {
            if (boundary.length())
                rd_print_to(buf, 250, "\r\n--%s\r\n", boundary.c_str());

            rd_print_to(buf, 250, "Content-Type: %s%s\r\n", mime.c_str(), ct_prop.c_str());

            if (dispos_type.length())
            {
                String dispos_prop;
                if (filename.length())
                {
                    rd_print_to(dispos_prop, 100, "; filename=\"%s\";", filename.c_str());
                    if (size)
                        rd_print_to(dispos_prop, 50, " size=%d;", size);
                }

                rd_print_to(buf, 250, "Content-Disposition: %s%s\r\n", dispos_type.c_str(), dispos_prop.c_str());
                if (cid.length())
                    rd_print_to(buf, 250, "Content-Location: %s\r\nContent-ID: <%s>\r\n", location.c_str(), cid.c_str());
            }

            if (transfer_encoding.length())
                rd_print_to(buf, 250, "Content-Transfer-Encoding: %s\r\n\r\n", transfer_encoding.c_str());
            else
                buf += "\r\n";
        }

        bool sendAttachment(SMTPMessage &msg, smtp_attach_type type, int content_type_index, bool close_parent_boundary)
        {
            if (msg.attachments.attachments_idx < (int)msg.attachments.size() && cAttach(msg).type == type)
            {
                if (cAttach(msg).data_index == 0)
                {
                    if (cAttach(msg).attach_file.blob && cAttach(msg).attach_file.blob_size)
                        cAttach(msg).data_size = cAttach(msg).attach_file.blob_size;
#if defined(ENABLE_FS)
                    else if (cAttach(msg).attach_file.callback)
                    {
                        if (msg.file && msg.file_opened)
                            msg.file.close();

                        // open file read
                        cAttach(msg).attach_file.callback(msg.file, cAttach(msg).attach_file.path.c_str(), readymail_file_mode_open_read);
                        if (msg.file)
                        {
                            cAttach(msg).data_size = msg.file.size();
                            msg.file_opened = true;
                        }
                    }
#endif
                    if (cAttach(msg).data_size > 0)
                    {
                        String str;
                        switch (type)
                        {
                        case attach_type_inline:
                            msg.attachments.inline_idx++;
                            rd_print_to(str, 100, "Sending inline image, %s (%d) %d of %d...", cAttach(msg).filename.c_str(), cAttach(msg).data_size, msg.attachments.inline_idx, msg.attachments.count(type));
                            break;

                        case attach_type_attachment:
                            msg.attachments.attachment_idx++;
                            rd_print_to(str, 100, "Sending attachment, %s (%d) %d of %d...", cAttach(msg).filename.c_str(), cAttach(msg).data_size, msg.attachments.attachment_idx, msg.attachments.count(type));
                            break;

                        case attach_type_parallel:
                            msg.attachments.parallel_idx++;
                            rd_print_to(str, 100, "Sending parallel attachment, %s (%d) %d of %d...", cAttach(msg).filename.c_str(), cAttach(msg).data_size, msg.attachments.parallel_idx, msg.attachments.count(type));
                            break;

                        default:
                            break;
                        }

                        if (str.length())
                            setDebugState(smtp_state_send_body, str);

                        updateUploadStatus(cAttach(msg));

                        String buf, ct_prop;
                        rd_print_to(ct_prop, 250, "; Name=\"%s\";", cAttach(msg).name.c_str());
                        setContentTypeHeader(buf, msg.content_types[content_type_index].boundary, cAttach(msg).mime, ct_prop, "base64", type == attach_type_inline ? "inline" : (type == attach_type_parallel ? "parallel" : "attachment"), cAttach(msg).filename, cAttach(msg).data_size, cAttach(msg).name, type == attach_type_inline ? cAttach(msg).content_id : "");

                        if (!sendBuffer(buf))
                            return false;

                        setState(smtp_state_send_body, smtp_server_status_code_0);
                        return sendAttachData(msg);
                    }
                }
                else if (cAttach(msg).data_index < cAttach(msg).data_size)
                    return sendAttachData(msg);
            }

            msg.attachments.attachments_idx++;

            // if send data completes
            if (msg.attachments.count(type) == (type == attach_type_inline ? msg.attachments.inline_idx : (type == attach_type_parallel ? msg.attachments.parallel_idx : msg.attachments.attachment_idx)))
            {
                msg.resetIndex();

                if (!sendEndBoundary(msg, content_type_index))
                    return false;

                if (close_parent_boundary && content_type_index >= 1 && !sendEndBoundary(msg, content_type_index - 1))
                    return false;

                if (type == attach_type_inline || type == attach_type_parallel)
                    msg.send_state++;

                if ((type == attach_type_parallel && !msg.attachments.exists(attach_type_attachment)) || (type == attach_type_attachment) || (type == attach_type_inline && content_type_index >= 0 && msg.send_state_root != smtp_send_state_body_type_2 && msg.send_state_root != smtp_send_state_body_type_2P && msg.send_state_root != smtp_send_state_body_type_3 && msg.send_state_root != smtp_send_state_body_type_3P) /* related only */)
                    return terminateData(msg);
            }
            return true;
        }

        bool terminateData(SMTPMessage &msg)
        {
            // embed message
            if (msg.parent)
            {
                this->msg_ptr = msg.parent;
                msg.parent->rfc822_idx++;
                if (msg.parent->rfc822_idx == (int)msg.parent->rfc822.size())
                    setState(smtp_state_send_body, smtp_server_status_code_0);
                return true;
            }

            if (!smtp_ctx->options.accumulate && !smtp_ctx->options.imap_mode && !sendBuffer("\r\n.\r\n"))
                return false;
            else if (smtp_ctx->options.imap_mode && smtp_ctx->options.last_append)
                sendBuffer("\r\n");

            setState(smtp_state_data_termination, smtp_server_status_code_250);
            return true;
        }

        int readBlob(SMTPMessage &msg, uint8_t *buf, int len)
        {
            memcpy(buf, cAttach(msg).attach_file.blob + cAttach(msg).data_index, len);
            return len;
        }

        bool sendAttachData(SMTPMessage &msg)
        {
            bool ret = false;
            if (cAttach(msg).data_size)
            {

#if defined(ENABLE_FS)
                int available = cAttach(msg).attach_file.callback ? msg.file.available() : cAttach(msg).attach_file.blob_size - cAttach(msg).data_index;
#else
                int available = cAttach(msg).attach_file.blob_size - cAttach(msg).data_index;
#endif
                validateAttEnc(cAttach(msg), available);

                int chunkSize = cAttach(msg).content_encoding != cAttach(msg).transfer_encoding ? 57 : BASE64_CHUNKED_LEN;

                int toSend = available > chunkSize ? chunkSize : available;
                if (toSend)
                {
                    uint8_t *readBuf = (uint8_t *)malloc(toSend + 1);
                    memset(readBuf, 0, toSend + 1);
#if defined(ENABLE_FS)
                    int read = cAttach(msg).attach_file.callback ? msg.file.read(readBuf, toSend) : readBlob(msg, readBuf, toSend);
#else
                    int read = readBlob(msg, readBuf, toSend);
#endif
                    String buf;

                    cAttach(msg).data_index += read;
                    updateUploadStatus(cAttach(msg));

                    if (cAttach(msg).content_encoding != cAttach(msg).transfer_encoding)
                    {
                        char *enc = rd_base64_encode(readBuf, read);
                        if (enc)
                        {
                            buf = enc;
                            rd_release((void *)enc);
                        }
                    }
                    else
                        buf = (const char *)readBuf;

                    rd_release((void *)readBuf);
                    buf += "\r\n";

                    if (!sendBuffer(buf))
                        goto exit;

                    setState(smtp_state_send_body, smtp_server_status_code_0);
                    ret = true;
                }
            }
#if defined(ENABLE_FS)
            if (msg.file && cAttach(msg).attach_file.callback && cAttach(msg).data_size - cAttach(msg).data_index == 0)
            {
                // close file
                msg.file.close();
                msg.file_opened = false;
            }
#endif
        exit:
            return ret;
        }

        void validateAttEnc(Attachment &cAtt, int len)
        {
            if (cAtt.data_index == 0)
            {
                cAtt.content_encoding.toLowerCase();
                if (cAtt.content_encoding.indexOf("base64") > -1 && (len * 3) % 4 > 0)
                    cAtt.content_encoding.remove(0, cAtt.content_encoding.length());
            }
        }

        void updateUploadStatus(Attachment &cAtt)
        {
            cAtt.progress = (float)(cAtt.data_index * 100) / (float)cAtt.data_size;
            if (cAtt.progress > 100.0f)
                cAtt.progress = 100.0f;

            if ((int)cAtt.progress != cAtt.last_progress && ((int)cAtt.progress > (int)cAtt.last_progress + 5 || (int)cAtt.progress == 0 || (int)cAtt.progress == 100))
            {
                smtp_ctx->status->progress = cAtt.progress;
                smtp_ctx->status->progressUpdated = true;
                smtp_ctx->status->filename = cAtt.filename;

                if (smtp_ctx->resp_cb)
                    smtp_ctx->resp_cb(*smtp_ctx->status);

                smtp_ctx->status->progressUpdated = false;
                cAtt.last_progress = cAtt.progress;
            }
        }

        smtp_function_return_code loop()
        {
            if (cState() != smtp_state_prompt && serverStatus())
            {
                if (cCode() == function_return_success)
                {
                    switch (cState())
                    {
                    case smtp_state_connect_command:
                    case smtp_state_send_command:
                    case smtp_state_send_data:
                        setDebug((cState() == smtp_state_connect_command ? "The server is connected successfully\n" : (cState() == smtp_state_send_command ? "The command is sent successfully\n" : "The data is sent successfully\n")));
                        setState(smtp_state_prompt, smtp_server_status_code_0);
                        cCode() = function_return_exit;
                        smtp_ctx->options.processing = false;
                        smtp_ctx->status->isComplete = true;
                        break;

                    case smtp_state_send_header_sender:
                        if (msg_ptr)
                            sendRecipient(*msg_ptr);
                        break;

                    case smtp_state_send_header_recipient:
                        if (msg_ptr && !msg_ptr->send_recipient_complete)
                            sendRecipient(*msg_ptr);
                        else if (msg_ptr)
                            startData(*msg_ptr);
                        break;

                    case smtp_state_wait_data:

                        if (msg_ptr)
                        {
                            setSendState(*msg_ptr, smtp_send_state_body_data);
                            sendBodyData(*msg_ptr);
                        }

                        break;

                    case smtp_state_send_body:
                        if (msg_ptr)
                            sendBodyData(*msg_ptr);
                        break;

                    case smtp_state_data_termination:
                        setState(smtp_state_prompt, smtp_server_status_code_0);
                        cCode() = function_return_exit;
                        smtp_ctx->options.processing = false;
                        smtp_ctx->status->isComplete = true;
                        setDebug("The E-mail is sent successfully\n");
                        break;

                    case smtp_state_terminated:
                        setState(smtp_state_prompt, smtp_server_status_code_0);
                        cCode() = function_return_exit;
                        smtp_ctx->options.processing = false;
                        break;

                    default:
                        break;
                    }
                }
                else if (cCode() == function_return_failure)
                {
                    switch (cState())
                    {
                    case smtp_state_send_header_sender:
                    case smtp_state_send_header_recipient:
                    case smtp_state_wait_data:
                        setError(__func__, SMTP_ERROR_SEND_HEADER);
                        break;

                    case smtp_state_send_body:
                        setError(__func__, SMTP_ERROR_SEND_BODY);
                        break;

                    case smtp_state_data_termination:
                    case smtp_state_terminated:
                        setError(__func__, SMTP_ERROR_SEND_DATA);
                    default:
                        break;
                    }
                }
            }
            return cCode();
        }

        bool sendQuit()
        {
            if (!sendBuffer("QUIT\r\n"))
                return false;
            setState(smtp_state_terminated, smtp_server_status_code_221);
            return true;
        }

        void setState(smtp_state state, smtp_server_status_code status_code)
        {
            res->begin(smtp_ctx);
            cState() = state;
            smtp_ctx->server_status->state_info.status_code = status_code;
        }

        void setXEnc(SMTPMessage &msg)
        {
            if (msg.text.content.length() && msg.text.transfer_encoding.length())
                setXEnc(msg.text.xenc, msg.text.transfer_encoding);

            if (msg.html.content.length() && msg.html.transfer_encoding.length())
                setXEnc(msg.html.xenc, msg.html.transfer_encoding);

            for (size_t i = 0; i < msg.attachments.size(); i++)
                setXEnc(msg.attachments[i].xenc, msg.attachments[i].transfer_encoding);
        }

        void setXEnc(smtp_content_xenc &xenc, const String &enc)
        {
            if (strcmp(enc.c_str(), smtp_transfer_encoding_t::enc_binary) == 0)
                xenc = xenc_binary;
            else if (strcmp(enc.c_str(), smtp_transfer_encoding_t::enc_8bit) == 0)
                xenc = xenc_8bit;
            else if (strcmp(enc.c_str(), smtp_transfer_encoding_t::enc_7bit) == 0)
                xenc = xenc_7bit;
        }

        void setMIMEList(SMTPMessage &msg)
        {
            msg.resetIndex();
            msg.content_types.clear();

            // If no html version or no content id, treat inline image as a attachment
            if (msg.attachments.exists(attach_type_inline) && (msg.html.content.length() == 0 || msg.html.content.indexOf("cid:") == -1))
                msg.attachments.inlineToAttachment();

            if (!msg.attachments.exists(attach_type_attachment) && !msg.attachments.exists(attach_type_parallel) && msg.rfc822.size() == 0)
            {
                if (msg.attachments.exists(attach_type_inline))
                {
                    if (msg.text.content.length() > 0)
                    {
                        msg.content_types.push_back(content_type_data("alternative")); // both text and html
                        msg.content_types.push_back(content_type_data("related"));     // inline
                    }
                    else
                        msg.content_types.push_back(content_type_data("related")); // html only inline
                }
                else                                                                                                                                                                                               // no attchment and inline image
                    msg.content_types.push_back(content_type_data(msg.html.content.length() > 0 && msg.text.content.length() > 0 ? "alternative" : (msg.html.content.length() > 0 ? "text/html" : "text/plain"))); // both text and html
            }
            else
            {
                msg.content_types.push_back(content_type_data("mixed"));
                // has attachment and inline image
                if (msg.attachments.exists(attach_type_inline))
                {
                    // has text and html versions
                    if (msg.text.content.length() > 0)
                        msg.content_types.push_back(content_type_data("alternative"));
                    msg.content_types.push_back(content_type_data("related"));
                }
                else // has only attachment
                    msg.content_types.push_back(content_type_data(msg.html.content.length() > 0 && msg.text.content.length() > 0 ? "alternative" : (msg.html.content.length() > 0 ? "text/html" : "text/plain")));

                if (msg.attachments.exists(attach_type_parallel))
                    msg.content_types.push_back(content_type_data("parallel"));

                for (size_t i = 0; i < msg.rfc822.size(); i++)
                    msg.rfc822[i].parent = &msg;
            }
        }
        void setSendState(SMTPMessage &msg, smtp_send_state state) { msg.send_state = state; }

        bool initSendState(SMTPMessage &msg)
        {
            msg.text.data_index = 0;
            msg.text.data_size = 0;
            clear(msg.text.enc_text);

            msg.html.data_index = 0;
            msg.html.data_size = 0;
            clear(msg.html.enc_text);

            setState(smtp_state_send_body, smtp_server_status_code_0);
            msg.send_state++;

            if (msg.content_types.size())
            {
                if (msg.content_types[0].mime.indexOf("text/") > -1)
                    setSendState(msg, smtp_send_state_body_type_1);
                else
                {
                    if (msg.content_types[0].mime == "mixed")
                    {
                        if (msg.content_types[1].mime.indexOf("text/") == -1)
                        {
                            if (msg.content_types.size() == 4 && msg.content_types[1].mime == "alternative" && msg.content_types[2].mime == "related" && msg.content_types[3].mime == "parallel")
                                setSendState(msg, smtp_send_state_body_type_2P);
                            else if (msg.content_types.size() == 3 && msg.content_types[1].mime == "alternative" && msg.content_types[2].mime == "related")
                                setSendState(msg, smtp_send_state_body_type_2);
                            else if (msg.content_types.size() == 3 && msg.content_types[1].mime == "related" && msg.content_types[2].mime == "parallel")
                                setSendState(msg, smtp_send_state_body_type_3P);
                            else if (msg.content_types.size() == 3 && msg.content_types[1].mime == "alternative" && msg.content_types[2].mime == "parallel")
                                setSendState(msg, smtp_send_state_body_type_4P);
                            else if (msg.content_types.size() == 2 && msg.content_types[1].mime == "related")
                                setSendState(msg, smtp_send_state_body_type_3);
                            else if (msg.content_types.size() == 2 && msg.content_types[1].mime == "alternative")
                                setSendState(msg, smtp_send_state_body_type_4);
                        }
                        else
                        {
                            if (msg.content_types.size() == 3 && msg.content_types[2].mime == "alternative")
                                setSendState(msg, smtp_send_state_body_type_5P);
                            else
                                setSendState(msg, smtp_send_state_body_type_5);
                        }
                    }
                    else
                    {
                        //  no attachment
                        if (msg.content_types.size() > 1)
                            setSendState(msg, smtp_send_state_body_type_6);
                        else if (msg.content_types[0].mime == "related")
                            setSendState(msg, smtp_send_state_body_type_7);
                        else if (msg.content_types[0].mime == "alternative")
                            setSendState(msg, smtp_send_state_body_type_8);
                    }
                }
            }
            msg.send_state_root = msg.send_state;
            return true;
        }

        bool sendMultipartHeaderMIME(SMTPMessage &msg, int parent_content_type_index, int content_type_index)
        {
            return sendMultipartHeaderImpl(msg.content_types[content_type_index].boundary, msg.content_types[content_type_index].mime, parent_content_type_index > -1 ? msg.content_types[parent_content_type_index].boundary : "");
        }

        bool sendMultipartHeaderImpl(const String &boundary, const String &mime, const String &parent_boundary)
        {
            setDebugState(smtp_state_send_body, "Sending multipart content (" + mime + ")...");
            String buf;
            buf.reserve(128);
            if (parent_boundary.length())
                rd_print_to(buf, 250, "\r\n--%s\r\n", parent_boundary.c_str());
            rd_print_to(buf, 250, "Content-Type: multipart/%s; boundary=\"%s\"\r\n", mime.c_str(), boundary.c_str());
            return sendBuffer(buf);
        }

    public:
        SMTPSend() {}
    };
}
#endif
#endif