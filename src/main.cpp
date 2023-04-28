#include "ArduinoJson.h"
#include "humidity.h"
#include "secrets.h"
#include "thermometer.h"
#include <Arduino.h>
#include <OneButton.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <utility>

#include "Calibri32.h"
#include "NotoSansBold15.h"
#define digits Calibri32
#define small NotoSansBold15

#define MQTT_PORT 1883
#define UPPER_BUTTON_PIN 14
#define LOWER_BUTTON_PIN 0

namespace {

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqttServer = MQTT_SERVER;

const char *ntpServer = "time.google.com";
const long gmtOffset_sec = 3600 * 1;
const int daylightOffset_sec = 3600 * 1;

const uint32_t backgroundColor = TFT_WHITE;

uint32_t displayHeight = TFT_HEIGHT;
uint32_t displayWidth = TFT_WIDTH;
uint32_t displayBrightness = 50;

struct reading_t {
  String sensor;
  String location;
  float temperature;
  float humidity;
};

QueueHandle_t qhReading = nullptr;
SemaphoreHandle_t semDisplay = nullptr;

auto tft = TFT_eSPI();
auto tempSprite = TFT_eSprite(&tft);
auto humSprite = TFT_eSprite(&tft);
auto detailsSprite = TFT_eSprite(&tft);

auto wifiClient = WiFiClient();
auto pubSubClient = PubSubClient(wifiClient);

auto lowerButton = OneButton(LOWER_BUTTON_PIN);
auto upperButton = OneButton(UPPER_BUTTON_PIN);

uint32_t secondsSinceLastUpdate = 0;

void reconnect() {
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
void changeBrightnessTask(void *param) {
  auto buttonPin = reinterpret_cast<unsigned>(param);
  for (;;) {
    if (buttonPin == UPPER_BUTTON_PIN && displayBrightness < 240) {
      displayBrightness++;
    } else if (buttonPin == LOWER_BUTTON_PIN && displayBrightness > 10) {
      displayBrightness--;
    }

    ledcWrite(0, displayBrightness);
    delay(20);
  }
}

void displayTask(void *argp) {
  reading_t reading;
  UBaseType_t hwmStack = 0;
  uint32_t freeHeapSize = 0;

  auto ticktime = xTaskGetTickCount();

  for (;;) {
    auto rc = xQueuePeek(qhReading, &reading, 0);
    if (rc == pdPASS) {
      auto strTemperature = String(reading.temperature, 1) + "°C";

      tempSprite.createSprite(90, 26);
      tempSprite.fillSprite(backgroundColor);
      tempSprite.loadFont(digits);
      tempSprite.setTextColor(TFT_BLACK, backgroundColor);
      tempSprite.drawString(strTemperature, 0, 0, 4);
      tempSprite.pushSprite(56, 22);

      auto strHumidity = String(reading.humidity, 0) + "%";

      humSprite.createSprite(60, 26);
      humSprite.fillSprite(backgroundColor);
      humSprite.loadFont(digits);
      humSprite.setTextColor(TFT_BLACK, backgroundColor);
      humSprite.drawString(strHumidity, 0, 0, 4);
      humSprite.pushSprite(198, 22);

      detailsSprite.createSprite(displayWidth, 80);
      detailsSprite.fillSprite(TFT_DARKGREY);
      detailsSprite.loadFont(small);
      detailsSprite.setTextColor(TFT_WHITE, TFT_DARKGREY);

      int32_t y = 4;
      detailsSprite.drawString(reading.sensor, 4, y);

      y += 17;
      auto strCounter = "Counter: " + String(secondsSinceLastUpdate++);
      detailsSprite.drawString(strCounter, 4, y);

      auto battery_mv = (analogRead(4) * 2 * 3.3 * 1000) / 4096;
      y += 17;
      auto strBattery = "Battery: " + String(battery_mv / 1000., 2) + "V";
      detailsSprite.drawString(strBattery, 4, y);
      detailsSprite.pushSprite(0, displayHeight - 80);
    }

    vTaskDelayUntil(&ticktime, 1000);
  }
}

void callback(char *topic, byte *payloadRaw, unsigned int length) {
  secondsSinceLastUpdate = 0;

  auto strTopic = String(topic);
  auto pos = strTopic.lastIndexOf('/');
  assert(pos != -1);
  auto strSensor = strTopic.substring(pos + 1);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String json((const char *)payloadRaw, length);
  Serial.println(json);

  DynamicJsonDocument doc(1024);
  auto rc = deserializeJson(doc, json);
  assert(rc == DeserializationError::Ok);

  reading_t reading{.sensor = strSensor,
                    .temperature = doc["temp"].as<float>(),
                    .humidity = doc["hum"].as<float>()};
  xQueueOverwrite(qhReading, &reading);
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

  semDisplay = xSemaphoreCreateBinary();
  assert(semDisplay);

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

  tft.init();
  tft.fillScreen(TFT_WHITE);
  tft.setRotation(1);
  std::swap(displayHeight, displayWidth);
  tft.setSwapBytes(true);

  // Set initial TFT displayBrightness
  ledcSetup(0, 10000, 8);
  ledcAttachPin(38, 0);
  ledcWrite(0, displayBrightness);

  tft.pushImage(16, 8, 32, 64, thermometer);
  tft.pushImage(160, 12, 32, 40, humidity);

  WiFi.begin(ssid, password);

  uint16_t retryCount = 8;
  while (WiFi.status() != WL_CONNECTED && retryCount-- != 0) {
    log_d("WiFi not connected");
    delay(500);
  }

  log_i("WiFi connected: %s", WiFi.localIP().toString().c_str());

  // Get system time from NTP server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Configure MQTT server connection
  pubSubClient.setServer(mqttServer, MQTT_PORT);
  pubSubClient.setCallback(callback);

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
