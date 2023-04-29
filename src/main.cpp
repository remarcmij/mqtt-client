#include "ArduinoJson.h"
#include "data.h"
#include "display.h"
#include "pin_config.h"
#include "secrets.h"
#include <Arduino.h>
#include <OneButton.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <time.h>
#include <utility>

#include "Calibri32.h"
#include "NotoSansBold15.h"
#define digits Calibri32
#define small NotoSansBold15

#define MQTT_PORT 1883
#define MAX_SAMPLES 240

// externals
QueueHandle_t qhReading = nullptr;
uint32_t updateCounter = 0;

namespace {

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqttServer = MQTT_SERVER;

const char *ntpServer = "time.google.com";
const long gmtOffset_sec = 3600 * 1;
const int daylightOffset_sec = 3600 * 1;

auto wifiClient = WiFiClient();
auto pubSubClient = PubSubClient(wifiClient);

auto lowerButton = OneButton(LOWER_BUTTON_PIN);
auto upperButton = OneButton(UPPER_BUTTON_PIN);

void reconnect() {
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.reconnect();
    delay(10000);
  }

  // Loop until we're reconnected
  while (!pubSubClient.connected()) {
    log_d("Attempting MQTT connection...");
    // Create a random pubSubClient ID
    String clientId = "esp32client-" + String(random(0xffff), HEX);
    // Attempt to connect
    if (pubSubClient.connect(clientId.c_str())) {
      log_d("...connected");
      auto success = pubSubClient.subscribe("/home/sensors/+");
      assert(success);
    } else {
      log_e("Could not connect to MQTT server, rc=%d", pubSubClient.state());
      delay(5000);
    }
  }
}

void handleLowerClick() { log_d("Lower button pressed"); }

TaskHandle_t brightnessTaskHandle;

void handleLongPressStart(void *param) {
  if (!brightnessTaskHandle) {
    BaseType_t rc =
        xTaskCreatePinnedToCore(changeBrightnessTask, "displayBrightness", 2400,
                                param, 1, &brightnessTaskHandle, 1);
    assert(rc == pdPASS);
  }
}

void handleLongPressStop() {
  if (brightnessTaskHandle) {
    vTaskDelete(brightnessTaskHandle);
    brightnessTaskHandle = nullptr;
  }
}

} // namespace

void setup() {
  Serial.begin(115200);

  qhReading = xQueueCreate(1, sizeof(reading_t));

  // Enable battery
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  lowerButton.attachClick(handleLowerClick);

  lowerButton.attachLongPressStart(handleLongPressStart,
                                   (void *)LOWER_BUTTON_PIN);
  lowerButton.attachLongPressStop(handleLongPressStop);

  upperButton.attachLongPressStart(handleLongPressStart,
                                   (void *)UPPER_BUTTON_PIN);
  upperButton.attachLongPressStop(handleLongPressStop);

  initDisplay();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    log_d("WiFi not connected");
    delay(500);
  }

  log_i("WiFi connected: %s", WiFi.localIP().toString().c_str());

  // Get system time from NTP server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Configure MQTT server connection
  pubSubClient.setServer(mqttServer, MQTT_PORT);
  pubSubClient.setCallback(mqttCallback);

  auto rc = xTaskCreatePinnedToCore(displayTask, "display", 3096, nullptr, 1,
                                    nullptr, 1);
  assert(rc == pdPASS);
}

void loop() {
  if (!pubSubClient.connected()) {
    reconnect();
  }

  pubSubClient.loop();
  lowerButton.tick();
  upperButton.tick();

  // Give idle task some execution time
  delay(1);
}
