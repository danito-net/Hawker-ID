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

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  // Format: YYYY-MM-DD HH:mm:SS
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.println(buf);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());

  // Configure NTP + timezone
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Time synchronized, current WIB time:");
  printLocalTime();
}

void loop() {
  printLocalTime();
  delay(1000);
}
