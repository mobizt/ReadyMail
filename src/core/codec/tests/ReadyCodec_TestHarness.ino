#include <Arduino.h>
#include "./core/codec/ReadyCodec.h"
#include "./core/codec/ReadyCodec_QP.h"
#include "./core/codec/ReadyCodec_Chunk.h"
#include "./core/codec/ReadyCodec_Utils.h"

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

void test_encoding(const char *label, const char *input, smtp_content_xenc mode,
                   int (*encoder)(src_data_ctx &, int &, char *, size_t) = nullptr, bool smtp_mode = true)
{
    Serial.printf("\n[TEST] %s\n", label);

    src_data_ctx ctx;
    ctx.src = (uint8_t *)input;
    ctx.type = src_data_string;
    ctx.valid = true;
    ctx.xenc = mode;

    int index = 0;
    int chunk_index = 0;
    while (ctx.available())
    {
        char buf[100];
        int ret = encoder ? encoder(ctx, index, buf, 100) : rd_encode_chunk(ctx, index, buf, 100);
        Serial.printf("Line (%d): %s\n", ret, buf);
        if (smtp_mode)
            Serial.print("\r\n");

        String clean_encoded = buf;
        clean_encoded.replace("=\r\n", ""); // remove soft breaks
        clean_encoded.replace("\r\n", "");  // remove hard breaks

        ctx.lines_encoded++;
        if (ret > ctx.max_line_length)
            ctx.max_line_length = ret;
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
                       int (*encoder)(src_data_ctx &, int &, char *, size_t), bool test_mode = true)
{
    Serial.printf("\n[VERIFY] %s\n", label);

    src_data_ctx ctx;
    ctx.src = (uint8_t *)original;
    ctx.type = src_data_string;
    ctx.valid = true;
    ctx.xenc = mode;

    int index = 0;
    String encoded;
    while (ctx.available())
    {
        char buf[100];
        int chunk = encoder(ctx, index, buf, 100);
        encoded += buf;
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

    Serial.println("Original:");
    Serial.println("[" + normalize(original) + "]");
    Serial.println("Decoded:");
    Serial.println("[" + normalize(decoded) + "]");
}
const uint8_t my_blob[] PROGMEM = {0xDE, 0xAD, 0xBE, 0xEF};

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

    src_data_ctx ctx;
    ctx.init_static(my_blob, sizeof(my_blob));
    ctx.xenc = xenc_binary;
    int index = 0;
    while (ctx.available())
    {
        rd_chunk_write_binary(ctx, Serial, index);
    }
}

void loop() {}