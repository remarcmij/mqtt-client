#include "ArduinoJson.h"
#include "Controller.h"
#include "DataManager.h"
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
uint32_t updateCounter;

namespace {

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqttServer = MQTT_SERVER;

const char *ntpServer = "time.google.com";
const long gmtOffset_sec = 3600 * 1;
const int daylightOffset_sec = 3600 * 1;

auto wifiClient = WiFiClient();
auto pubSubClient = PubSubClient(wifiClient);

auto dataManager = DataManager{};

auto controller = Controller{};

void reconnect() {
  int retries = 0;

  // Loop until we're reconnected
  while (!pubSubClient.connected()) {

    if (retries > 2) {
      // Try and reconnected to WiFi if reconnecting to
      // MQTT server keeps failing
      WiFi.disconnect();
      WiFi.begin(ssid, password);

      while (WiFi.status() != WL_CONNECTED) {
        log_d("WiFi not connected");
        delay(500);
      }

      log_i("WiFi reconnected: %s", WiFi.localIP().toString().c_str());
    }

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

    retries++;
  }
}

void mqttCallback(char *topic, byte *payloadRaw, unsigned int length) {
  dataManager.mqttUpdate(topic, payloadRaw, length);
}

} // namespace

void setup() {
  Serial.begin(115200);

  // Enable battery
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

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

  auto rc = xTaskCreatePinnedToCore(
      +[](void *param) { controller.controllerTask(param); }, "controller",
      2048, nullptr, 1, nullptr, 1);
  assert(rc == pdPASS);

  auto pvDataManager = static_cast<void *>(&dataManager);
  rc = xTaskCreatePinnedToCore(displayTask, "display", 3096, pvDataManager, 1,
                               nullptr, 1);
  assert(rc == pdPASS);
}

void loop() {
  if (!pubSubClient.connected()) {
    reconnect();
  }

  pubSubClient.loop();

  // Give idle task some execution time
  delay(1);
}
