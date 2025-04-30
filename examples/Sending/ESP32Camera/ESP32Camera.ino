/**
 * The example to send image from ESP32 camera.
 * For proper network/SSL client and port selection, please see http://bit.ly/437GkRA
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"

// ===================
// Select camera model
// ===================
// #define CAMERA_MODEL_WROVER_KIT // Has PSRAM
#define CAMERA_MODEL_ESP_EYE // Has PSRAM
// #define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
// #define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
// #define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
// #define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
// #define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
// #define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
// #define CAMERA_MODEL_AI_THINKER // Has PSRAM
// #define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
// #define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
//  ** Espressif Internal Boards **
// #define CAMERA_MODEL_ESP32_CAM_BOARD
// #define CAMERA_MODEL_ESP32S2_CAM_BOARD
// #define CAMERA_MODEL_ESP32S3_CAM_LCD
// #define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3 // Has PSRAM
// #define CAMERA_MODEL_DFRobot_Romeo_ESP32S3 // Has PSRAM
#include "camera_pins.h"

#define ENABLE_SMTP  // Allows SMTP class and data
#define ENABLE_DEBUG // Allows debugging
#define READYMAIL_DEBUG_PORT Serial
#include "ReadyMail.h"

#define SMTP_HOST "_______"
#define SMTP_PORT 465
#define DOMAIN_OR_IP "127.0.0.1"
#define AUTHOR_EMAIL "_______"
#define AUTHOR_PASSWORD "_______"
#define RECIPIENT_EMAIL "_______"

#define WIFI_SSID "_______"
#define WIFI_PASSWORD "_______"

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

// For more information, see https://bit.ly/44g9Fuc
void smtpCb(SMTPStatus status)
{
    if (status.progressUpdated)
        ReadyMail.printf("ReadyMail[smtp][%d] Uploading file %s, %d %% completed\n", status.state, status.filename.c_str(), status.progress);
    else
        ReadyMail.printf("ReadyMail[smtp][%d]%s\n", status.state, status.text.c_str());
}

void addBlobAttachment(SMTPMessage &msg, const String &filename, const String &mime, const String &name, const uint8_t *blob, size_t size, const String &encoding = "", const String &cid = "")
{
    Attachment attachment;
    attachment.filename = filename;
    attachment.mime = mime;
    attachment.name = name;
    // The inline content disposition.
    // Should be matched the image src's cid in html body
    attachment.content_id = cid;
    attachment.attach_file.blob = blob;
    attachment.attach_file.blob_size = size;
    // Specify only when content is already encoded.
    attachment.content_encoding = encoding;
    if (cid.length() > 0)
        msg.addInlineImage(attachment);
    else
        msg.addAttachment(attachment);
}

void setup()
{
    Serial.begin(115200);
    Serial.println();

    camera_config_t camCfg;
    camCfg.ledc_channel = LEDC_CHANNEL_0;
    camCfg.ledc_timer = LEDC_TIMER_0;
    camCfg.pin_d0 = Y2_GPIO_NUM;
    camCfg.pin_d1 = Y3_GPIO_NUM;
    camCfg.pin_d2 = Y4_GPIO_NUM;
    camCfg.pin_d3 = Y5_GPIO_NUM;
    camCfg.pin_d4 = Y6_GPIO_NUM;
    camCfg.pin_d5 = Y7_GPIO_NUM;
    camCfg.pin_d6 = Y8_GPIO_NUM;
    camCfg.pin_d7 = Y9_GPIO_NUM;
    camCfg.pin_xclk = XCLK_GPIO_NUM;
    camCfg.pin_pclk = PCLK_GPIO_NUM;
    camCfg.pin_vsync = VSYNC_GPIO_NUM;
    camCfg.pin_href = HREF_GPIO_NUM;
    camCfg.pin_sscb_sda = SIOD_GPIO_NUM;
    camCfg.pin_sscb_scl = SIOC_GPIO_NUM;
    camCfg.pin_pwdn = PWDN_GPIO_NUM;
    camCfg.pin_reset = RESET_GPIO_NUM;
    camCfg.xclk_freq_hz = 20000000;
    camCfg.pixel_format = PIXFORMAT_JPEG;
    camCfg.frame_size = FRAMESIZE_QXGA;
    camCfg.jpeg_quality = 10;
    camCfg.fb_count = 2;

    // camera init
    esp_err_t err = esp_camera_init(&camCfg);
    if (err != ESP_OK)
    {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    ssl_client.setInsecure();

    smtp.connect(SMTP_HOST, SMTP_PORT, DOMAIN_OR_IP, smtpCb);
    if (!smtp.isConnected())
        return;

    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
    if (!smtp.isAuthenticated())
        return;

    SMTPMessage msg;
    msg.sender.name = "ReadyMail";
    msg.sender.email = AUTHOR_EMAIL;
    msg.subject = "ReadyMail ESP32 camera";
    msg.addRecipient("User", RECIPIENT_EMAIL);

    String bodyText = "This is image from ESP32 camera.\n";
    msg.text.content = bodyText;
    msg.text.transfer_encoding = "base64";
    msg.html.content = "<html><body><div style=\"color:#00ffff;\">" + bodyText + "<br/><br/><img src=\"cid:camera_image\" alt=\"ESP32 camera image\"></div></body></html>";
    msg.html.transfer_encoding = "base64";

    // current timestamp
    msg.timestamp = 1746013620;

    camera_fb_t *fb = esp_camera_fb_get();
    addBlobAttachment(msg, "camera.jpg", "image/jpg", "camera.jpg", fb->buf, fb->len, "", "camera_image");
    smtp.send(msg);
}

void loop()
{
}