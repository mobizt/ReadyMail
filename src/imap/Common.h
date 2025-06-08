/*
 * SPDX-FileCopyrightText: 2025 Suwatchai K. <suwatchai@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef IMAP_COMMON_H
#define IMAP_COMMON_H
#if defined(ENABLE_IMAP)
#include <Arduino.h>
#if defined(ENABLE_IMAP_APPEND)
#include "smtp/SMTPClient.h"
#endif

#define IMAP_ERROR_RESPONSE -100
#define IMAP_ERROR_NO_MAILBOX -101
#define IMAP_ERROR_INVALID_SEARCH_CRITERIA -102
#define IMAP_ERROR_MODSEQ_WAS_NOT_SUPPORTED -103
#define IMAP_ERROR_IDLE_NOT_SUPPORTED -104
#define IMAP_ERROR_MESSAGE_NOT_EXISTS -105
#define IMAP_ERROR_PROCESSING -106
#define IMAP_ERROR_MAILBOX_NOT_EXISTS -107
#define IMAP_ERROR_NO_CALLBACK -108
#define IMAP_ERROR_COMMAND_NOT_ALLOW -109

#define DEFAULT_IDLE_TIMEOUT 8 * 60 * 1000

using namespace ReadyMailCallbackNS;

namespace ReadyMailIMAP
{
    enum imap_function_return_code
    {
        function_return_undefined,
        function_return_continue,
        function_return_success,
        function_return_failure,
        function_return_exit
    };

    enum imap_state
    {
        imap_state_prompt,
        imap_state_initial_state,
        imap_state_greeting,
        imap_state_start_tls,
        imap_state_start_tls_ack,
        imap_state_authentication,
        imap_state_login_user,
        imap_state_auth_plain,
        imap_state_auth_plain_next,
        imap_state_auth_login,
        imap_state_auth_xoauth2,
        imap_state_auth_xoauth2_next,
        imap_state_login_psw,
        imap_state_list,
        imap_state_select,
        imap_state_examine,
        imap_state_close,
        imap_state_search,
        imap_state_fetch,
        imap_state_fetch_envelope,
        imap_state_fetch_body_part,
        imap_state_logout,
        imap_state_idle,
        imap_state_idle_fetch,
        imap_state_idle_add,
        imap_state_idle_remove,
        imap_state_done,
        imap_state_append,
        imap_state_append_init,
        imap_state_append_last,
        imap_state_id,
        imap_state_unselect,
        imap_state_copy,
        imap_state_send_command,
        imap_state_stop
    };

    enum imap_mailbox_mode
    {
        mailbox_mode_examine,
        mailbox_mode_select
    };

    enum imap_response_types
    {
        imap_response_undefined,
        imap_response_ok,
        imap_response_no,
        imap_response_bad
    };

    enum imap_read_caps_enum
    {
        imap_read_cap_imap4,
        imap_read_cap_imap4rev1,
        // rfc2177
        imap_read_cap_idle,
        imap_read_cap_literal_plus,
        imap_read_cap_literal_minus,
        imap_read_cap_multiappend,
        imap_read_cap_uidplus,
        // rfc4314
        imap_read_cap_acl,
        imap_read_cap_binary,
        imap_read_cap_logindisable,
        // rfc6851
        imap_read_cap_move,
        // rfc2087
        imap_read_cap_quota,
        // rfc2342
        imap_read_cap_namespace,
        // rfc5161
        imap_read_cap_enable,
        // rfc2971
        imap_read_cap_id,
        imap_read_cap_unselect,
        imap_read_cap_children,
        // rfc7162 (rfc4551 obsoleted)
        imap_read_cap_condstore,
        imap_read_cap_auto_caps,
        imap_read_cap_max_type
    };

    enum imap_envelope_enum
    {
        imap_envelpe_date,
        imap_envelpe_subject,
        imap_envelpe_from,
        imap_envelpe_sender,
        imap_envelpe_reply_to,
        imap_envelpe_to,
        imap_envelpe_cc,
        imap_envelpe_bcc,
        imap_envelpe_in_reply_to,
        imap_envelpe_message_id,
        imap_envelpe_max_type
    };

    enum imap_server_status
    {
        server_status_unknown,
        server_status_ok,
        server_status_no,
        server_status_bad
    };

    enum imap_char_encoding_types
    {
        imap_char_encoding_utf8,
        imap_char_encoding_iso_8859_1,
        imap_char_encoding_iso_8859_11,
        imap_char_encoding_tis_620,
        imap_char_encoding_windows_874,
        imap_char_encoding_max_type
    };

    enum imap_char_encoding_scheme
    {
        imap_char_encoding_scheme_default = -1,
        imap_char_encoding_scheme_utf_8,
        imap_char_encoding_scheme_iso8859_1,
        imap_char_encoding_scheme_iso8859_11,
        imap_char_encoding_scheme_tis_620,
        imap_char_encoding_scheme_windows_874
    };

    enum imap_transfer_encoding_scheme
    {
        imap_transfer_encoding_undefined,
        imap_transfer_encoding_base64,
        imap_transfer_encoding_7bit,
        imap_transfer_encoding_8bit,
        imap_transfer_encoding_binary,
        imap_transfer_encoding_quoted_printable
    };

    enum imap_auth_caps_enum
    {
        imap_auth_cap_plain,
        imap_auth_cap_xoauth2,
        imap_auth_cap_cram_md5,
        imap_auth_cap_digest_md5,
        imap_auth_cap_login,
        imap_auth_cap_starttls,
        // imap rfc4959
        imap_auth_cap_sasl_ir,

        imap_auth_cap_max_type,
    };

    enum imap_body_structure_non_multipart_fields
    {
        // basic fields of non-multipart body part
        non_multipart_field_type,           // text, application, image, audio, video
        non_multipart_field_subtype,        // plain, html etc.
        non_multipart_field_parameter_list, // charset, name (key sp value ...)
        non_multipart_field_content_id,     // content ID for inline image
        non_multipart_field_description,
        non_multipart_field_encoding, // content transfer encoding
        non_multipart_field_size,     // octets/transfer size

        non_multipart_field_lines_or_md5, // it's the number of lines for basic field or md5 for extension fields
                                          // disposition type (attachment/inline) followed by lists (key sp value ...) of disposition params
                                          // e.g. filename, name, creation-date, modification-date, size (unencoded) etc.
        non_multipart_field_disposition,
        non_multipart_field_language,
        non_multipart_field_location,
        non_multipart_field_max_type
    };

    enum imap_body_structure_multipart_fields // multipart/mixed, multipart/related, multipart/alternative
    {
        // basic fields of multipart body part
        multipart_field_type,           // multipart
        multipart_field_subtype,        // mixed, related, alternative
                                        // extension fields of multipart body part
        multipart_field_parameter_list, // (key sp value ...)
        multipart_field_disposition,
        multipart_field_language,
        multipart_field_location,
        multipart_field_max_type
    };

    enum imap_body_structure_part_type
    {
        // non-multipart
        imap_body_structure_text,
        imap_body_structure_image,
        imap_body_structure_application,
        imap_body_structure_audio,
        imap_body_structure_video,
        imap_body_structure_parallel,
        imap_body_structure_digest,
        // multipart
        imap_body_structure_related,
        imap_body_structure_alternative,
        imap_body_structure_mixed
    };

    enum imap_fetch_mode
    {
        imap_fetch_envelope,
        imap_fetch_body_part
    };

    enum imap_data_callback_event
    {
        imap_data_event_undefined,
        imap_data_event_search,
        imap_data_event_fetch_envelope,
        imap_data_event_fetch_body
    };

    __attribute__((used)) struct
    {
        bool operator()(uint32_t a, uint32_t b) const { return a > b; }
    } compareMore;

    struct imap_auth_cap_t
    {
        char text[20];
    };

    struct imap_read_cap_t
    {
        char text[15];
    };

    struct imap_envelope_t
    {
        char text[12];
    };

    struct imap_char_encoding_t
    {
        char text[12];
    };

    const struct imap_envelope_t imap_envelopes[imap_envelpe_max_type] PROGMEM = {"Date", "Subject", "From", "Sender", "Reply-To", "To", "Cc", "Bcc", "In-Reply-To", "Message-ID"};
    const struct imap_auth_cap_t imap_auth_cap_token[imap_auth_cap_max_type] PROGMEM = {"AUTH=PLAIN", "AUTH=XOAUTH2", "AUTH=CRAM-MD5", "AUTH=DIGEST-MD5", "AUTH=LOGIN", "STARTTLS", "SASL-IR"};
    const struct imap_read_cap_t imap_read_cap_token[imap_read_cap_max_type] PROGMEM = {"IMAP4", "IMAP4rev1", "IDLE", "LITERAL+", "LITERAL-", "MULTIAPPEND", "UIDPLUS", "ACL", "BINARY", "LOGINDISABLED", "MOVE", "QUOTA", "NAMESPACE", "ENABLE", "ID", "UNSELECT", "CHILDREN", "CONDSTORE", "" /* Auto cap */};
    const struct imap_char_encoding_t imap_char_encodings[imap_char_encoding_max_type] PROGMEM = {"utf-8", "iso-8859-1", "iso-8859-11", "tis-620", "windows-874"};

    struct imap_state_info
    {
        imap_state state = imap_state_prompt;
        imap_state target = imap_state_prompt;
    };

    typedef struct imap_response_status_t
    {
        int errorCode = 0;
        imap_state state = imap_state_prompt;
        String text;
    } IMAPStatus;

    typedef struct imap_connand_response_status_t
    {
        int errorCode = 0;
        bool isComplete = false;
        String command, text;
    } IMAPCommandResponse;

    struct imap_server_status_t
    {
        bool start_tls = false, connected = false, secured = false, server_greeting_ack = false, authenticated = false;
        imap_state_info state_info;
        imap_function_return_code ret = function_return_undefined;
    };

    typedef void (*IMAPResponseCallback)(IMAPStatus status);

    typedef void (*IMAPCustomComandCallback)(IMAPCommandResponse response);

    typedef void (*IMAPTextDecodingCallback)(const String &charset, const uint8_t *in, int inSize, uint8_t *out, int &outSize);

    struct imap_timeout
    {
        unsigned long con = 1000 * 3, send = 1000 * 30, read = 1000 * 120;
        unsigned long mailbox_selected = 0, idle = DEFAULT_IDLE_TIMEOUT;
    };

    struct imap_options
    {
        imap_timeout timeout;
        size_t search_limit = 20;
        bool recent_sort = true, read_only_mode = true, mailbox_selected = false, mailboxes_updated = false;
        uint32_t fetch_number;
        int32_t modsequence = -1;
        uint32_t part_size_limit = 1024 * 1024;
        bool uid_search = false, uid_fetch = false, searching = false, processing = false, idling = false, multiline = false, await = false;
        bool use_auto_client = false;
    };

    // body part field item
    struct body_part_field
    {
        String token;
        int depth = 0;
        int index = 0;
    };

    // body part context
    struct part_ctx
    {
        std::vector<body_part_field> field;
        String name, section, id, parent_id;
        uint8_t num_specifier = 0, part_count = 0;
        int depth = 0, field_index = 0;
    };

    struct imap_file_info
    {
        String filename, mime, charset, transferEncoding;
        uint32_t fileSize = 0;
    };

    struct imap_file_chunk
    {
        uint8_t *data = nullptr;
        uint16_t size = 0;
        uint32_t index = 0;
        bool isComplete = false;
    };

    struct imap_file_progress
    {
        int value = 0, last_value = -1;
        bool available = false;
    };

    struct imap_file_ctx
    {
        friend class IMAPSend;

    public:
        imap_file_info info;
        imap_file_chunk chunk;
        imap_file_progress progress;
        bool fetch = true;
#if defined(ENABLE_FS)
        FileCallback fileCallback = NULL;
        String downloadFolder;
#endif
        IMAPTextDecodingCallback textEncCb = NULL;

    private:
        friend class IMAPParser;
        String section, filepath;
        uint32_t octet_count = 0, total_read = 0, decoded_len = 0 /* The sum of the decoded octet */;
        imap_transfer_encoding_scheme transfer_encoding = imap_transfer_encoding_undefined;
        imap_char_encoding_scheme char_encoding = imap_char_encoding_scheme_default;
        bool text_part = false, last_octet = false /* last octet bytes ')\r\n' found */;
    };

    // message context
    struct imap_msg_ctx
    {
        int cur_file_index = 0, fetch_count = 0;
        std::vector<std::pair<String, String>> headers;
        std::vector<imap_file_ctx> files;
        String raw_chunk, qp_chunk;
        bool exists = false;
    };

    class IMAPCallbackData
    {
        friend class IMAPParser;
        friend class IMAPBase;
        friend class IMAPSend;

    public:
        /**
         * Provides the event of callback data
         *
         * @return imap_data_callback_event enum of current operation i.g. imap_data_event_search,
         * imap_data_event_fetch_envelope and imap_data_event_fetch_body.
         */
        imap_data_callback_event event() { return eventType; }

        /**
         * Provides the size of message headers.
         *
         * @return size of message headers.
         */
        size_t headerCount() { return headers->size(); }

        /**
         * Provides a message header at index.
         *
         * @param index The index.
         * @return key-value pair of message header.
         */
        std::pair<String, String> getHeader(int index) { return (*headers)[index]; }

        /**
         * Provides the number of files contains in the messages (included text and message file types).
         *
         * @return number of files.
         */
        size_t fileCount() { return files->size(); }

        /**
         * Provides a file info data at index..
         *
         * @param index Optional. The index.
         * Provides -1 or leave default to get current file info data.
         * @return imap_file_info struct data i.e. filename, mime, charset, transferEncoding and fileSize.
         */
        imap_file_info fileInfo(int index = -1) { return getFile(index).info; }

        /**
         * Provides a file chunk data at index..
         *
         * @param index  Optional. The index.
         * Provides -1 or leave default to get current file chunk data.
         * @return imap_file_chunk struct data i.e. data (uint8_t *), index, size and isComplete.
         */
        imap_file_chunk fileChunk(int index = -1) { return getFile(index).chunk; }

        /**
         * Provides the file progress data at index.
         *
         * @param index  Optional. The index.
         * Provides -1 or leave default to get current file progress.
         * @return imap_file_progress struct data i.e. value and available.
         */
        imap_file_progress fileProgress(int index = -1) { return getFile(index).progress; }

        /**
         * Provides the fetch option at file index.
         *
         * @param index The index.
         * @return file fetch option at index.
         */
        bool &fetchOption(int index) { return getFile(index).fetch; }

#if defined(ENABLE_FS)
        /**
         * Provides the file callback and folder to download file at file index.
         *
         * @param index The index.
         * @param fileCallback The FileCallback callback function that provides the file openning and removing operations for file/attachment download.
         * @param downloadFolder The name of folder that stores the downloaded files.
         */
        void setFileCallback(int index, FileCallback fileCallback, const String &downloadFolder = "")
        {
            getFile(index).fileCallback = fileCallback;
            getFile(index).downloadFolder = downloadFolder;
        }
#endif

        /**
         * Provides the text encoding callback at file index.
         *
         * @param index The index.
         * @param callback The IMAPTextDecodingCallback callback function that performs the text encoding.
         */
        void setTextEncodingCallback(int index, IMAPTextDecodingCallback callback) { getFile(index).textEncCb = callback; }

        /**
         * Provides current message index from search result.
         * @return The message index.
         */
        int messageIndex() { return *msgIndex; }

        /**
         * Provides the total messages found from search.
         *
         * @return The number of message found.
         */
        int messageFound() { return msgFound; }

        /**
         * Provides the total messages store in the search result.
         *
         * @return The number of message found.
         */
        int messageAvailable() { return msgNums.size(); }

        /**
         * Provides the message number or UID at the index.
         *
         * @param index Optional. The index of message in search result list.
         * Provides -1 or leave default to get current message.
         * @return The number or UID of message at index..
         */
        uint32_t messageNum(int index = -1) { return msgNums[index > -1 ? index : *msgIndex]; }

    private:
        int *fileIndex = nullptr;
        int *msgIndex = nullptr;
        int msgFound = 0;
        std::vector<uint32_t> msgNums;
        imap_data_callback_event eventType = imap_data_event_undefined;

        std::vector<imap_file_ctx> *files = nullptr;
        std::vector<std::pair<String, String>> *headers = nullptr;
        imap_file_ctx &getFile(int index = -1) { return (*files)[index > -1 ? index : *fileIndex]; }
    };

    typedef void (*IMAPDataCallback)(IMAPCallbackData &data);

    struct imap_callback
    {
        IMAPResponseCallback resp = NULL;
        IMAPDataCallback data = NULL;
        IMAPCustomComandCallback cmd = NULL;
        String download_path;
        IMAPCommandResponse command_response;
#if defined(ENABLE_FS)
        FileCallback file = NULL;
#endif
    };

    struct imap_context
    {
#if defined(ENABLE_FS)
        File file;
#endif
        Client *client = nullptr;
#if defined(ENABLE_READYCLIENT)
        ReadyClient *auto_client = nullptr;
#endif
        String tag = "ReadyMail", cmd, current_mailbox;
        uint32_t ts = 0;
        int cur_msg_index = 0;
        bool auth_caps[imap_auth_cap_max_type], feature_caps[imap_read_cap_max_type];
        std::vector<imap_msg_ctx> messages;
        imap_options options;
        IMAPCallbackData cb_data;
        imap_callback cb;
        IMAPStatus *status = nullptr;
        imap_server_status_t *server_status = nullptr;
        imap_response_types resp_type = imap_response_undefined;
        std::vector<std::array<String, 3>> *mailboxes = nullptr;
        String idle_status;
        bool idle_available = false;
        bool ssl_mode = false;
        bool auth_mode = true;
        uint32_t current_message = 0;
#if defined(ENABLE_IMAP_APPEND)
        SMTPClient *smtp = nullptr;
        SMTPMessage msg;
        smtp_context *smtp_ctx = nullptr;
#endif
    };

}

#endif
#endif