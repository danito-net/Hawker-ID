#include <WiFi.h>
#include "time.h"
#include "driver/i2s.h"
#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include <WiFiClientSecure.h>
#include <DFRobot_LTR308.h>

#include "secrets.h"
#include "telegram_secrets.h"

const int IR_LED_PIN = 47;
const float IR_ON_LUX  = 10.0;  // below this, turn IR on
const float IR_OFF_LUX = 20.0;  // above this, turn IR off
bool irOn = false;

// SDCard's chip select pin for DFRobot ESP32-S3 AI CAM
#define SD_CARD_CS 10

// PDM mic pins for DFRobot ESP32-S3 AI CAM
#define MIC_PIN_CLK  38   // CLOCK_PIN
#define MIC_PIN_DATA 39   // DATA_PIN
#define MIC_I2S_PORT       I2S_NUM_0
#define MIC_SAMPLE_RATE    16000
#define MIC_SAMPLE_BITS    I2S_BITS_PER_SAMPLE_16BIT
#define MIC_BUFFER_SAMPLES 512

// Camera pins for DFRobot ESP32-S3 AI CAM (OV3660)
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  5
#define Y9_GPIO_NUM    4
#define Y8_GPIO_NUM    6
#define Y7_GPIO_NUM    7
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    17
#define Y4_GPIO_NUM    21
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    16
#define VSYNC_GPIO_NUM 1
#define HREF_GPIO_NUM  2
#define PCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  8
#define SIOC_GPIO_NUM  9

bool micReady = false;
bool cameraReady = false;

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// NTP config
const char* ntpServer = "pool.ntp.org";
// I live in West Java, Indonesia; so my TimeZone is WIB (GMT+7)
// GMT+7 = 7 * 3600 = 25200 seconds
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;  // no DST in WIB

RTC_DATA_ATTR int photo_count = 0;

DFRobot_LTR308 light;

WiFiClientSecure tgClient;
const char* TELEGRAM_HOST = "api.telegram.org";

String currentTimestamp() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01 00:00:00";
  }

  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);

}

void micInit() {

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM), // RX + PDM
    .sample_rate = MIC_SAMPLE_RATE,
    .bits_per_sample = MIC_SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = MIC_BUFFER_SAMPLES,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_PIN_NO_CHANGE, // not used for PDM mic
    .ws_io_num = MIC_PIN_CLK,        // PDM clock (DFRobot example maps CLOCK_PIN -> ws_io_num)
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_PIN_DATA      // PDM data
  };

  esp_err_t err = i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);

  if (err != ESP_OK) {
    micReady = false;
    Serial.print("i2s_driver_install failed, err=");
    Serial.println(err);
    return;
  }

  i2s_set_pin(MIC_I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(MIC_I2S_PORT);
  micReady = true;
  Serial.println("I2S PDM mic initialized.");

}

float computeRmsLoudness(int16_t *samples, size_t count) {
  double sumSquares = 0.0;
  for (size_t i = 0; i < count; i++) {
    float s = samples[i];
    sumSquares += s * s;
  }
  double mean = sumSquares / (double)count;
  return sqrt(mean);  // RMS amplitude
}

int16_t micBuffer[MIC_BUFFER_SAMPLES];
const float LOUDNESS_THRESHOLD = 3000.0f;  // I will tune this later 

bool initCamera() {

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_VGA;
  
  // config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.fb_location = CAMERA_FB_IN_DRAM;

  config.jpeg_quality = 15; // 1–63, lower is better quality

  config.fb_count     = 1;

  config.grab_mode    = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  Serial.print("esp_camera_init err=0x");
  Serial.println(err, HEX);

  if (err != ESP_OK) {
    Serial.println("Camera init failed.");
    cameraReady = false;
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  
  if (s && s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  cameraReady = true;
  Serial.println("Camera initialized.");
  return true;
  
}

bool sendPhotoToTelegram(camera_fb_t *fb) {
  if (!tgClient.connect(TELEGRAM_HOST, 443)) {
    Serial.println("Telegram: connection failed");
    return false;
  }

  String head = "--Xboundary\r\n"
                "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
                String(TELEGRAM_CHAT_ID) +
                "\r\n--Xboundary\r\n"
                "Content-Disposition: form-data; name=\"photo\"; filename=\"clap.jpg\"\r\n"
                "Content-Type: image/jpeg\r\n\r\n";

  String tail = "\r\n--Xboundary--\r\n";

  uint32_t imageLen = fb->len;
  uint32_t totalLen = head.length() + imageLen + tail.length();

  tgClient.println("POST /bot" + String(TELEGRAM_BOT_TOKEN) + "/sendPhoto HTTP/1.1");
  tgClient.println("Host: " + String(TELEGRAM_HOST));
  tgClient.println("Content-Type: multipart/form-data; boundary=Xboundary");
  tgClient.println("Content-Length: " + String(totalLen));
  tgClient.println();
  tgClient.print(head);

  // send image buffer
  tgClient.write(fb->buf, fb->len);

  tgClient.print(tail);

  // optional: read minimal response
  while (tgClient.connected()) {
    String line = tgClient.readStringUntil('\n');
    if (line == "\r") break;
  }
  String body = tgClient.readString();
  Serial.println("Telegram response: " + body);

  tgClient.stop();
  return true;
}

void captureOneFrame() {

  if (!cameraReady) {
    Serial.println("Camera not ready, cannot capture.");
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to obtain image frame.");
    return;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time for filename.");
    esp_camera_fb_return(fb);
    return;
  }

  // Filename format: YYYYMMDD-HHMMSS-N.jpg
  char timeStr[32];
  strftime(timeStr, sizeof(timeStr), "%Y%m%d-%H%M%S", &timeinfo);
  String path = "/" + String(timeStr) + "-" + String(photo_count) + ".jpg";

  File file = SD.open(path.c_str(), FILE_WRITE);

  if (!file) {
    Serial.println("Failed to open file on SD");
  } else {
    file.write(fb->buf, fb->len);
    file.close();
    Serial.print("Saved photo to ");
    Serial.println(path);
    photo_count++;
  }

  if (sendPhotoToTelegram(fb)) {
    Serial.println("Photo sent to Telegram.");
  } else {
    Serial.println("Failed to send photo to Telegram.");
  }
  
  esp_camera_fb_return(fb);
}

bool initSDCard() {

  if (!SD.begin(SD_CARD_CS)) {
    Serial.println("SD card init failed.");
    return false;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card detected.");
    return false;
  }

  Serial.println("SD card initialized.");
  return true;
}

void setup() {
  
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting HAWKER-ID ...");

  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi ");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Waiting for time sync ...");
  
  struct tm timeinfo;
  
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Time synced: ");
  Serial.println(currentTimestamp());
  Serial.println("Start initializing camera ...");
  
  if (!initCamera()) {
    Serial.println("Camera initialization failed, will not capture!");
  } else {
    Serial.println("Camera is ready ...");
  }

  Serial.println("Initializing light sensor...");
  while (!light.begin()) {
    Serial.println("LTR308 init failed, retrying...");
    delay(1000);
  }
  Serial.println("LTR308 init OK.");

  micInit();

  if (!micReady) {
     Serial.println("Microphone init failed, stopping.");
   } else {
     delay(1000);
     Serial.println("Microphone initialized.");
  }

  Serial.println("Initializing SD card...");
  
  if (!initSDCard()) {
    Serial.println("SD init failed, photos will not be saved.");
  } else {
    Serial.println("SD card ready.");
  }

  tgClient.setInsecure(); // or load root cert properly later
  Serial.println("Telegram client ready.");

    // IR LED pin
  const int IR_LED_PIN = 47;
  pinMode(IR_LED_PIN, OUTPUT);
  digitalWrite(IR_LED_PIN, LOW);  // start with IR off

}

void loop() {
  
  static unsigned long lastLightCheck = 0;
  unsigned long now = millis();
  
  if (now - lastLightCheck > 500) {  // check every 500 ms
    
    lastLightCheck = now;

    uint32_t raw = light.getData();
    float lux = light.getLux(raw);


    // Serial.print(currentTimestamp());
    // Serial.print(" raw=");
    // Serial.print(raw);
    // Serial.print(" lux=");
    // Serial.println(lux);

    // Simple hysteresis
    if (!irOn && lux < IR_ON_LUX) {
      irOn = true;
      digitalWrite(IR_LED_PIN, HIGH);
      // Serial.print(currentTimestamp());
      // Serial.print(" IR ON, lux=");
      // Serial.println(lux);
    } else if (irOn && lux > IR_OFF_LUX) {
      irOn = false;
      digitalWrite(IR_LED_PIN, LOW);
      // Serial.print(currentTimestamp());
      // Serial.print(" IR OFF, lux=");
      // Serial.println(lux);
    }
  }

  if (!micReady) {
    delay(1000);
    return;
  }

  size_t bytesRead = 0;

  esp_err_t res = i2s_read(
      MIC_I2S_PORT,
      (void*)micBuffer,
      sizeof(micBuffer),
      &bytesRead,
      portMAX_DELAY);

  if (res != ESP_OK || bytesRead == 0) {
    Serial.println("i2s_read error or zero bytes");
    return;
  }

  size_t samplesRead = bytesRead / sizeof(int16_t);

  // Debug: show first few samples
  
  // Serial.print(currentTimestamp());
  // Serial.print(" first samples: ");
  // for (int i = 0; i < 8 && i < samplesRead; i++) {
  //   Serial.print(micBuffer[i]);
  //   Serial.print(" ");
  // }
  // Serial.println();

  float rms = computeRmsLoudness(micBuffer, samplesRead);

  // Log every buffer with timestamp and RMS
  
  // String ts = currentTimestamp();
  // Serial.print(ts);
  // Serial.print(" RMS=");
  // Serial.println(rms);

  if (rms > LOUDNESS_THRESHOLD) {
    Serial.print(currentTimestamp());
    Serial.print(" Loud sound detected, RMS=");
    Serial.println(rms);

    captureOneFrame();
  }

}