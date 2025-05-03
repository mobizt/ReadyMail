#ifndef IMAP_PARSER_H
#define IMAP_PARSER_H
#if defined(ENABLE_IMAP)
#include <Arduino.h>
#include "Common.h"
#include "./core/QBDecoder.h"

namespace ReadyMailIMAP
{
    class IMAPParser
    {
    private:
        NumString numString;

    public:
        IMAPParser() {}
        ~IMAPParser() {}

        bool next(const String &line, char terminator, int &index, int &lastIndex)
        {
            // Skip any escape sequence
            if (line[index] == '\\')
                index += 2;

            bool ret = index < lastIndex && line[index] != terminator;
            return ret && index < lastIndex;
        }

        void skip(const String &line, char terminator, int &index, int &lastIndex)
        {
            while (next(line, terminator, index, lastIndex))
                index++;
        }

        void getBoundary(const String &line, const String &beginToken, const String &lastToken, int &beginIndex, int &lastIndex)
        {
            beginIndex = beginToken.length() && line.indexOf(beginToken) > -1 ? line.indexOf(beginToken) + beginToken.length() : 0;

            while (line[beginIndex] == ' ') // skip sp
                beginIndex++;

            if (beginToken[beginToken.length() - 1] == '(')
            {
                lastIndex = beginIndex - 1;
                skipL(line, lastIndex, line.length() - 1);
            }
            else
                lastIndex = line.lastIndexOf(lastToken) > -1 ? line.lastIndexOf(lastToken) : line.length();
        }

        void skipL(const String &line, int &index, int lastIndex)
        {
            int list_count = 1;
            while (list_count > 0 && index < lastIndex)
            {
                index++;
                if (line[index] == '(')
                    list_count++;
                else if (line[index] == ')')
                    list_count--;
            }
        }

        void setSection(std::vector<part_ctx> &parts)
        {
            int max_depth = 0;
            for (int i = 0; i < (int)parts.size(); i++)
            {
                if (max_depth < parts[i].depth)
                    max_depth = parts[i].depth;
            }

            String *section = new String[max_depth + 1];
            for (int i = 0; i < (int)parts.size(); i++)
            {
                if (i > 0 && parts[i - 1].name == "message")
                {
                    parts[i].section = parts[i - 1].section;
                    continue;
                }

                if (parts[i].depth == 1)
                {
                    parts[i].section = numString.get(parts[i].num_specifier);
                    section[parts[i].depth] = parts[i].section;
                }
                else if (parts[i].depth > 0)
                {
                    if (parts[i].num_specifier == 1 && i > 0)
                        section[parts[i].depth] = parts[i - 1].section;

                    if (section[parts[i].depth].length() == 0)
                        section[parts[i].depth] = numString.get(parts[i].num_specifier);

                    parts[i].section = section[parts[i].depth];
                    parts[i].section += ".";
                    parts[i].section += parts[i].num_specifier;
                }
            }
            delete[] section;
            section = nullptr;
        }

        String nextToken(const String &line, int &i, int beginIndex, int lastIndex)
        {
            int pos1 = -1, pos2 = -1;
            while (i <= lastIndex)
            {
                sys_yield();

                if (line[i] == ' ' && pos1 > -1 && pos2 == -1)
                    pos2 = i;
                else
                {
                    if (pos1 == -1)
                        pos1 = i;

                    if (i == lastIndex && pos1 != -1 && pos2 == -1)
                        pos2 = (line[i - 1] == ')') ? i - 1 : i;
                    else if (line[i] == '(')
                    {
                        skipL(line, i, lastIndex);
                        if (line[i + 1] == '(')
                            pos2 = i + 1;
                        else
                        {
                            skip(line, ' ', i, lastIndex);
                            pos2 = i;
                        }
                    }
                    else if (line[i] == '"')
                    {
                        i++;
                        skip(line, '"', i, lastIndex);
                        skip(line, ' ', i, lastIndex);
                        pos2 = line[i] == ')' ? i - 1 : i;
                    }
                }
                i++;
                if (pos1 > -1 && pos2 > -1)
                    return line.substring(pos1 + (line[pos1] == '"' ? 1 : 0), pos2 - (line[pos2 - 1] == '"' ? 1 : 0));
            }
            return String();
        }

        uint8_t *decodeContent(String &res, int &decoded_len, part_ctx &cpart, imap_msg_ctx &cmsg)
        {
            uint8_t *decoded = nullptr;
            decoded_len = 0;
            int rlen = res.length();

            // Binary string (string with NUL char) without base64 encoding is not allowed.
            if (cpart.transfer_encoding == imap_transfer_encoding_base64 || cpart.transfer_encoding == imap_transfer_encoding_binary)
            {
                // expected 78 bytes long or less base64 encoded string
                removeLinebreak(res, rlen);
                decoded = (uint8_t *)rd_base64_decode_impl((const char *)res.c_str(), decoded_len);
                if (decoded && cpart.text_part)
                    decoded[decoded_len] = '\0';
            }
            else if (cpart.transfer_encoding == imap_transfer_encoding_quoted_printable)
            {
                if (res[rlen - 3] == '=' && res[rlen - 2] == '\r' && res[rlen - 1] == '\n') // remove soft break for QP
                {
                    res.remove(rlen - 3, 3);

                    if (cmsg.qp_chunk.length())
                        cmsg.qp_chunk += res;
                    else
                        cmsg.qp_chunk = res;

                    if (cmsg.qp_chunk.length() > 1024)
                        goto decode;

                    return nullptr;
                }
                else
                {
                decode:
                    const char *buf = nullptr;
                    if (cmsg.qp_chunk.length())
                    {
                        cmsg.qp_chunk += res;
                        buf = cmsg.qp_chunk.c_str();
                    }
                    else
                        buf = res.c_str();

                    decoded = (uint8_t *)allocate((cmsg.qp_chunk.length() ? cmsg.qp_chunk.length() : rlen) + 10);
                    rd_decode_qp_utf8(buf, (char *)decoded);
                    cmsg.qp_chunk.remove(0, cmsg.qp_chunk.length());
                    decoded_len = strlen((char *)decoded);
                }
            }
            else if (cpart.transfer_encoding == imap_transfer_encoding_7bit)
            {
                char *buf = rd_decode_7bit_utf8(res.c_str());
                decoded_len = strlen(buf);
                decoded = (uint8_t *)buf;
            }
            else if (cpart.transfer_encoding == imap_transfer_encoding_8bit)
            {
                char *buf = rd_decode_8bit_utf8(res.c_str());
                decoded_len = strlen(buf);
                decoded = (uint8_t *)buf;
            }
            return decoded;
        }

        uint8_t *decodeText(uint8_t *decoded, int decoded_len, part_ctx &cpart)
        {
            if (cpart.text_part)
            {
                if (cpart.char_encoding == imap_char_encoding_scheme_iso8859_1)
                {
                    int len = (decoded_len + 1) * 2;
                    unsigned char *buf = (unsigned char *)allocate(len);
                    rd_decode_latin1_utf8(buf, &len, (unsigned char *)decoded, &decoded_len);
                    rd_release((void *)decoded);
                    decoded_len = len;
                    decoded = (uint8_t *)buf;
                }
                else if (cpart.char_encoding == imap_char_encoding_scheme_tis_620 || cpart.char_encoding == imap_char_encoding_scheme_iso8859_11 || cpart.char_encoding == imap_char_encoding_scheme_windows_874)
                {
                    char *buf = (char *)allocate((decoded_len + 1) * 3);
                    rd_decode_tis620_utf8(buf, (const char *)decoded, decoded_len);
                    decoded_len = strlen(buf);
                    rd_release((void *)decoded);
                    decoded = (uint8_t *)buf;
                }
            }
            return decoded;
        }

        String getField(part_ctx &cpart, imap_body_structure_non_multipart_fields field_type, int index = -1, bool key = false)
        {
            int i = 0;
            if (cpart.field.size())
            {
                while (cpart.field[i].index != field_type && i < (int)cpart.field.size())
                    i++;

                if (index > -1)
                {
                    int cnt = 0;
                    while (cpart.field[i].index == field_type && i < (int)cpart.field.size())
                    {
                        if (key && cnt == index * 2)
                            return cpart.field[i].token;

                        if (!key && cnt == index * 2 + 1)
                            return cpart.field[i].token;

                        cnt++;
                        i++;
                    }
                }
            }
            return i < (int)cpart.field.size() ? cpart.field[i].token : String();
        }

        void getPartTransferEncoding(part_ctx &cpart)
        {
            if (getField(cpart, non_multipart_field_encoding) == "quoted-printable")
                cpart.transfer_encoding = imap_transfer_encoding_quoted_printable;
            else if (getField(cpart, non_multipart_field_encoding) == "base64")
                cpart.transfer_encoding = imap_transfer_encoding_base64;
            else if (getField(cpart, non_multipart_field_encoding) == "7bit")
                cpart.transfer_encoding = imap_transfer_encoding_7bit;
            else if (getField(cpart, non_multipart_field_encoding) == "8bit")
                cpart.transfer_encoding = imap_transfer_encoding_8bit;
            else if (getField(cpart, non_multipart_field_encoding) == "binary")
                cpart.transfer_encoding = imap_transfer_encoding_binary;
        }

        void InitPart(String &line, part_ctx &cpart)
        {
            cpart.decoded_len = 0;
            cpart.total_read = 0;
            cpart.initialized = true;
            cpart.octet_count = getField(cpart, non_multipart_field_size).toInt();
            getPartTransferEncoding(cpart);

            cpart.mime = getField(cpart, non_multipart_field_type);
            cpart.text_part = cpart.mime == "text";
            cpart.mime += "/";
            cpart.mime += getField(cpart, non_multipart_field_subtype);
            cpart.data_index = 0;
            if (cpart.text_part)
                cpart.char_encoding = getCharEncoding(getCharset(cpart));

            if (!cpart.octet_count)
                cpart.octet_count = getOctetLen(line);

            if (cpart.transfer_encoding == imap_transfer_encoding_base64 || cpart.transfer_encoding == imap_transfer_encoding_binary)
            {
                cpart.decoded_size = numString.toNum(getPartFiled(cpart, non_multipart_field_disposition, "size", false).c_str());
                if (cpart.decoded_size == 0)
                    cpart.decoded_size = cpart.octet_count * 3 / 4;
            }
        }

        uint32_t getOctetLen(const String &line)
        {
            uint32_t len = 0;
            int pos1 = line.lastIndexOf("{"), pos2 = line.lastIndexOf("}");
            if (pos1 < pos2 && pos1 > -1 && pos2 > -1)
                len = numString.toNum(line.substring(pos1 + 1, pos2).c_str());
            return len;
        }

        String getFilename(part_ctx &cpart)
        {
            String filename = getPartFiled(cpart, non_multipart_field_disposition, "filename", false);
            if (filename.length())
                return filename;
            return getPartFiled(cpart, non_multipart_field_disposition, "name", false);
        }

        String getName(part_ctx &cpart) { return getPartFiled(cpart, non_multipart_field_parameter_list, "name", false); }

        String getCharset(part_ctx &cpart) { return getPartFiled(cpart, non_multipart_field_parameter_list, "charset", true); }

        String getPartFiled(part_ctx &cpart, imap_body_structure_non_multipart_fields field, const String &name, bool toLowercase)
        {
            String val;
            int i = 0;
            while (getField(cpart, field, i, true).length())
            {
                if (getField(cpart, field, i, true) == name)
                {
                    val = getField(cpart, field, i, false);
                    break;
                }
                i++;
            }
            if (toLowercase)
                val.toLowerCase();
            return val;
        }

        bool isLastOctet(const String &line) { return line.length() >= 3 && line[line.length() - 3] == ')' && line[line.length() - 2] == '\r' && line[line.length() - 1] == '\n'; }

        // remove last of octet bytes ')\r\n' if existed
        void removeLastOctet(String &line, part_ctx &cpart)
        {
            cpart.last_octet = isLastOctet(line);
            if (cpart.last_octet)
                line.remove(line.length() - 3, 3);
            cpart.total_read -= 3;
        }

        // resore last of octet bytes ')\r\n' if removed
        void restoreLastOctet(part_ctx &cpart, imap_msg_ctx &cmsg)
        {
            if (cpart.last_octet && cpart.transfer_encoding != imap_transfer_encoding_base64 && cpart.transfer_encoding != imap_transfer_encoding_binary)
            {
                cpart.last_octet = false;
                if (cmsg.raw_chunk.length())
                    cmsg.raw_chunk += ")\r\n";
                else
                    cmsg.raw_chunk = ")\r\n";
            }
        }

        int readContent(const String &line, String &res, part_ctx &cpart, imap_msg_ctx &cmsg)
        {
            int llen = line.length();
            // we expect the line with linebreak
            if (line[llen - 2] == '\r' && line[llen - 1] == '\n')
            {
                restoreLastOctet(cpart, cmsg); // check if last octet bytes removed, then restore
                if (cmsg.raw_chunk.length())
                {
                    res = cmsg.raw_chunk;
                    res += line;
                    cmsg.raw_chunk.remove(0, cmsg.raw_chunk.length());
                }
                else
                    res = line;
            }
            else // partial data or soft linebreak exists (qp encoded data)
            {
                restoreLastOctet(cpart, cmsg); // check if last octet bytes removed, then restore
                if (cmsg.raw_chunk.length())
                    cmsg.raw_chunk += line;
                else
                    cmsg.raw_chunk = line;
                return 0;
            }
            return res.length();
        }

        void removeLinebreak(String &res, int rlen)
        {
            if (res[rlen - 2] == '\r' && res[rlen - 1] == '\n')
                res.remove(rlen - 2, 2);
        }

        void storeDecodedData(uint8_t *decoded, int decoded_len, bool isComplete, part_ctx &cpart, imap_context *imap_ctx)
        {
            imap_ctx->cb_data.filename = cpart.filepath.c_str();
            imap_ctx->cb_data.mime = cpart.mime.c_str();
            imap_ctx->cb_data.dataIndex = cpart.data_index;
            imap_ctx->cb_data.blob = decoded;
            imap_ctx->cb_data.dataLength = decoded_len;
            imap_ctx->cb_data.size = cpart.decoded_size;
            imap_ctx->cb_data.isComplete = isComplete;

            if ((int)cpart.progress != cpart.last_progress && ((int)cpart.progress > (int)cpart.last_progress + 5 || (int)cpart.progress == 0 || (int)cpart.progress == 100))
            {
                imap_ctx->cb_data.progressUpdated = true;
                imap_ctx->cb_data.progress = cpart.progress;
                cpart.last_progress = cpart.progress;
            }

            if (imap_ctx->cb.data)
                imap_ctx->cb.data(imap_ctx->cb_data);

            imap_ctx->cb_data.progressUpdated = false;

#if defined(ENABLE_FS)
            if (imap_ctx->file && imap_ctx->cb.file)
                imap_ctx->file.write(decoded, decoded_len);
#endif
            cpart.data_index += decoded_len;
        }

        void decodeString(String &str, const char *enc = "")
        {
            // handle rfc2047 Q (quoted printable) and B (base64) decodings
            QBDecoder decoder;
            int pos1 = 0, pos2 = 0;
            String headerEnc;

            if (strlen(enc) == 0)
            {
                while (str[pos1] == ' ' && pos1 < (int)str.length() - 1)
                    pos1++;

                if (str[pos1] == '=' && str[pos1 + 1] == '?')
                {
                    pos2 = str.indexOf('?', pos1 + 2);
                    if (pos2 > -1)
                        headerEnc = str.substring(pos1 + 2, pos2 - 2);
                }
            }
            else
                headerEnc = enc;

            int bufSize = str.length() + 10;
            char *buf = (char *)allocate(bufSize);

            // Content Q and B decodings
            decoder.decode(buf, str.c_str(), bufSize);

            // Char set decoding
            imap_char_encoding_scheme scheme = getCharEncoding(headerEnc.c_str());
            if (scheme == imap_char_encoding_scheme_iso8859_1)
            {
                int len = strlen(buf);
                int olen = (len + 1) * 2;
                unsigned char *out = (unsigned char *)allocate(olen);
                rd_decode_latin1_utf8(out, &olen, (unsigned char *)buf, &len);
                rd_release((void *)buf);
                buf = (char *)out;
            }
            else if (scheme == imap_char_encoding_scheme_tis_620 || scheme == imap_char_encoding_scheme_iso8859_11 || scheme == imap_char_encoding_scheme_windows_874)
            {
                size_t len2 = strlen(buf);
                char *out = (char *)allocate((len2 + 1) * 3);
                rd_decode_tis620_utf8(out, buf, len2);
                rd_release((void *)buf);
                buf = out;
            }
            str = buf;
            rd_release((void *)buf);
        }

        String getToken(const String &line, int pos, const String &beginToken, const String &lastToken)
        {
            int beginIndex = 0, lastIndex = 0, count = 0;
            getBoundary(line, beginToken, lastToken, beginIndex, lastIndex);
            String token;
            int i = beginIndex;
            while (i <= lastIndex)
            {
                token = nextToken(line, i, beginIndex, lastIndex);
                if (pos == count)
                    break;
                count++;
            }
            return token;
        }

        imap_char_encoding_scheme getCharEncoding(const String &enc)
        {
            imap_char_encoding_scheme scheme = imap_char_encoding_scheme_default;
            for (int i = imap_char_encoding_utf8; i < imap_char_encoding_max_type; i++)
            {
                if (enc.indexOf(imap_char_encodings[i].text) > -1)
                    scheme = (imap_char_encoding_scheme)i;
            }
            return scheme;
        }

        void *allocate(int len)
        {
            void *buf = (void *)malloc(len);
            resetMem(buf, len);
            return buf;
        }

        void resetMem(void *m, int size) { memset(m, 0, size); }

        void parseBodyStructure(const String &line, const String &beginToken, const String &lastToken, int depth, int index, int field_type, std::vector<part_ctx> &parts, part_ctx *cpart)
        {
            int beginIndex = 0, lastIndex = 0;
            getBoundary(line, beginToken, lastToken, beginIndex, lastIndex);
            int i = beginIndex;

            int field_index = 0, part_count = 0, rnd = random(100000, 200000) + micros();
            bool multipart = false, key = false;

            if (line.indexOf("\"mixed\"") > -1 || line.indexOf("\"MIXED\"") > -1 || line.indexOf("\"alternative\"") > -1 || line.indexOf("\"ALTERNATIVE\"") > -1 || line.indexOf("\"related\"") > -1 || line.indexOf("\"RELATED\"") > -1)
                multipart = true;

            while (i <= lastIndex)
            {
                String token = nextToken(line, i, beginIndex, lastIndex);
                if (token.length())
                {
                    if (token[0] == '(' && token[token.length() - 1] == ')')
                    {
                        part_count++;
                        part_ctx part;
                        part.parent_id = numString.get(rnd);
                        parseBodyStructure(token, "(", ")", depth + 1, part_count, field_index, parts, &part);
                        field_index++;
                    }
                    else
                    {
                        String part_type = token;
                        part_type.toLowerCase();

                        if (part_type == "nil")
                            token.remove(0, token.length());

                        if (part_type == "attachment" || part_type == "inline") // disposition
                        {
                            key = false;
                            field_index = field_type;
                            continue;
                        }

                        if (cpart && (part_type == "message" || part_type == "text" || part_type == "mixed" || part_type == "alternative" || part_type == "related" || part_type == "application" || part_type == "image" || part_type == "audio" || part_type == "video"))
                        {
                            field_index = 0;
                            cpart->name = part_type;
                            if (part_type == "mixed")
                                index = 1;
                            cpart->num_specifier = index;
                            cpart->part_count = part_count;
                            cpart->depth = depth;
                            cpart->id = numString.get(rnd);
                            cpart->section = multipart ? "" : String(index);
                            bool inserted = false;
                            if (multipart)
                            {
                                int size = parts.size();
                                for (int j = 0; j < size; j++)
                                {
                                    if (parts[j].parent_id == cpart->id)
                                    {
                                        if (!inserted)
                                        {
                                            inserted = true;
                                            parts.insert(parts.begin() + j, 1, *cpart);
                                            size++;
                                        }
                                    }
                                }
                            }

                            if (!inserted)
                                parts.push_back(*cpart);
                        }

                        if (parts.size())
                        {
                            body_part_field field;
                            field.depth = depth;
                            field.index = -1;

                            if (parts[parts.size() - 1].id == String(rnd))
                                field.index = field_index;
                            else if (parts[parts.size() - 1].depth < depth)
                            {
                                field.index = field_type;
                                key = !key;
                                if (token.length() && key)
                                    token.toLowerCase();
                                else
                                    decodeString(token);
                            }

                            // Ignore any extension fields of message/xxxx part
                            if (parts[parts.size() - 1].field.size() && parts[parts.size() - 1].field[0].token == "message" && (field.index == 0 || field.index > non_multipart_field_size))
                                continue;

                            if (token.length() && field.index != 2 && field.index != 3 && field.index != 4 && field.index != 8)
                                token.toLowerCase();

                            field.token = token;

                            if (field.index > -1)
                                parts[parts.size() - 1].field.push_back(field);
                        }
                        field_index++;
                    }
                }
            }

            if (depth == 0)
                setSection(parts);
        }

        void parseCaps(String &line, imap_context *imap_ctx)
        {
            if (line.indexOf("CAPABILITY") > -1)
            {
                String token;
                int beginIndex = 0, lastIndex = 0;
                getBoundary(line, "CAPABILITY", "\r\n", beginIndex, lastIndex);
                int i = beginIndex;
                while (i <= lastIndex)
                {
                    token = nextToken(line, i, beginIndex, lastIndex);
                    for (int i = imap_auth_cap_plain; i < imap_auth_cap_max_type; i++)
                    {
                        if (strcmp(token.c_str(), imap_auth_cap_token[i].text) == 0)
                            imap_ctx->auth_caps[i] = true;
                    }

                    for (int i = imap_read_cap_imap4; i < imap_read_cap_max_type - 1; i++)
                    {
                        if (strcmp(token.c_str(), imap_read_cap_token[i].text) == 0)
                        {
                            imap_ctx->feature_caps[i] = true;
                            if (i == imap_read_cap_logindisable)
                                imap_ctx->auth_caps[imap_auth_cap_login] = false;
                        }
                    }
                }
            }
        }

        void parseIdle(const String &line, MailboxInfo &mailbox_info, imap_context *imap_ctx)
        {
            if (line[0] != '*')
                imap_ctx->idle_status.remove(0, imap_ctx->idle_status.length());
            else
            {
                imap_ctx->idle_available = 1;
                if (line.indexOf("EXPUNGE") > -1)
                {
                    imap_ctx->idle_status = "[-] " + getToken(line, 0, "* ", "EXPUNGE");
                    imap_ctx->current_message = numString.toNum(getToken(line, 0, "* ", "EXPUNGE").c_str());
                    mailbox_info.msgCount = imap_ctx->current_message;
                }
                else if (line.indexOf("EXISTS") > -1)
                {
                    imap_ctx->idle_status = "[+] " + getToken(line, 0, "* ", "EXISTS");
                    imap_ctx->current_message = numString.toNum(getToken(line, 0, "* ", "EXISTS").c_str());
                    mailbox_info.msgCount = imap_ctx->current_message;
                }
                else if (line.indexOf("FETCH") > -1)
                {
                    imap_ctx->idle_status = "[=][";
                    imap_ctx->current_message = numString.toNum(getToken(line, 0, "* ", "FETCH").c_str());
                    int beginIndex = 0, lastIndex = 0;
                    getBoundary(line, "FLAGS (", ")", beginIndex, lastIndex);
                    int i = beginIndex, count = 0;
                    while (i <= lastIndex)
                        imap_ctx->idle_status += (count++ > 0 ? ", " : "") + nextToken(line, i, beginIndex, lastIndex);
                    imap_ctx->idle_status += "] " + getToken(line, 0, "* ", "FETCH");
                }
            }
        }

        void parseSearch(const String &line, imap_context *imap_ctx, std::vector<uint32_t> &imap_msg_num, MailboxInfo &mailbox_info)
        {
            if (line.indexOf(imap_ctx->tag) > -1)
            {
                if (imap_ctx->options.recent_sort)
                    std::sort(imap_msg_num.begin(), imap_msg_num.end(), compareMore);
                return;
            }

            String token;
            int beginIndex = 0, lastIndex = 0;
            getBoundary(line, "* SEARCH", line[line.length() - 1] == ' ' ? " " : "\r\n", beginIndex, lastIndex);
            int i = beginIndex;
            uint32_t msg_num = 0;
            while (i <= lastIndex)
            {
                token = nextToken(line, i, beginIndex, lastIndex);

                if (token.length() == 0)
                    break;

                imap_ctx->cb_data.searchCount++;
                msg_num = numString.toNum(token.c_str());
                if (imap_ctx->options.recent_sort)
                {
                    imap_msg_num.push_back(msg_num);
                    if (imap_msg_num.size() > imap_ctx->options.search_limit)
                        imap_msg_num.erase(imap_msg_num.begin());
                }
                else if (imap_msg_num.size() < imap_ctx->options.search_limit)
                    imap_msg_num.push_back(msg_num);
            }
        }

        void parseMailbox(const String &line, std::array<String, 3> &buf)
        {
            int beginIndex = 0, lastIndex = 0, count = 0;
            getBoundary(line, "LIST ", "\r\n", beginIndex, lastIndex);
            int i = beginIndex;
            while (i <= lastIndex)
            {
                buf[count] = nextToken(line, i, beginIndex, lastIndex);
                if (buf[count][0] == '(' && buf[count][buf[count].length() - 1] == ')')
                    buf[count] = buf[count].substring(1, buf[count].length() - 1);
                count++;
                if (count == 3)
                    count = 0;
            }
        }

        void parseExamine(const String &line, MailboxInfo &mailbox_info, imap_context *imap_ctx)
        {
            if (line.indexOf("EXISTS") > -1)
                mailbox_info.msgCount = numString.toNum(getToken(line, 0, "* ", "EXISTS").c_str());
            else if (line.indexOf("RECENT") > -1)
                mailbox_info.RecentCount = numString.toNum(getToken(line, 0, "* ", "RECENT").c_str());
            else if (line.indexOf("FLAGS") > -1 || line.indexOf("PERMANENTFLAGS") > -1)
                parseFlags(line, mailbox_info);
            else if (line.indexOf("[UIDVALIDITY") > -1)
                mailbox_info.UIDValidity = numString.toNum(getToken(line, 0, "[UIDVALIDITY", "]").c_str());
            else if (line.indexOf("[UIDNEXT") > -1)
                mailbox_info.nextUID = numString.toNum(getToken(line, 0, "[UIDNEXT", "]").c_str());
            else if (line.indexOf("[UNSEEN") > -1)
                mailbox_info.UnseenIndex = numString.toNum(getToken(line, 0, "[UNSEEN", "]").c_str());
            else if (imap_ctx->feature_caps[imap_read_cap_condstore] && line.indexOf("[HIGHESTMODSEQ") > -1)
                mailbox_info.highestModseq = numString.toNum(getToken(line, 0, "[HIGHESTMODSEQ", "]").c_str());
            else if (line.indexOf("NOMODSEQ") > -1)
                mailbox_info.noModseq = true;
        }

        void parseFlags(const String &line, MailboxInfo &mailbox_info)
        {
            bool permanent = line.indexOf("PERMANENTFLAGS") > -1;
            int beginIndex = 0, lastIndex = 0;
            getBoundary(line, permanent ? "PERMANENTFLAGS (" : "FLAGS (", ")", beginIndex, lastIndex);
            int i = beginIndex;
            while (i <= lastIndex)
            {
                if (permanent)
                    mailbox_info.permanentFlags.push_back(nextToken(line, i, beginIndex, lastIndex));
                else
                    mailbox_info.flags.push_back(nextToken(line, i, beginIndex, lastIndex));
            }
        }

        void parseEnvelope(const String &line, const String &beginToken, const String &lastToken, int depth, String *header, int &header_index)
        {
            int beginIndex = 0, lastIndex = 0;
            getBoundary(line, beginToken, lastToken, beginIndex, lastIndex);
            int i = beginIndex;
            int addr_index = 0;
            String addr_struct[4];
            while (i <= lastIndex)
            {
                String token = nextToken(line, i, beginIndex, lastIndex);
                if (token.length())
                {
                    if (token[0] == '(' && token[token.length() - 1] == ')')
                    {
                        parseEnvelope(token, "(", ")", depth + 1, header, header_index);
                        if (depth == 0)
                            header_index++;
                    }
                    else
                    {
                        if (depth == 0)
                        {
                            if (token != "NIL")
                            {
                                decodeString(token);
                                header[header_index] = token;
                            }
                            header_index++;
                        }
                        else if (depth == 2)
                        {
                            if (addr_index == 0 && token != "NIL")
                                decodeString(token);
                            addr_struct[addr_index] = token;
                            addr_index++;

                            if (addr_index == 4 && addr_struct[2] != "NIL" && addr_struct[3] != "NIL")
                            {
                                String buf;
                                rd_print_to(buf, 200, "%s%s<%s@%s>", addr_struct[0] != "NIL" ? addr_struct[0].c_str() : "", addr_struct[0] != "NIL" ? " " : "", addr_struct[2].c_str(), addr_struct[3].c_str());
                                if (header[header_index].length())
                                    header[header_index] += ", ";
                                header[header_index] += buf;
                                addr_index = 0;
                            }
                        }
                    }
                }
            }
            if (depth == 0)
                header[imap_envelpe_attachments] = numString.get(getAttachCount(line, "BODY (", ")"));
        }

        void parseFetch(String &line, imap_context *imap_ctx, imap_msg_ctx &cmsg, imap_state &cstate, part_ctx &cpart)
        {
            if (line[0] == '*' || imap_ctx->options.multiline)
            {
                cmsg.exists = true;
                imap_ctx->cb_data.isEnvelope = false;
                if (cstate == imap_state_fetch_envelope)
                {
                    if (line[0] == '*')
                        imap_ctx->current_message = numString.toNum(getToken(line, 0, "* ", "FETCH").c_str());

                    if (line[line.length() - 3] == ')' && line[line.length() - 2] == '\r' && line[line.length() - 1] == '\n')
                        imap_ctx->options.multiline = false;
                    else
                    {
                        imap_ctx->options.multiline = true;
                        return;
                    }

                    String header[imap_envelpe_max_type];
                    int i = imap_envelpe_date;
                    parseEnvelope(line, "ENVELOPE (", ")", 0, header, i);

                    for (i = imap_envelpe_date; i < imap_envelpe_max_type; i++)
                        cmsg.header.emplace_back(imap_envelopes[i].text, header[i]);

                    if (imap_ctx->cb.data)
                    {
                        imap_ctx->cb_data.header = cmsg.header;
                        imap_ctx->cb_data.currentMsgIndex = imap_ctx->cur_msg_index;
                        imap_ctx->cb_data.isEnvelope = true;
                        imap_ctx->cb_data.isSearch = imap_ctx->options.searching;
                        imap_ctx->cb.data(imap_ctx->cb_data);
                    }
                }
                else if (cstate == imap_state_fetch_body_structure)
                {
                    if (line[line.length() - 3] == ')' && line[line.length() - 2] == '\r' && line[line.length() - 1] == '\n')
                        imap_ctx->options.multiline = false;
                    else
                    {
                        imap_ctx->options.multiline = true;
                        return;
                    }
                    parseBodyStructure(line, "BODYSTRUCTURE (" /* also supports BODY */, ")", 0, 1, -1, cmsg.parts, &cmsg.part);
                    debugBodystructure(cmsg, imap_ctx);
                }
                else if (cstate == imap_state_fetch_body_part && !cpart.initialized)
                {
                    InitPart(line, cpart);
                    openFile(imap_ctx, cpart);
                }
            }
            else if (cstate == imap_state_fetch_body_part && cpart.initialized)
            {
                if (line.indexOf(imap_ctx->tag) != 0)
                {
                    String res;
                    int rlen = readContent(line, res, cpart, cmsg);
                    if (rlen)
                    {
                        cpart.total_read += rlen;
                        // We cannot trust the total octet read and the octet count from non-base64 encoded string as
                        // some server reports incorrect octet count in none-base64 transfer encoding.
                        // Then we check for last octet bytes sequence ')\r\n' every line and remove it before decoding
                        removeLastOctet(res, cpart);

                        int decoded_len = 0;
                        if (cpart.transfer_encoding == imap_transfer_encoding_base64 && cpart.transfer_encoding == imap_transfer_encoding_binary)
                            removeLinebreak(res, rlen);

                        uint8_t *decoded = decodeContent(res, decoded_len, cpart, cmsg);
                        if (decoded)
                        {
                            if (decoded_len)
                            {
                                decoded = decodeText(decoded, decoded_len, cpart);
                                updateDownloadStatus(decoded_len, cpart);
                                storeDecodedData(decoded, decoded_len, false, cpart, imap_ctx);
                            }
                            rd_release((void *)decoded);
                        }
                    }
                }
                else
                {
                    cpart.progress = 100.0f;
                    cpart.last_progress = -1;
                    storeDecodedData(nullptr, 0, true, cpart, imap_ctx);
                    closeFile(imap_ctx);
                }
            }
        }

        int getAttachCount(const String &line, const String &beginToken, const String &lastToken)
        {
            int beginIndex = 0, lastIndex = 0;
            getBoundary(line, beginToken, lastToken, beginIndex, lastIndex);
            int i = beginIndex;
            int att_count = 0;
            while (i <= lastIndex)
            {
                String token = nextToken(line, i, beginIndex, lastIndex);
                if (token.length())
                {
                    if (token[0] == '(' && token[token.length() - 1] == ')')
                        att_count += getAttachCount(token, "(", ")");
                    else
                    {
                        token.toLowerCase();
                        if (token == "image" || token == "audio" || token == "video" || token == "application" || token == "message")
                            att_count++;
                    }
                }
            }
            return att_count;
        }

        void updateDownloadStatus(int len, part_ctx &cpart)
        {
            cpart.decoded_len += len;
            cpart.progress = (float)(cpart.total_read * 100) / (float)cpart.octet_count;
            if (cpart.progress > 100.0f)
                cpart.progress = 100.0f;
        }

        void openFile(imap_context *imap_ctx, part_ctx &cpart)
        {
#if defined(ENABLE_FS)
            cpart.filepath.remove(0, cpart.filepath.length());
            String name = getField(cpart, non_multipart_field_type) == "message" ? getName(cpart) : getFilename(cpart);
            if (name.length() == 0)
            {
                String part = cpart.section;
                part.replace(".", "_");
                rd_print_to(name, 100, "%s.%s", part.c_str(), getField(cpart, non_multipart_field_type) == "message" ? "eml" : (getField(cpart, non_multipart_field_subtype) == "plain" ? "txt" : "html"));
            }

            rd_print_to(cpart.filepath, 200, "/%d/%s", imap_ctx->options.fetch_number, name.c_str());
            if (imap_ctx->cb.file)
            {
                String path = imap_ctx->cb.download_path.length() ? imap_ctx->cb.download_path + "/" : "";
                path += cpart.filepath;
                imap_ctx->cb.file(imap_ctx->file, path.c_str(), readymail_file_mode_remove);
                imap_ctx->cb.file(imap_ctx->file, path.c_str(), readymail_file_mode_open_write);
            }
#endif
        }

        void closeFile(imap_context *imap_ctx)
        {
#if defined(ENABLE_FS)
            if (imap_ctx->file)
                imap_ctx->file.close();
#endif
        }

        void debugBodystructure(imap_msg_ctx &cmsg, imap_context *imap_ctx)
        {
#if defined(ENABLE_CORE_DEBUG)
            for (int i = 0; i < (int)cmsg.parts.size(); i++)
            {
                String tab;
                for (int j = 0; j < cmsg.parts[i].depth; j++)
                    tab += ' ';

                String buf;
                rd_print_to(buf, 200, "%sPart: %s, Section: %s, Numeric Specifier: %d, Parts Count: %d", tab.c_str(), cmsg.parts[i].name.c_str(), cmsg.parts[i].section.c_str(), cmsg.parts[i].num_specifier, cmsg.parts[i].part_count);

                IMAPBase::setDebug(imap_ctx, buf, true);

                if (cmsg.parts[i].field.size())
                {
                    buf.remove(0, buf.length());
                    rd_print_to(buf, 200, "%s Part Fields", tab.c_str());
                    IMAPBase::setDebug(imap_ctx, buf, true);
                    for (int k = 0; k < (int)cmsg.parts[i].field.size(); k++)
                    {
                        buf.remove(0, buf.length());
                        rd_print_to(buf, 200, "%s  Index: %d, Token: %s", tab.c_str(), cmsg.parts[i].field[k].index, cmsg.parts[i].field[k].token.c_str());
                        IMAPBase::setDebug(imap_ctx, buf, true);
                    }
                    IMAPBase::setDebug(imap_ctx, "", true);
                }
            }
#endif
        }
    };
}
#endif
#endif