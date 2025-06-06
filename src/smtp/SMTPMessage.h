/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SMTP_MESSAGE_H
#define SMTP_MESSAGE_H
#if defined(ENABLE_SMTP)
#include <Arduino.h>
#include "Common.h"

namespace ReadyMailSMTP
{
  struct content_type_data
  {
    friend class SMTPSend;

  private:
    String mime, boundary;

    explicit content_type_data(const String &mime)
    {
      this->mime = mime;
      if (mime == "alternative" || mime == "related" || mime == "mixed" || mime == "parallel")
        boundary = getMIMEBoundary(15);
    }

    String getMIMEBoundary(int len)
    {
      String tmp = boundary_map;
      char *buf = rd_mem<char *>(len + 1);
      if (len > 2)
      {
        --len;
        buf[0] = tmp[0];
        buf[1] = tmp[1];
        for (int n = 2; n < len; n++)
        {
          int key = rand() % (int)(tmp.length() - 1);
          buf[n] = tmp[key];
        }
        buf[len] = '\0';
      }
      String s = buf;
      rd_free(&buf);
      return s;
    }
  };

  struct smtp_headers
  {
    friend class SMTPMessage;
    friend class SMTPSend;

  private:
    std::vector<smtp_header_item> el;
    smtp_header_item default_hdr;

    void push_back(smtp_header_item hdr) { el.push_back(hdr); }

    smtp_header_item &operator[](int index)
    {
      if (index < (int)size())
        return el[index];
      return default_hdr;
    }

    int findHeaders(rfc822_header_types type)
    {
      for (size_t i = 0; i < size(); i++)
      {
        if (el[i].type == type)
          return i;
      }
      return -1;
    }

    // RFC2047 encode word (UTF-8 only)
    String encodeHeaderLine(const char *src)
    {
      String buf;
      size_t len = strlen(src);
      if (len > 4 && src[0] != '=' && src[1] != '?' && src[len - 1] != '=' && src[len - 2] != '?')
        encodeHeaderLineImpl(src, buf);
      else
        buf = src;
      return buf;
    }

    void encodeHeaderLineImpl(const char *src, String &out)
    {
      // rfc2822 section-2.2.3 Long Header Fields
      int len = strlen(src);
      String buf;
      buf.reserve(200);
      int index = 0;
      for (int i = 0; i < len; i++)
      {
        if (index <= 43)
        {
          buf += src[i];
          index++;
        }
        else
        {
          encodeWord(buf, out);
          buf.remove(0, buf.length());
          buf += src[i];
          index = 0;
        }
      }

      if (buf.length())
        encodeWord(buf, out);
    }

    void encodeWord(const String &str, String &out)
    {
      String buf;
      char *enc = rd_b64_enc(rd_cast<const unsigned char *>(str.c_str()), str.length());
      rd_print_to(buf, strlen(enc), "=?utf-8?B?%s?=", enc);
      rd_free(&enc);
      if (out.length())
        out += "\r\n ";
      out += buf;
    }

  public:
    /** Add header
     *
     * @param type The rfc822_header_types enum.
     * @param value The value.
     */
    smtp_headers &add(rfc822_header_types type, const String &value)
    {
      if (type == rfc822_max_type || type == rfc822_custom || (findHeaders(type) > -1 && !rfc822_headers[type].multi))
        return *this;

      // Email sub type
      if (rfc822_headers[type].sub_type > -1)
      {
        int p1 = value.indexOf("<");
        int p2 = value.indexOf(">");

        if ((p1 == -1 && p2 > -1) || (p1 > -1 && p2 == -1) || (p1 > p2))
          return *this;

        String email = p1 > -1 && p2 > -1 ? value.substring(p1 + 1, p2) : value;

        smtp_header_item hdr;

        // Email with name
        if (rfc822_headers[type].sub_type == 0 && p1 > -1 && p2 > -1)
          hdr.name = "\"" + value.substring(0, p1 - 1) + "\" ";

        hdr.type = type;
        hdr.value = email;
        el.push_back(hdr);
      }
      else
      {
        smtp_header_item hdr;
        hdr.type = type;
        hdr.name = rfc822_headers[type].text;
        hdr.value = (rfc822_headers[type].enc ? encodeHeaderLine(value.c_str()) : value);
        el.push_back(hdr);
      }
      return *this;
    }

    /** Add custom header
     *
     * @param name The header name.
     * @param value The header value.
     */
    smtp_headers &addCustom(const String &name, const String &value)
    {
      smtp_header_item hdr;
      hdr.type = rfc822_custom;
      hdr.name = name;
      hdr.value = value;
      el.push_back(hdr);
      return *this;
    }

    /** Clear all RFC822 headers
     */
    smtp_headers &clear()
    {
      el.clear();
      return *this;
    }
    /** Provides size of headers
     */
    size_t size() const { return el.size(); }
  };

  struct smtp_attachment
  {
    friend class SMTPMessage;
    friend class SMTPSend;
    friend class SMTPBase;

  private:
    NumString numString;
    std::vector<Attachment> el;
    Attachment att;
    uint32_t parent_addr =0;
    int attachments_idx = 0, attachment_idx = 0, inline_idx = 0, parallel_idx = 0;

    void setParent(uint32_t addr) { parent_addr = addr; }

    void push_back(Attachment att) { el.push_back(att); }

    Attachment &operator[](int index)
    {
      if (index < (int)size())
        return el[index];
      return att;
    }

    bool exists(smtp_attach_type type) { return count(type) > 0; }

    int count(smtp_attach_type type)
    {
      int count = 0;
      for (int i = 0; i < (int)size(); i++)
      {
        if (el[i].type == type)
          count++;
      }
      return count;
    }

    void inlineToAttachment()
    {
      for (int i = 0; i < (int)size(); i++)
      {
        if (el[i].type == attach_type_inline)
          el[i].type = attach_type_attachment;
      }
    }

    void resetIndex()
    {
      attachments_idx = 0;
      attachment_idx = 0;
      inline_idx = 0;
      parallel_idx = 0;
      for (int i = 0; i < (int)size(); i++)
      {
        el[i].data_index = 0;
        el[i].data_size = 0;
      }
    }

  public:
    size_t size() const { return el.size(); }

    /** Clear all attachments
     */
    void clear() { el.clear(); }

    /** Clear attachments by type
     * @param type smtp_attach_type enum.
     */
    void clearAttachments(smtp_attach_type type)
    {
      for (int i = (int)size() - 1; i >= 0; i--)
      {
        if (el[i].type == type)
          el.erase(el.begin() + i);
      }
    }

    /** Add attachment
     *
     * @param att The Attachment object.
     * @param type The smtp_attach_type enum.
     */
    void add(Attachment &att, smtp_attach_type type)
    {
      if (type == attach_type_inline)
        att.cid = numString.get(random(2000, 4000));
      att.type = type;
      el.push_back(att);
    }
  };

  class SMTPMessage
  {
    friend class smtp_message_body_t;
    friend class SMTPClient;

  public:
    /** SMTPMessage class constructor
     */
    SMTPMessage()
    {
      text.contentType("text/plain");
      html.contentType("text/html");
    }

    /** SMTPMessage class deconstructor
     */
    ~SMTPMessage() { clear(); };

    /** Clear message
     */
    void clear()
    {
      text.clear();
      html.clear();
      headers.clear();
      attachments.clear();
      content_types.clear();
    }

    /** Add RFC 822 message
     *
     * @param msg The SMTPMessage object.
     * @param name The name of message.
     * @param filename The file name of message.
     */
    void addMessage(SMTPMessage &msg, const String &name = "msg.eml", const String &filename = "msg.eml")
    {
      rfc822.push_back(msg);
      rfc822[rfc822.size() - 1].rfc822_name = name;
      rfc822[rfc822.size() - 1].rfc822_filename = filename;
    }

    // The text version message.
    TextMessage text;

    // The html version message.
    HtmlMessage html;

    // UNIX timestamp for message
    uint32_t timestamp = 0;

    // RFC 822 headers
    smtp_headers headers;

    // Attachments
    smtp_attachment attachments;

  private:
    friend class SMTPSend;
    friend class SMTPBase;

    String buf, header, rfc822_name, rfc822_filename;
#if defined(ENABLE_FS)
    File file;
#endif
    uint8_t recipient_index = 0, cc_index = 0, bcc_index = 0;
    bool send_recipient_complete = false;
    int send_state = smtp_send_state_undefined, send_state_root = smtp_send_state_undefined;
    int rfc822_idx = 0;
    SMTPMessage *parent = nullptr;
    std::vector<SMTPMessage> rfc822;
    bool file_opened = false;
    std::vector<content_type_data> content_types;

    void resetIndex()
    {
      rfc822_idx = 0;
      attachments.resetIndex();
    }
#if defined(ENABLE_FS)
    void openFileRead(bool html)
    {
      if (html)
        this->html.openFileRead(file, file_opened);
      else
        this->text.openFileRead(file, file_opened);
    }
#endif

    void beginSource(bool html)
    {
      String enc;
#if defined(ENABLE_FS)
      if (html)
        this->html.beginSource(enc, file, file_opened);
      else
        this->text.beginSource(enc, file, file_opened);
#else
      if (html)
        this->html.beginSource(enc);
      else
        this->text.beginSource(enc);
#endif

      if (enc.length())
        setXEnc(html ? this->html.xenc : this->text.xenc, enc);
    }

    void setXEnc(smtp_content_xenc &xenc, const String &enc)
    {
      if (strcmp(enc.c_str(), "binary") == 0)
        xenc = xenc_binary;
      else if (strcmp(enc.c_str(), "8bit") == 0)
        xenc = xenc_8bit;
      else if (strcmp(enc.c_str(), "7bit") == 0)
        xenc = xenc_7bit;
      else if (strcmp(enc.c_str(), "base64") == 0)
        xenc = xenc_base64;
      else if (strcmp(enc.c_str(), "quoted-printable") == 0)
        xenc = xenc_qp;
    }

    String getEnc(smtp_content_xenc xenc)
    {
      switch (xenc)
      {
      case xenc_binary:
        return "binary";
      case xenc_8bit:
        return "8bit";
      case xenc_7bit:
        return "7bit";
      case xenc_base64:
        return "base64";
      case xenc_qp:
        return "quoted-printable";
      default:
        return "";
      }
    }
  };
}
#endif
#endif