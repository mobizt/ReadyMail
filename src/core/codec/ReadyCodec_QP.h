#ifndef READY_CODEC_QP_H
#define READY_CODEC_QP_H

#include "ReadyStream.h"

namespace rd
{

    template <typename T>
    struct qp_traits
    {
        static constexpr bool flowed = T::flowed;
        static constexpr int max_len = T::max_len;

        static String encode_chunk(src_data_ctx &src, int &index)
        {
            String out;
            char *buf = rd_mem<char *>(10);
            src.seek(index);

            int c1 = 0;
            while (src.available() || c1 > 0)
            {
                int c = c1 == 0 ? src.read() : c1;
                c1 = src.available() ? src.read() : 0;

                if ((int)out.length() >= max_len - 3 && c != 10 && c != 13)
                {
                    out += "=\r\n";
                    break;
                }

                if (c == 10 || c == 13)
                {
                    out += (char)c;
                }
                else if (c < 32 || c == '=' || c > 126)
                {
                    memset(buf, 0, 10);
                    sprintf(buf, "=%02X", (unsigned char)c);
                    out += buf;
                }
                else if (c == ' ' && (c1 == 10 || c1 == 13))
                {
                    out += "=20";
                }
                else
                {
                    out += (char)c;
                }

                index++;
            }

            rd_free(&buf);

            // Diagnostics
            src.lines_encoded++;
            if ((int)out.length() > src.max_line_length)
                src.max_line_length = out.length();

            return out;
        }
    };

    // Traits for plain text
    struct plain_text
    {
        static constexpr bool flowed = false;
        static constexpr int max_len = 76;
    };

    // Traits for flowed text (e.g., format=flowed)
    struct flowed_text
    {
        static constexpr bool flowed = true;
        static constexpr int max_len = 76;
    };

} // namespace rd

// Default quoted-printable encoder (plain text, non-flowed)
static inline String rd_qp_encode_chunk(src_data_ctx &ctx, int &index)
{
    return rd::qp_traits<rd::plain_text>::encode_chunk(ctx, index);
}

static inline String rd_qp_encode_chunk_flowed(src_data_ctx &ctx, int &index) {
    return rd::qp_traits<rd::flowed_text>::encode_chunk(ctx, index);
}


#endif // READY_CODEC_QP_H
