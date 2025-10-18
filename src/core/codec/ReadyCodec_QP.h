#ifndef READY_CODEC_QP_H
#define READY_CODEC_QP_H

#include "ReadyStream.h"
#include <stdio.h>
#include <string.h>

namespace rd
{
// Max line length is 76 characters, plus 2 for \r\n, plus 1 for \0.
#define QP_MAX_LINE_SIZE 79

    // Mode tags
    struct plain_text
    {
    };
    struct flowed_text
    {
    };

    // Compatible across AVR, ARM, ESP, etc.
    // Append a single byte to the buffer
    static inline bool append_byte_helper(char b, char *buf, size_t &len, size_t max_size)
    {
        if (len < max_size - 1)
        { // -1 for null terminator
            buf[len++] = b;
            return true;
        }
        return false;
    }

    // Append a string (sequence of bytes) to the buffer
    static inline bool append_str_helper(const char *s, size_t len_s, char *buf, size_t &len, size_t max_size)
    {
        if (len + len_s < max_size)
        {
            memcpy(buf + len, s, len_s);
            len += len_s;
            return true;
        }
        return false;
    }

    template <typename Mode>
    struct qp_traits
    {
        static int encode_chunk(src_data_ctx &src, int &index, char *out, size_t out_size)
        {
            if (out_size < QP_MAX_LINE_SIZE)
                return -1;
            bool is_flowed = (sizeof(Mode) == sizeof(flowed_text));
            src.seek(index);

            size_t current_len = 0;
            int line_len_tracker = 0;
            bool is_soft_break_added = false;
            int bytes_read = 0;

            while (src.available())
            {
                uint8_t c = src.peek();
                int bytes_to_add = 0;

                // Check for hard line break in source stream: \r or \n
                if (c == '\r' || c == '\n')
                {
                    // Consume the \r and optionally the following \n
                    src.read();
                    bytes_read++;
                    if (c == '\r' && src.peek() == '\n')
                    {
                        src.read();
                        bytes_read++;
                    }
                    break; // Ends the QP line
                }

                // Determine bytes to add for the current character
                if (c == '=' || c < 32 || c > 126 || (c == '\t' && is_flowed))
                    bytes_to_add = 3; // Encoded as '=XX'
                else
                    bytes_to_add = 1; // Literal character

                // Check Line Length Constraint (72 max before soft break '=\r\n')
                if (line_len_tracker + bytes_to_add > 72)
                {
                    // Insert soft break: "=\r\n"
                    if (!append_str_helper("=\r\n", 3, out, current_len, QP_MAX_LINE_SIZE))
                        return -1;

                    is_soft_break_added = true;
                    break; // Ends the QP line
                }

                // Process the character
                if (bytes_to_add == 3)
                {
                    char hex[4];
                    sprintf(hex, "=%02X", c);
                    if (!append_str_helper(hex, 3, out, current_len, QP_MAX_LINE_SIZE))
                        return -1;
                    line_len_tracker += 3;
                }
                else
                {
                    if (!append_byte_helper((char)c, out, current_len, QP_MAX_LINE_SIZE))
                        return -1;
                    line_len_tracker += 1;
                }

                // Consume the byte and update the read count
                src.read();
                bytes_read++;
            }

            // Flowed mode finalization
            if (is_flowed && !is_soft_break_added)
            {
                // Check for trailing space:
                if (current_len > 0 && out[current_len - 1] == ' ')
                {
                    current_len--; // Remove the space
                    if (!append_str_helper("=20", 3, out, current_len, QP_MAX_LINE_SIZE))
                        return -1;
                }
                // Add required Flowed line break (which is =\r\n)
                if (!append_str_helper("=\r\n", 3, out, current_len, QP_MAX_LINE_SIZE))
                    return -1;
                is_soft_break_added = true;
            }

            // Mandatory Hard Break (\r\n) for transmission, unless already added
            if (current_len > 0 && !is_soft_break_added)
            {
                // Append a hard break to complete the RFC line
                if (!append_str_helper("\r\n", 2, out, current_len, QP_MAX_LINE_SIZE))
                    return -1;
            }

            // Null-terminate and return
            out[current_len] = '\0';

            // Update the index before returning
            if (current_len > 0)
            {
                index += bytes_read;
                return bytes_read;
            }
            return -1;
        }
    };

}

static inline int rd_qp_encode_chunk(src_data_ctx &ctx, int &index, char *out, size_t out_size)
{
    return rd::qp_traits<rd::plain_text>::encode_chunk(ctx, index, out, out_size);
}

static inline int rd_qp_encode_chunk_flowed(src_data_ctx &ctx, int &index, char *out, size_t out_size)
{
    return rd::qp_traits<rd::flowed_text>::encode_chunk(ctx, index, out, out_size);
}

#endif // READY_CODEC_QP_H