#ifndef READY_CODEC_CHUNK_H
#define READY_CODEC_CHUNK_H

#include "ReadyStream.h"
#include "ReadyCodec_QP.h"
#include "ReadyCodec_Base64.h"


    // Raw 7bit encoder
    static inline String rd_chunk_encode_raw(src_data_ctx &ctx, int &index)
    {
        String out;
        while (ctx.available())
            out += (char)ctx.read();
        return out;
    }

    // Dispatcher for safe encodings
    static inline String encode_chunk(src_data_ctx &ctx, int &index)
    {
        switch (ctx.xenc)
        {
        case xenc_7bit:
            return rd_chunk_encode_raw(ctx, index);
        case xenc_qp:
            return rd_qp_encode_chunk(ctx, index);
        case xenc_qp_flowed:
            return rd_qp_encode_chunk_flowed(ctx, index);
        case xenc_base64:
            return rd_b64_encode_chunk(ctx, index);
        default:
            return "";
        }
    }

    // Ex:
    // String encoded = rd::rd_encode_chunk(ctx, index);

    static inline String rd_encode_chunk(src_data_ctx &ctx, int &index)
    {
        return encode_chunk(ctx, index);
    }

    // Binary-safe writers
    template <typename Sink>
    static inline void rd_chunk_write_8bit(src_data_ctx &ctx, Sink &out)
    {
        while (ctx.available())
            out.write(ctx.read());
    }

    // Ex:
    // rd::rd_chunk_write_binary(ctx, Serial);
    template <typename Sink>
    static inline void rd_chunk_write_binary(src_data_ctx &ctx, Sink &out)
    {
        while (ctx.available())
            out.write(ctx.read());
    }

    // Ex:
    // rd::chunk_writer_traits<xenc_8bit, WiFiClient>::write(ctx, client);

    // Chunk writer traits
    template <smtp_content_xenc Mode, typename Sink>
    struct chunk_writer_traits
    {
        static void write(src_data_ctx &ctx, Sink &out)
        {
            switch (Mode)
            {
            case xenc_8bit:
                rd_chunk_write_8bit(ctx, out);
                break;
            case xenc_binary:
                rd_chunk_write_binary(ctx, out);
                break;
            default:
            {
                int index = 0;
                String encoded = encode_chunk(ctx, index);
                out.print(encoded);
                break;
            }
            }
        }
    };

    // Ex:
    // size_t chunk = rd::max_chunk_size(ctx.xenc);

    // Max chunk size helper
    static inline size_t max_chunk_size(uint8_t xenc)
    {
        switch (xenc)
        {
        case xenc_base64:
            return 57;
        case xenc_qp:
        case xenc_qp_flowed:
            return 72;
        case xenc_7bit:
        case xenc_8bit:
            return 998;
        case xenc_binary:
            return 1024;
        default:
            return 0;
        }
    }

    // Ex:
    //  rd::src_data_ctx ctx = ...;
    // ctx.xenc = xenc_base64;
    // ctx.bytes_read = 57;

    // rd::chunk_logger("Header", ctx);
    // String encoded = rd::rd_encode_chunk(ctx, index);

    // const char* mime = rd::infer_mime_type("photo.jpg");
    // bool valid_utf8 = rd::is_utf8(ctx.buf, ctx.bytes_read);

    // MIME type inference (basic)
    static inline const char *infer_mime_type(const char *filename)
    {
        if (!filename)
            return "application/octet-stream";
        const char *ext = strrchr(filename, '.');
        if (!ext)
            return "application/octet-stream";
        ext++;
        if (!strcmp(ext, "txt"))
            return "text/plain";
        if (!strcmp(ext, "html"))
            return "text/html";
        if (!strcmp(ext, "jpg") || !strcmp(ext, "jpeg"))
            return "image/jpeg";
        if (!strcmp(ext, "png"))
            return "image/png";
        if (!strcmp(ext, "pdf"))
            return "application/pdf";
        return "application/octet-stream";
    }

    // Use is_utf8(const uint8_t* data, size_t len) to check if a buffer contains valid UTF-8:
    // bool valid = rd::is_utf8(ctx.buf, ctx.bytes_read);
    // You can use this to decide whether to fallback to base64 or flag xenc_8bit.

    // UTF-8 validator (non-strict, embedded-safe)
    static inline bool is_utf8(const uint8_t *data, size_t len)
    {
        size_t i = 0;
        while (i < len)
        {
            uint8_t c = data[i];
            if (c < 0x80)
            {
                i++;
                continue;
            }
            else if ((c & 0xE0) == 0xC0 && i + 1 < len &&
                     (data[i + 1] & 0xC0) == 0x80)
            {
                i += 2;
                continue;
            }
            else if ((c & 0xF0) == 0xE0 && i + 2 < len &&
                     (data[i + 1] & 0xC0) == 0x80 &&
                     (data[i + 2] & 0xC0) == 0x80)
            {
                i += 3;
                continue;
            }
            else
                return false;
        }
        return true;
    }

    // rd::src_data_ctx ctx = ...;
    // ctx.xenc = xenc_8bit;
    // ctx.bytes_read = 128;

    // rd::chunk_logger("Attachment", ctx);

    // if (!rd::is_utf8(ctx.buf, ctx.bytes_read)) {
    //  ctx.xenc = xenc_base64;  // fallback for binary safety
    //}

    // const char* mime = rd::infer_mime_type("image.png");

    // WiFiClient client;
    // rd::chunk_writer_traits<xenc_base64, WiFiClient>::write(ctx, client);

    // Diagnostic hook
    static inline void chunk_logger(const char *label, src_data_ctx &ctx)
    {
        Serial.print("[CHUNK] ");
        Serial.print(label);
        Serial.print(" | size=");
        Serial.print(ctx.bytes_read);
        Serial.print(" | xenc=");
        Serial.println(ctx.xenc);
    }

    // 1.

    // rd::src_data_ctx ctx = ...;
    // ctx.xenc = xenc_base64;
    // ctx.bytes_read = 57;

    // WiFiClient client;
    // rd::rd_chunk_write_mime(ctx, client, "photo.jpg", "attachment");

    // Result

    // Content-Type: image/jpeg; name="photo.jpg"
    // Content-Transfer-Encoding: base64
    // Content-Disposition: attachment; filename="photo.jpg"

    // <encoded body>

    // 2.

    // const char* boundary = "----boundary123";
    // WiFiClient client;

    // client.println(F("Content-Type: multipart/mixed; boundary=\"----boundary123\""));
    // client.println();

    // rd::rd_chunk_write_mime(ctx, client, "photo.jpg", "attachment", boundary);

    // Final boundary close
    // client.print(F("--"));
    // client.print(boundary);
    // client.println(F("--"));

    template <typename Sink>
    static inline void rd_chunk_write_mime(src_data_ctx &ctx, Sink &out, const char *name = nullptr, const char *disposition = "inline", const char *boundary = nullptr, const char *cid = nullptr)
    {
        if (boundary)
        {
            out.print(F("--"));
            out.println(boundary);
        }

        const char *mime = infer_mime_type(name);
        out.print(F("Content-Type: "));
        out.print(mime);
        if (ctx.xenc == xenc_qp || ctx.xenc == xenc_qp_flowed || ctx.xenc == xenc_8bit)
        {
            out.print(F("; charset=\"UTF-8\""));
        }
        if (name)
        {
            out.print(F("; name=\""));
            out.print(name);
            out.println(F("\""));
        }
        else
        {
            out.println();
        }

        out.print(F("Content-Transfer-Encoding: "));
        switch (ctx.xenc)
        {
        case xenc_7bit:
            out.println(F("7bit"));
            break;
        case xenc_qp:
            out.println(F("quoted-printable"));
            break;
        case xenc_qp_flowed:
            out.println(F("quoted-printable"));
            break;
        case xenc_base64:
            out.println(F("base64"));
            break;
        case xenc_8bit:
            out.println(F("8bit"));
            break;
        case xenc_binary:
            out.println(F("binary"));
            break;
        default:
            out.println(F("7bit"));
            break;
        }

        out.print(F("Content-Disposition: "));
        out.print(disposition);
        if (name)
        {
            out.print(F("; filename=\""));
            out.print(name);
            out.println(F("\""));
        }
        else
        {
            out.println();
        }

        if (cid)
        {
            out.print(F("Content-ID: <"));
            out.print(cid);
            out.println(F(">\r\n"));
        }

        out.println();
        chunk_writer_traits<(smtp_content_xenc)ctx.xenc, Sink>::write(ctx, out);
        out.println();
    }

    template <typename Sink>
    static inline void rd_chunk_write_mime_html(src_data_ctx &ctx, Sink &out, const char *boundary = nullptr)
    {
        if (boundary)
        {
            out.print(F("--"));
            out.println(boundary);
        }

        out.println(F("Content-Type: text/html; charset=\"UTF-8\""));
        out.println(F("Content-Transfer-Encoding: quoted-printable"));
        out.println();

        int index = 0;
        String encoded = rd_qp_encode_chunk(ctx, index);
        out.print(encoded);
        out.println();
    }

    // 1.

    // const char* boundary = "----msg-boundary";
    // WiFiClient client;

    // rd::rd_chunk_write_mime_part(client, boundary, "related");

    // rd::src_data_ctx html = ...;
    // html.xenc = xenc_qp;
    // rd::rd_chunk_write_mime_html(html, client, boundary);

    // rd::src_data_ctx image = ...;
    // image.xenc = xenc_base64;
    // rd::rd_chunk_write_mime(image, client, "logo.png", "inline", boundary, "logo123");

    // client.print(F("--"));
    // client.print(boundary);
    // client.println(F("--"));

    // 2.
    //  const char* boundary = "----msg-boundary";

    // Step 1: Emit multipart header
    // client.println(F("Content-Type: multipart/related; boundary=\"----msg-boundary\""));
    // client.println();

    // Step 2: Emit HTML part
    // rd::src_data_ctx html = ...;
    // html.xenc = xenc_qp;
    // rd::rd_chunk_write_mime_html(html, client, boundary);

    // Step 3: Emit inline image with Content-ID
    // rd::src_data_ctx image = ...;
    // image.xenc = xenc_base64;
    // rd::rd_chunk_write_mime(image, client, "logo.png", "inline", boundary, "logo123");

    // Step 4: Close multipart
    // client.print(F("--"));
    // client.print(boundary);
    // client.println(F("--"));

    // 3.
    // const char* boundary = "----msg-boundary";

    // Step 1: SMTP Envelope
    // client.println(F("From: Suwatchai <suwatchai@example.com>"));
    // client.println(F("To: Recipient <recipient@example.com>"));
    // client.println(F("Subject: Embedded Image Test"));
    // client.println(F("MIME-Version: 1.0"));
    // client.print(F("Content-Type: multipart/related; boundary=\""));
    // client.print(boundary);
    // client.println(F("\""));
    // client.println();

    // Step 2: HTML Part
    // rd::src_data_ctx html = ...;
    // html.xenc = xenc_qp;
    // rd::rd_chunk_write_mime_html(html, client, boundary);

    // Step 3: Inline Image Part
    // rd::src_data_ctx image = ...;
    // image.xenc = xenc_base64;
    // rd::rd_chunk_write_mime(image, client, "logo.png", "inline", boundary, "logo123");

    // Step 4: Close Multipart
    // client.print(F("--"));
    // client.print(boundary);
    // client.println(F("--"));

    template <typename Sink>
    static inline void rd_chunk_write_mime_part(Sink &out, const char *boundary, const char *subtype = "mixed")
    {
        out.print(F("Content-Type: multipart/"));
        out.print(subtype);
        out.print(F("; boundary=\""));
        out.print(boundary);
        out.println(F("\""));
        out.println();
    }

    // const char* html_body =
    // "<html><body>"
    // "<h1>Hello!</h1>"
    // "<p>This email includes an inline image:</p>"
    // "<img src=\"cid:logo123\" alt=\"Logo\">"
    // "</body></html>";

    // const uint8_t* image_data = ...;  // your raw image bytes
    // size_t image_len = ...;           // image size in bytes

    // rd::src_data_ctx html_ctx;
    // html_ctx.buf = (const uint8_t*)html_body;
    // html_ctx.len = strlen(html_body);
    // html_ctx.xenc = xenc_qp;

    // rd::src_data_ctx image_ctx;
    // image_ctx.buf = image_data;
    // image_ctx.len = image_len;
    // image_ctx.xenc = xenc_base64;

    // 1.

    // WiFiClient client;
    // const char *boundary = "----msg-boundary";

    // rd::rd_chunk_write_mime_email(
    //   client,
    //   "Suwatchai <suwatchai@example.com>",
    //   "Recipient <recipient@example.com>",
    //   "Test Email with Inline Image",
    //   boundary,
    //   html_ctx,
    //   image_ctx,
    //   "logo.png",
    //   "logo123"
    //);

    // Result
    //
    // From: Suwatchai <suwatchai@example.com>
    // To: Recipient <recipient@example.com>
    // Subject: Test Email with Inline Image
    // MIME-Version: 1.0
    // Content-Type: multipart/related; boundary="----msg-boundary"
    //
    // ------msg-boundary
    // Content-Type: text/html; charset="UTF-8"
    // Content-Transfer-Encoding: quoted-printable

    // <html><body>...<img src="cid:logo123">...</body></html>

    // ------msg-boundary
    // Content-Type: image/png; name="logo.png"
    // Content-Transfer-Encoding: base64
    // Content-Disposition: inline; filename="logo.png"
    // Content-ID: <logo123>

    // iVBORw0KGgoAAAANSUhEUgAA...

    // ------msg-boundary--

    template <typename Sink>
    static inline void rd_chunk_write_mime_email(
        Sink &out,
        const char *from,
        const char *to,
        const char *subject,
        const char *boundary,
        src_data_ctx &html,
        src_data_ctx &image,
        const char *image_name = "logo.png",
        const char *image_cid = "logo123")
    {
        // Envelope headers
        out.print(F("From: "));
        out.println(from);
        out.print(F("To: "));
        out.println(to);
        out.print(F("Subject: "));
        out.println(subject);
        out.println(F("MIME-Version: 1.0"));
        out.print(F("Content-Type: multipart/related; boundary=\""));
        out.print(boundary);
        out.println(F("\""));
        out.println();

        // HTML part
        html.xenc = xenc_qp;
        rd_chunk_write_mime_html(html, out, boundary);

        // Inline image part
        image.xenc = xenc_base64;
        rd_chunk_write_mime(image, out, image_name, "inline", boundary, image_cid);

        // Final boundary close
        out.print(F("--"));
        out.print(boundary);
        out.println(F("--"));
    }

#endif // READY_CODEC_CHUNK_H
