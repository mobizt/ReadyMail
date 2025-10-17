#include "ReadyStream.h"
#include "ReadyCodec_QP.h"
#include "ReadyCodec_Base64.h"
#include "ReadyCodec_Chunk.h"
#include "ReadyCodec.h"


String decode_base64(const String &in)
{
    return rd_b64_dec(in.c_str());
}

String decode_qp(const String &in)
{
    String out;
    int i = 0;
    while (i < in.length())
    {
        if (in[i] == '=' && i + 2 < in.length())
        {
            // Soft break
            if (in[i + 1] == '\r' && in[i + 2] == '\n')
            {
                i += 3;
                continue;
            }

            // Encoded byte
            char hex[3] = {in[i + 1], in[i + 2], 0};
            char byte = (char)strtol(hex, nullptr, 16);
            out += byte;
            i += 3;
        }
        else
        {
            out += in[i++];
        }
    }
    return out;
}

String normalize(const String &s)
{
    String out;
    for (int i = 0; i < s.length(); ++i)
    {
        char c = s[i];
        if (c != '\r')
            out += c; // remove carriage returns
    }
    return out;
}

void chunk_stats(const String &chunk, int index, src_data_ctx &ctx)
{
    int bytes = chunk.length();
    bool has_utf8 = false;
    bool has_trailing_space = false;

    for (int i = 0; i < chunk.length(); ++i)
    {
        uint8_t c = chunk[i];
        if (c >= 0xC2 && c <= 0xF4)
        {
            if (i + 1 < chunk.length())
            {
                uint8_t next = chunk[i + 1];
                if (next >= 0x80 && next <= 0xBF)
                {
                    has_utf8 = true;
                    break;
                }
            }
        }
    }

    if (chunk.endsWith(" ") || chunk.endsWith("=20"))
    {
        has_trailing_space = true;
    }

    const char *enc_name = (ctx.xenc == xenc_base64) ? "base64" : (ctx.xenc == xenc_qp) ? "quoted-printable"
                                                              : (ctx.xenc == xenc_7bit) ? "7bit"
                                                                                        : "unknown";

    Serial.printf("Chunk %d: %d bytes, UTF-8: %s, Trailing space: %s, Encoding: %s\n",
                  index, bytes,
                  has_utf8 ? "✅" : "❌",
                  has_trailing_space ? "✅" : "❌",
                  enc_name);
}

void test_encoding(const char *label, const char *input, smtp_content_xenc mode,
                   String (*encoder)(src_data_ctx &, int &) = nullptr, bool smtp_mode = true)
{
    Serial.printf("\n[TEST] %s\n", label);

    src_data_ctx ctx;
    ctx.str = input;
    ctx.type = src_data_string;
    ctx.valid = true;
    ctx.xenc = mode;

    int index = 0;
    int chunk_index = 0;
    while (ctx.available())
    {
        String line = encoder ? encoder(ctx, index) : rd_encode_chunk(ctx, index);
        Serial.printf("Line (%d): %s\n", line.length(), line.c_str());
        if (smtp_mode)
            Serial.print("\r\n");

        String clean_encoded = line;
        clean_encoded.replace("=\r\n", ""); // remove soft breaks
        clean_encoded.replace("\r\n", "");  // remove hard breaks

        chunk_stats(clean_encoded, chunk_index++, ctx);

        ctx.lines_encoded++;
        if (line.length() > ctx.max_line_length)
            ctx.max_line_length = line.length();
    }

    Serial.printf("Lines: %d, Bytes: %d, Max Line: %d\n",
                  ctx.lines_encoded, ctx.bytes_read, ctx.max_line_length);
}

String clean_base64(const String &in)
{
    String out;
    for (int i = 0; i < in.length(); ++i)
    {
        char c = in[i];
        if (c != '\r' && c != '\n')
            out += c;
    }
    return out;
}

void verify_round_trip(const char *label, const char *original, smtp_content_xenc mode,
                       String (*encoder)(src_data_ctx &, int &), bool test_mode = true)
{
    Serial.printf("\n[VERIFY] %s\n", label);

    src_data_ctx ctx;
    ctx.str = original;
    ctx.type = src_data_string;
    ctx.valid = true;
    ctx.xenc = mode;

    int index = 0;
    String encoded;
    while (ctx.available())
    {
        String chunk = encoder(ctx, index);
        encoded += chunk;
        if (!test_mode)
            encoded += "\r\n"; // SMTP-safe
    }

    String clean_encoded = encoded;
    clean_encoded.replace("=\r\n", ""); // remove soft breaks
    clean_encoded.replace("\r\n", "");  // remove hard breaks

    String decoded;
    if (mode == xenc_base64)
        decoded = decode_base64(clean_encoded);
    else if (mode == xenc_qp)
        decoded = decode_qp(clean_encoded);

    bool match = (normalize(decoded) == normalize(original));
    Serial.printf("Match: %s\n", match ? "✅ YES" : "❌ NO");
    if (!match)
    {
        Serial.println("Original:");
        Serial.println("[" + normalize(original) + "]");
        Serial.println("Decoded:");
        Serial.println("[" + normalize(decoded) + "]");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    test_encoding("7bit ASCII", "Hello world!\r\nThis is a test.", xenc_7bit);
    test_encoding("Quoted-Printable", "Hello 🌍 — this is UTF-8!", xenc_qp);
    test_encoding("Base64", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=", xenc_base64);

    test_encoding("Quoted-Printable (Flowed)",
                  "This is a long paragraph that should be wrapped using format=flowed. "
                  "It contains multiple sentences and should break softly at word boundaries. "
                  "Each line ends with a space to indicate a soft break. "
                  "This allows email clients to rewrap the text dynamically. ",
                  xenc_qp, rd_qp_encode_chunk_flowed);

    test_encoding("Thai UTF-8 (Flowed)",
                  "สวัสดีครับ นี่คือข้อความที่ยาวมากและควรจะถูกตัดบรรทัดแบบ flowed "
                  "เพื่อให้สามารถจัดรูปแบบใหม่ได้โดยไคลเอนต์อีเมลที่รองรับ format=flowed "
                  "ข้อความนี้มีเว้นวรรคท้ายบรรทัดเพื่อให้สามารถรวมบรรทัดได้อย่างถูกต้อง ",
                  xenc_qp, rd_qp_encode_chunk_flowed);

    verify_round_trip("Base64 Round-Trip",
                      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=",
                      xenc_base64, rd_b64_encode_chunk, true); // test mode

    verify_round_trip("QP Thai Flowed Round-Trip",
                      "สวัสดีครับ นี่คือข้อความที่ยาวมากและควรจะถูกตัดบรรทัดแบบ flowed "
                      "เพื่อให้สามารถจัดรูปแบบใหม่ได้โดยไคลเอนต์อีเมลที่รองรับ format=flowed "
                      "ข้อความนี้มีเว้นวรรคท้ายบรรทัดเพื่อให้สามารถรวมบรรทัดได้อย่างถูกต้อง ",
                      xenc_qp, rd_qp_encode_chunk_flowed, true); // test mode
}

void loop() {}