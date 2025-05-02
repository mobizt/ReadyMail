#ifndef SMTP_COMMON_H
#define SMTP_COMMON_H
#if defined(ENABLE_SMTP)
#include <Arduino.h>

#define SMTP_ERROR_RESPONSE -100
#define SMTP_ERROR_INVALID_SENDER_EMAIL -103
#define SMTP_ERROR_INVALID_RECIPIENT_EMAIL -104
#define SMTP_ERROR_SEND_HEADER -105
#define SMTP_ERROR_SEND_BODY -106
#define SMTP_ERROR_SEND_DATA -107
#define SMTP_ERROR_PROCESSING -108

using namespace ReadyMailCallbackNS;

namespace ReadyMailSMTP
{

    static const char boundary_map[65] = "=_abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char *smtp_months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    enum smtp_function_return_code
    {
        function_return_undefined,
        function_return_continue,
        function_return_success,
        function_return_failure,
        function_return_exit
    };

    enum smtp_state
    {
        smtp_state_prompt,
        smtp_state_initial_state,
        smtp_state_greeting,
        smtp_state_start_tls,
        smtp_state_start_tls_ack,
        smtp_state_authentication,
        smtp_state_login_user,
        smtp_state_auth_plain,
        smtp_state_auth_login,
        smtp_state_auth_xoauth2,
        smtp_state_login_psw,
        smtp_state_send_header_sender,
        smtp_state_send_header_recipient,
        smtp_state_wait_data,
        smtp_state_send_body,
        smtp_state_data_termination,
        smtp_state_terminated
    };

    enum smtp_send_state
    {
        smtp_send_state_undefined,
        smtp_send_state_body_data,
        smtp_send_state_body_header,
        smtp_send_state_message_data,
        smtp_send_state_body_type_1 = 100,
        smtp_send_state_body_type_2 = 200,
        smtp_send_state_body_type_2_1,
        smtp_send_state_body_type_2_2,
        smtp_send_state_body_type_2_3,
        smtp_send_state_body_type_2_4,
        smtp_send_state_body_type_2_5,
        smtp_send_state_body_type_2_6,
        smtp_send_state_body_type_2P = 250,
        smtp_send_state_body_type_2P_1,
        smtp_send_state_body_type_2P_2,
        smtp_send_state_body_type_2P_3,
        smtp_send_state_body_type_2P_4,
        smtp_send_state_body_type_2P_5,
        smtp_send_state_body_type_2P_6,
        smtp_send_state_body_type_2P_7,
        smtp_send_state_body_type_2P_8,
        smtp_send_state_body_type_3 = 300,
        smtp_send_state_body_type_3_1,
        smtp_send_state_body_type_3_2,
        smtp_send_state_body_type_3_3,
        smtp_send_state_body_type_3_4,
        smtp_send_state_body_type_3P = 350,
        smtp_send_state_body_type_3P_1,
        smtp_send_state_body_type_3P_2,
        smtp_send_state_body_type_3P_3,
        smtp_send_state_body_type_3P_4,
        smtp_send_state_body_type_3P_5,
        smtp_send_state_body_type_3P_6,
        smtp_send_state_body_type_4 = 400,
        smtp_send_state_body_type_4_1,
        smtp_send_state_body_type_4_2,
        smtp_send_state_body_type_4_3,
        smtp_send_state_body_type_4_4,
        smtp_send_state_body_type_4P = 450,
        smtp_send_state_body_type_4P_1,
        smtp_send_state_body_type_4P_2,
        smtp_send_state_body_type_4P_3,
        smtp_send_state_body_type_4P_4,
        smtp_send_state_body_type_4P_5,
        smtp_send_state_body_type_4P_6,
        smtp_send_state_body_type_5 = 500,
        smtp_send_state_body_type_5_1,
        smtp_send_state_body_type_5_2,
        smtp_send_state_body_type_5P = 550,
        smtp_send_state_body_type_5P_1,
        smtp_send_state_body_type_5P_2,
        smtp_send_state_body_type_5P_3,
        smtp_send_state_body_type_5P_4,
        smtp_send_state_body_type_6 = 600,
        smtp_send_state_body_type_6_1,
        smtp_send_state_body_type_6_2,
        smtp_send_state_body_type_6_3,
        smtp_send_state_body_type_6_4,
        smtp_send_state_body_type_7 = 700,
        smtp_send_state_body_type_7_1,
        smtp_send_state_body_type_7_2,
        smtp_send_state_body_type_8 = 800,
        smtp_send_state_body_type_8_1,
        smtp_send_state_body_type_8_2
    };

    /* The SMTP status codes enum */
    enum smtp_server_status_code
    {
        smtp_server_status_code_0, // default

        /* Positive Completion */
        smtp_server_status_code_211 = 211, // System status, or system help reply
        smtp_server_status_code_214 = 214, // Help message(A response to the HELP command)
        smtp_server_status_code_220 = 220, //<domain> Service ready
        smtp_server_status_code_221 = 221, //<domain> Service closing transmission channel [RFC 2034]
        smtp_server_status_code_235 = 235, // 2.7.0 Authentication succeeded[RFC 4954]
        smtp_server_status_code_250 = 250, // Requested mail action okay, completed
        smtp_server_status_code_251 = 251, // User not local; will forward
        smtp_server_status_code_252 = 252, // Cannot verify the user, but it will
                                           // try to deliver the message anyway

        /* Positive Intermediate */
        smtp_server_status_code_334 = 334, //(Server challenge - the text part
                                           // contains the Base64 - encoded
                                           // challenge)[RFC 4954]
        smtp_server_status_code_354 = 354, // Start mail input

        /* Transient Negative Completion */
        /* "Transient Negative" means the error condition is temporary, and the action
         may be requested again.*/
        smtp_server_status_code_421 = 421, // Service is unavailable because the server is shutting down.
        smtp_server_status_code_432 = 432, // 4.7.12 A password transition is needed [RFC 4954]
        smtp_server_status_code_450 = 450, // Requested mail action not taken: mailbox unavailable (e.g.,
                                           // mailbox busy or temporarily blocked for policy reasons)
        smtp_server_status_code_451 = 451, // Requested action aborted : local error in processing
        // e.g.IMAP server unavailable[RFC 4468]
        smtp_server_status_code_452 = 452, // Requested action not taken : insufficient system storage
        smtp_server_status_code_454 = 454, // 4.7.0 Temporary authentication failure[RFC 4954]
        smtp_server_status_code_455 = 455, // Server unable to accommodate parameters

        /* Permanent Negative Completion */
        smtp_server_status_code_500 = 500, // Syntax error, command unrecognized
                                           // (This may include errors such as
                                           // command line too long)
        // e.g. Authentication Exchange line is too long [RFC 4954]
        smtp_server_status_code_501 = 501, // Syntax error in parameters or arguments
        // e.g. 5.5.2 Cannot Base64-decode Client responses [RFC 4954]
        // 5.7.0 Client initiated Authentication Exchange (only when the SASL
        // mechanism specified that client does not begin the authentication exchange)
        // [RFC 4954]
        smtp_server_status_code_502 = 502, // Command not implemented
        smtp_server_status_code_503 = 503, // Bad sequence of commands
        smtp_server_status_code_504 = 504, // Command parameter is not implemented
        // e.g. 5.5.4 Unrecognized authentication type [RFC 4954]
        smtp_server_status_code_521 = 521, // Server does not accept mail [RFC 7504]
        smtp_server_status_code_523 = 523, // Encryption Needed [RFC 5248]
        smtp_server_status_code_530 = 530, // 5.7.0 Authentication required [RFC 4954]
        smtp_server_status_code_534 = 534, // 5.7.9 Authentication mechanism is too weak [RFC 4954]
        smtp_server_status_code_535 = 535, // 5.7.8 Authentication credentials invalid [RFC 4954]
        smtp_server_status_code_538 = 538, // 5.7.11 Encryption required for
                                           // requested authentication mechanism[RFC
                                           // 4954]
        smtp_server_status_code_550 = 550, // Requested action not taken: mailbox unavailable (e.g., mailbox not
                                           // found, no access, or command rejected for policy reasons)
        smtp_server_status_code_551 = 551, // User not local; please try <forward-path>
        smtp_server_status_code_552 = 552, // Requested mail action aborted: exceeded storage allocation
        smtp_server_status_code_553 = 553, // Requested action not taken: mailbox name not allowed
        smtp_server_status_code_554 = 554, // Transaction has failed (Or, in the
                                           // case of a connection-opening response,
                                           // "No SMTP service here")
        // e.g. 5.3.4 Message too big for system [RFC 4468]
        smtp_server_status_code_556 = 556, // Domain does not accept mail[RFC 7504]
    };

    enum rfc822_header_types
    {
        rfc822_date,
        rfc822_subject,
        rfc822_from,
        rfc822_sender,
        rfc822_reply_to,
        rfc822_to,
        rfc822_cc,
        rfc822_bcc,
        rfc822_in_reply_to,
        rfc822_message_id,
        rfc822_references,
        rfc822_comments,
        rfc822_keywords,
        rfc822_custom,
        rfc822_max_type
    };

    enum smtp_auth_caps_enum
    {
        smtp_auth_cap_plain,
        smtp_auth_cap_xoauth2,
        smtp_auth_cap_cram_md5,
        smtp_auth_cap_digest_md5,
        smtp_auth_cap_login,
        smtp_auth_cap_starttls,
        smtp_auth_cap_max_type,
    };

    enum smtp_send_caps_enum
    {
        smtp_send_cap_binary_mime,
        smtp_send_cap_8bit_mime,
        smtp_send_cap_chunking,
        smtp_send_cap_utf8,
        smtp_send_cap_pipelining,
        smtp_send_cap_dsn,
        smtp_send_cap_esmtp,
        smtp_send_cap_max_type
    };

    enum smtp_attach_type
    {
        attach_type_none,
        attach_type_attachment,
        attach_type_inline,
        attach_type_parallel
    };

    enum smtp_embed_message_type
    {
        embed_message_type_attachment = 0,
        embed_message_type_inline
    };

    enum smtp_content_xenc
    {
        xenc_none,
        /* rfc2045 section 2.7 */
        xenc_7bit,
        xenc_qp,
        xenc_base64,
        /* rfc2045 section 2.8 */
        xenc_8bit,
        /* rfc2045 section 2.9 */
        xenc_binary
    };

    struct smtp_attach_file_config
    {
        String path;
#if defined(ENABLE_FS)
        FileCallback callback = NULL;
#endif
        const uint8_t *blob = nullptr;
        uint32_t blob_size = 0;
    };

    struct smtp_mail_address_t
    {
        String name;
        String email;
    };

    struct smtp_attachment_t
    {
        friend class SMTPMessage;
        friend class SMTPSend;
        friend class smtp_attachment;

    public:
        /* The name of attachment */
        String name;

        /* The attachment file name */
        String filename;

        /* The MIME type of attachment */
        String mime;

        /* The transfer encoding of attachment e.g. base64 */
        String transfer_encoding = "base64";

        /* The content encoding of attachment e.g. base64 */
        String content_encoding;

        /* The content id of attachment file */
        String content_id;

        /* The description of attachment file */
        String description;

        smtp_attach_file_config attach_file;

    private:
        smtp_attach_type type = attach_type_attachment;
        String path;
        String cid;
        smtp_content_xenc xenc = xenc_none;
        int data_index = 0, data_size = 0;
        float progress = 0, last_progress = -1;
        int msg_uid = 0;
    };

    struct smtp_transfer_encoding_t
    {
        /* The default 7-bit transfer encoding for US-ACII characters*/
        static constexpr const char *enc_7bit = "7bit";

        /* The quoted printable transfer encoding for non-US-ASCII characters*/
        static constexpr const char *enc_qp = "quoted-printable";

        /* The base64 encoded transfer encoding */
        static constexpr const char *enc_base64 = "base64";

        /* The 8-bit transfer encoding for extended-US-ASCII characters*/
        static constexpr const char *enc_8bit = "8bit";

        /* The binary transfer encoding for extended-US-ASCII characters with no line
         * length limit*/
        static constexpr const char *enc_binary = "binary";
    };

    /* The option to embed this message content as a file */
    struct embed_message_body_t
    {
        bool enable = false;
        String filename;
        smtp_embed_message_type type = embed_message_type_attachment;
    };

    struct smtp_message_body_t
    {
        friend class SMTPSend;

    public:
        /* Set the body */
        smtp_message_body_t &body(const String &body)
        {
            content = body;
            return *this;
        }

        /* Set the character set */
        smtp_message_body_t &charset(const String &charset)
        {
            charSet = charset;
            return *this;
        }

        /* Set the content type */
        smtp_message_body_t &contentType(const String &contentType)
        {
            content_type = contentType;
            return *this;
        }

        /* Set the content transfer encoding */
        smtp_message_body_t &transferEncoding(const String &enc)
        {
            transfer_encoding = enc;
            return *this;
        }

        /* Set the PLAIN text wrapping */
        smtp_message_body_t &textFlow(bool value)
        {
            flowed = value;
            return *this;
        }

        /* Set the body as a file */
        smtp_message_body_t &embedFile(bool enable, const String &filename, smtp_embed_message_type type)
        {
            embed.enable = enable;
            embed.filename = filename;
            embed.type = type;
            return *this;
        }

        smtp_message_body_t &clear()
        {
            content.remove(0, content.length());
            charSet.remove(0, charSet.length());
            content_type.remove(0, content_type.length());
            transfer_encoding.remove(0, transfer_encoding.length());
            embed.enable = false;
            flowed = false;
            return *this;
        }

    private:
        String cid;
        int data_index = 0, data_size = 0;
        String enc_text;
        smtp_content_xenc xenc = xenc_none;
        String content, charSet = "UTF-8", content_type = "text/html", transfer_encoding = "7bit";
        bool flowed = false;
        embed_message_body_t embed;
    };

    struct smtp_header_item
    {
        String name, value;
        rfc822_header_types type = rfc822_custom;
    };

    typedef struct smtp_response_status_t
    {
        int errorCode = 0, statusCode = 0, progress = 0;
        smtp_state state = smtp_state_prompt;
        bool isComplete = false, progressUpdated = false;
        String filename;
        String text;
    } SMTPStatus;

    struct smtp_state_info
    {
        smtp_state state = smtp_state_prompt;
        smtp_state target = smtp_state_prompt;
        smtp_server_status_code status_code = smtp_server_status_code_0;
        bool esmtp = false;
    };

    struct smtp_server_status_t
    {
        bool start_tls = false, connected = false, secured = false, server_greeting_ack = false, authenticated = false;
        smtp_state_info state_info;
        smtp_function_return_code ret = function_return_undefined;
    };

    struct smtp_rfc822_envelope
    {
        char text[12];
        bool multi; // multi fields
        bool enc;   // needs encode
        int8_t sub_type;
    };

    struct smtp_auth_cap_t
    {
        char text[20];
    };

    struct smtp_send_cap_t
    {
        char text[15];
    };

    const struct smtp_rfc822_envelope rfc822_headers[rfc822_max_type] PROGMEM = {{"Date", false, false, -1}, {"Subject", false, true, -1}, {"From", false, true, 0}, {"Sender", false, true, 0}, {"Reply-To", false, false, 0}, {"To", true, true, 0}, {"Cc", true, false, 1}, {"Bcc", true, false, 1}, {"In-Reply-To", false, false, -1}, {"Message-ID", false, false, -1}, {"References", false, false, -1}, {"Comments", true, false, -1}, {"Keywords", true, false, -1}, {"", true, false, -1}};
    const struct smtp_auth_cap_t smtp_auth_cap_token[smtp_auth_cap_max_type] PROGMEM = {"PLAIN", "XOAUTH2", "CRAM-MD5", "DIGEST-MD5", "LOGIN", "STARTTLS"};
    const struct smtp_send_cap_t smtp_send_cap_token[smtp_send_cap_max_type] PROGMEM = {"BINARYMIME", "8BITMIME", "CHUNKING", "SMTPUTF8", "PIPELINING", "DSN", "" /* ESMTP */};

    typedef struct smtp_attachment_t Attachment;
    typedef struct smtp_message_body_t TextMessage;
    typedef struct smtp_message_body_t HtmlMessage;
    typedef void (*SMTPResponseCallback)(SMTPStatus status);

    struct smtp_timeout
    {
        unsigned long con = 1000 * 3, send = 1000 * 30, read = 1000 * 120;
    };

    struct smtp_options
    {
        smtp_timeout timeout;
        String notify;
        bool last_append = false, ssl_mode = false, processing = false, accumulate = false, imap_mode = false;
        int level = 0, data_len = 0;
    };

    struct smtp_context
    {
        Client *client = nullptr;
        SMTPResponseCallback resp_cb = NULL;
        SMTPStatus *status = nullptr;
        smtp_server_status_t *server_status = nullptr;
        smtp_options options;
        uint32_t ts = 0;
    };

}
#endif
#endif