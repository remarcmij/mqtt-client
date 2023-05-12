#include "Controller.h"
#include "DataModel.h"
#include "View.h"
#include "backlight.h"
#include "pin_config.h"
#include "secrets.h"
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <time.h>

#define MQTT_PORT 1883
#define MAX_SAMPLES 240

namespace {

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqttServer = MQTT_SERVER;

const char *ntpServer = "time.google.com";
const long gmtOffset_sec = 3600 * 1;
const int daylightOffset_sec = 3600 * 1;

auto wifiClient = WiFiClient{};
auto pubSubClient = PubSubClient{wifiClient};
auto dataModel = DataModel{};
auto view = View{TFT_WIDTH, TFT_HEIGHT};
auto controller = Controller{};

void nextPage() { view.nextPage(); }

button_handlers_t buttonEventHandlers = {
    .io14_handleClick = nullptr,
    .io14_handleDoubleClick = nullptr,
    .io14_handleLongPressStart = Backlight::startDecreaseBrightness,
    .io14_handleLongPressStop = Backlight::stopDecreaseBrightness,
    .boot_handleClick = nextPage,
    .boot_handleDoubleClick = nullptr,
    .boot_handleLongPressStart = Backlight::startIncreaseBrightness,
    .boot_handleLongPressStop = Backlight::stopDecreaseBrightness};

void reconnect() {
  while (WiFi.status() != WL_CONNECTED) {
    uint16_t innerRetry = 0;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && innerRetry++ < 4) {
      log_d("WiFi status retry: %u", innerRetry);
      delay(500);
    }
    if (WiFi.status() == WL_CONNECTED) {
      break;
    } else {
      WiFi.disconnect();
      delay(500);
    }
  }

  if (!pubSubClient.connected()) {
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
      WiFi.disconnect();
    }
  }
}

void mqttCallback(char *topic, byte *payloadRaw, unsigned int length) {
  dataModel.mqttUpdate(topic, payloadRaw, length);
}

} // namespace

void setup() {
  Serial.begin(115200);

  // Enable battery
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  view.init();
  Backlight::init();

  controller.setHandlers(buttonEventHandlers);

  WiFi.mode(WIFI_STA);
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
      4096, nullptr, 1, nullptr, 1);
  assert(rc == pdPASS);

  auto pvDataManager = static_cast<void *>(&dataModel);
  rc = xTaskCreatePinnedToCore(
      +[](void *param) { view.updateTask(param); }, "display", 6144,
      pvDataManager, 1, nullptr, 1);
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
