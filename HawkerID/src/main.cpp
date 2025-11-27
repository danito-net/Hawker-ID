#include <WiFi.h>
#include "time.h"
#include "secrets.h"

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// NTP config
const char* ntpServer = "pool.ntp.org";
// GMT+7 = 7 * 3600 = 25200 seconds
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;  // no DST in WIB

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
  delay(2000);

  Serial.println();
  Serial.println("Booting ESP32-S3 mic loudness demo");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
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
}


void loop() {
}
