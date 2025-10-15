#ifndef READY_CODEC_CHUNK_H
#define READY_CODEC_CHUNK_H

#include "ReadyStream.h"
#include "ReadyCodec_QP.h"
#include "ReadyCodec_Base64.h"

// Unified chunk encoder for streaming-safe SMTP body and attachments
static inline String rd_encode_chunk(src_data_ctx &ctx, int &index)
{
    switch (ctx.xenc)
    {
    case xenc_base64:
        return rd_b64_encode_chunk(ctx, index);

    case xenc_qp:
        return rd_qp_encode_chunk(ctx, index); // plain or flowed

    case xenc_7bit:
    default:
    {
        String out;
        ctx.seek(index);
        while (ctx.available() && out.length() < 76)
        {
            char c = ctx.read();
            if (c > 0 && c < 128)
                out += c;
            index++;
        }

        // Diagnostics
        ctx.lines_encoded++;
        if ((int)out.length() > ctx.max_line_length)
            ctx.max_line_length = out.length();

        return out;
    }
    }
}

#endif // READY_CODEC_CHUNK_H
