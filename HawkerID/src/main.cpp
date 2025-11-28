#include <WiFi.h>
#include "time.h"
#include "secrets.h"
#include "driver/i2s.h"

#define MIC_I2S_PORT       I2S_NUM_0
#define MIC_SAMPLE_RATE    16000
#define MIC_SAMPLE_BITS    I2S_BITS_PER_SAMPLE_16BIT
#define MIC_CHANNEL_FORMAT I2S_CHANNEL_FMT_ONLY_LEFT
#define MIC_BUFFER_SAMPLES 512   // samples per read

// TODO: update these pins from DFRobot documentation
#define MIC_PIN_BCK  42
#define MIC_PIN_WS   41
#define MIC_PIN_DATA 40

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// NTP config
const char* ntpServer = "pool.ntp.org";
// GMT+7 = 7 * 3600 = 25200 seconds
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;  // no DST in WIB

void micInit() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = MIC_SAMPLE_RATE,
    .bits_per_sample = MIC_SAMPLE_BITS,
    .channel_format = MIC_CHANNEL_FORMAT,
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = MIC_BUFFER_SAMPLES,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = MIC_PIN_BCK,
    .ws_io_num = MIC_PIN_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_PIN_DATA
  };

  i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(MIC_I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(MIC_I2S_PORT);
}

String currentTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01 00:00:00";
  }
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

void setup() {
  
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Booting ESP32-S3 mic loudness demo");

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
  Serial.println("Waiting for time sync...");
  
  struct tm timeinfo;
  
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Time synced: ");
  Serial.println(currentTimestamp());
  
  micInit();
  Serial.println("Microphone initialized.");
}

void loop() {
}
