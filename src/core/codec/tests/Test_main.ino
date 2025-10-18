#include <Arduino.h>
#include "./core/codec/ReadyCodec.h"
#include "./core/codec/ReadyCodec_QP.h"
#include "./core/codec/ReadyCodec_Chunk.h"
#include "./core/codec/ReadyCodec_Utils.h"

#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

const char sample_text[] = "Speak No Evil is a 2024 American psychological horror thriller film written and directed by James Watkins. A remake "
                           "of the 2022 Danish-Dutch film of the same name, the film stars James McAvoy, Mackenzie Davis, Aisling Franciosi, Alix "
                           "West Lefler, Dan Hough, and Scoot McNairy. Its plot follows an American family who are invited to stay at a remote "
                           "farmhouse of a British couple for the weekend, and the hosts soon test the limits of their guests as the situation "
                           "escalates. Jason Blum serves as a producer through his Blumhouse Productions banner. Speak No Evil premiered at the "
                           "DGA Theater in New York City on September 9, 2024 and was released in the United States by Universal Pictures on "
                           "September 13, 2024. The film received generally positive reviews from critics and grossed $77 million worldwide "
                           "with a budget of $15 million.";

struct SerialSink
{
  void write(uint8_t b) { Serial.write(b); }
  void write(const char *s) { Serial.print(s); }
  void print(const char *s) { Serial.print(s); }
  void println() { Serial.println(); }
};

SerialSink sink;
src_data_ctx ctx;

template <smtp_content_xenc mode>
void test_chunk_writer_traits_dispatch(src_data_ctx &ctx, smtp_content_xenc xenc, const char *title)
{
  int index = 0;
  ctx.index = 0;
  ctx.xenc = xenc;
  Serial.print(" ====== ");
  Serial.print(title);
  Serial.println(" ====== ");
  while (ctx.available())
  {
    chunk_writer_traits<mode, SerialSink>::write(ctx, sink, index);
  }

  sink.println();
}

void setup()
{
  Serial.begin(115200);

  ctx.init_static((const uint8_t *)sample_text, strlen(sample_text));

  test_chunk_writer_traits_dispatch<xenc_base64>(ctx, xenc_base64, "Base64 Encoding");
  test_chunk_writer_traits_dispatch<xenc_qp>(ctx, xenc_qp, "QP Encoding");
  test_chunk_writer_traits_dispatch<xenc_qp_flowed>(ctx, xenc_qp_flowed, "QP flowed text Encoding");
  test_chunk_writer_traits_dispatch<xenc_7bit>(ctx, xenc_7bit, "7 bit Encoding");
  test_chunk_writer_traits_dispatch<xenc_8bit>(ctx, xenc_8bit, "8 bit Encoding");
  test_chunk_writer_traits_dispatch<xenc_binary>(ctx, xenc_binary, "Binary Encoding");
}

void loop() {}
