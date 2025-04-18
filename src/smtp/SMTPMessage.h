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
    String mime;
    String boundary;
    content_type_data(const String &mime)
    {
      this->mime = mime;
      if (mime == "alternative" || mime == "related" || mime == "mixed" || mime == "parallel")
        boundary = getMIMEBoundary(15);
    }
    String getMIMEBoundary(size_t len)
    {
      String tmp = boundary_map;
      char *buf = (char *)malloc(len + 1);
      if (len)
      {
        --len;
        buf[0] = tmp[0];
        buf[1] = tmp[1];
        for (size_t n = 2; n < len; n++)
        {
          int key = rand() % (int)(tmp.length() - 1);
          buf[n] = tmp[key];
        }
        buf[len] = '\0';
      }
      String s = buf;
      free(buf);
      buf = nullptr;
      return s;
    }
  };

  class SMTPMessage
  {
  public:
    SMTPMessage()
    {
      text.content_type = "text/plain";
      html.content_type = "text/html";
    }
    ~SMTPMessage() { clear(); };
    void clear()
    {
      sClear(sender.name);
      sClear(sender.email);
      sClear(subject);
      sClear(text.charSet);
      sClear(text.content);
      sClear(text.content_type);
      text.embed.enable = false;
      sClear(html.charSet);
      sClear(html.content);
      sClear(html.content_type);
      html.embed.enable = false;
      sClear(response.reply_to);
      response.notify = message_notify_never;
      priority = message_priority_normal;
      recipients.clear();
      ccs.clear();
      bccs.clear();
      headers.clear();
      attachments.clear();
      content_types.clear();
    }
    void clearInlineimages()
    {
      for (int i = (int)attachments.size() - 1; i >= 0; i--)
      {
        if (attachments[i].type == attach_type_inline)
          attachments.erase(attachments.begin() + i);
      }
    }
    void clearAttachments()
    {
      for (int i = (int)attachments.size() - 1; i >= 0; i--)
      {
        if (attachments[i].type == attach_type_attachment)
          attachments.erase(attachments.begin() + i);
      }
    }
    void clearRecipients() { recipients.clear(); };
    void clearCc() { ccs.clear(); };
    void clearBcc() { bccs.clear(); };
    void clearHeader() { headers.clear(); };
    void addAttachment(Attachment &att)
    {
      att.type = attach_type_attachment;
      attachments.push_back(att);
    }
    void addParallelAttachment(Attachment &att)
    {
      att.type = attach_type_parallel;
      attachments.push_back(att);
    }
    void addInlineImage(Attachment &att)
    {
      att.type = attach_type_inline;
      att.cid = random(2000, 4000);
      attachments.push_back(att);
    }
    void addMessage(SMTPMessage &msg, const String &name = "msg.eml", const String &filename = "msg.eml")
    {
      rfc822.push_back(msg);
      rfc822[rfc822.size() - 1].rfc822_name = name;
      rfc822[rfc822.size() - 1].rfc822_filename = filename;
    }
    void addRecipient(const String &name, const String &email)
    {
      smtp_mail_address_t rcp;
      rcp.name = name;
      rcp.email = email;
      recipients.push_back(rcp);
    }
    void addCc(const String &email)
    {
      smtp_mail_address_t cc;
      cc.email = email;
      ccs.push_back(cc);
    }
    void addBcc(const String &email)
    {
      smtp_mail_address_t bcc;
      bcc.email = email;
      bccs.push_back(bcc);
    }
    void addHeader(const String &hdr) { headers.push_back(hdr); }

    // The message author config.
    smtp_mail_address_t author;

    // The message sender (agent or teansmitter) config.
    smtp_mail_address_t sender;

    // The topic of message.
    String subject;

    // The PLAIN text message.
    TextMessage text;

    // The HTML text message.
    HtmlMessage html;

    // The response config.
    message_response_t response;

    // The priority of the message.
    smtp_priority priority = message_priority_normal;

    // The message from config.
    smtp_mail_address_t from;

    // The message identifier.
    String messageID;

    // The keywords or phrases, separated by commas.
    String keywords;

    // The comments about message.
    String comments;

    // The date of message.
    String date;

    // The field that contains the parent's message ID of the message to which this one is a reply.
    String in_reply_to;

    // The field that contains the parent's references (if any) and followed by the parent's message ID (if any) of the message to which this one is a reply.
    String references;

    // UNIX timestamp for message
    uint32_t timestamp = 0;

  private:
    friend class MailClient;
    friend class SMTPSend;
    friend class SMTPBase;

    int attachments_idx = 0, attachment_idx = 0, inline_idx = 0, parallel_idx = 0, rfc822_idx = 0;
    String buf, header, rfc822_name, rfc822_filename;
#if defined(ENABLE_FS)
    File file;
#endif
    uint8_t recipient_index = 0, cc_index = 0, bcc_index = 0;
    bool send_recipient_complete = false;
    int send_state = smtp_send_state_undefined, send_state_root = smtp_send_state_undefined;
    SMTPMessage *parent = nullptr;
    std::vector<smtp_mail_address_t> recipients;
    std::vector<smtp_mail_address_t> ccs;
    std::vector<smtp_mail_address_t> bccs;
    std::vector<String> headers;
    std::vector<Attachment> attachments;
    std::vector<SMTPMessage> rfc822;
    bool file_opened = false;
    std::vector<content_type_data> content_types;

    bool hasAttachment(smtp_attach_type type) { return attachmentCount(type) > 0; }
    int attachmentCount(smtp_attach_type type)
    {
      int count = 0;
      for (int i = 0; i < (int)attachments.size(); i++)
      {
        if (attachments[i].type == type)
          count++;
      }
      return count;
    }

    void inlineToAttachment()
    {
      for (int i = 0; i < (int)attachments.size(); i++)
      {
        if (attachments[i].type == attach_type_inline)
          attachments[i].type = attach_type_attachment;
      }
    }

    void resetAttachmentIndex()
    {
      attachments_idx = 0;
      attachment_idx = 0;
      inline_idx = 0;
      parallel_idx = 0;
      rfc822_idx = 0;
      for (int i = 0; i < (int)attachments.size(); i++)
      {
        attachments[i].data_index = 0;
        attachments[i].data_size = 0;
      }
    }
    void sClear(String &s) { s.remove(0, s.length()); }
  };
}
#endif
#endif