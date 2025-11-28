#include <WiFi.h>
#include "time.h"
#include "secrets.h"
#include "driver/i2s.h"

#define MIC_I2S_PORT       I2S_NUM_0
#define MIC_SAMPLE_RATE    16000
#define MIC_SAMPLE_BITS    I2S_BITS_PER_SAMPLE_16BIT
#define MIC_BUFFER_SAMPLES 512

// PDM mic pins (DFRobot)
#define MIC_PIN_CLK  38   // CLOCK_PIN
#define MIC_PIN_DATA 39   // DATA_PIN

bool micReady = false;

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// NTP config
const char* ntpServer = "pool.ntp.org";
// GMT+7 = 7 * 3600 = 25200 seconds
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;  // no DST in WIB

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
    .ws_io_num = MIC_PIN_CLK,        // 38
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_PIN_DATA      // 39
  };

  esp_err_t err = i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
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
  if (!micReady) {
     Serial.println("Microphone init failed, stopping.");
     // TODO: while (true) delay(1000);
  }
  Serial.println("Microphone initialized.");
}

void loop() {
  
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
  Serial.print(currentTimestamp());
  Serial.print(" first samples: ");
  for (int i = 0; i < 8 && i < samplesRead; i++) {
    Serial.print(micBuffer[i]);
    Serial.print(" ");
  }
  Serial.println();

  float rms = computeRmsLoudness(micBuffer, samplesRead);

    // Log every buffer with timestamp and RMS
  String ts = currentTimestamp();
  Serial.print(ts);
  Serial.print(" RMS=");
  Serial.println(rms);

  // TODO: if (rms > LOUDNESS_THRESHOLD) {capture photo}

}

