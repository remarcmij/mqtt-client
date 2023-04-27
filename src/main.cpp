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

#include "Calibri32.h"
#include "NotoSansBold15.h"
#define digits Calibri32
#define small NotoSansBold15

#define MQTT_PORT 1883
#define UPPER_BUTTON_PIN 14
#define LOWER_BUTTON_PIN 0

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqttServer = MQTT_SERVER;

const char *ntpServer = "time.google.com";
const long gmtOffset_sec = 3600 * 1;
const int daylightOffset_sec = 3600 * 1;

// Display rotated
const uint32_t displayHeight = TFT_WIDTH;
const uint32_t displayWidth = TFT_HEIGHT;

const uint32_t backgroundColor = TFT_WHITE;

struct payload_t {
  uint32_t bootCount;
  uint32_t skipCount;
  String timestamp;
  String location;
  float temperature;
  float humidity;
};

struct reading_t {
  String sensor;
  String location;
  float temperature;
  float humidity;
};

namespace {
int app_cpu = 0;
QueueHandle_t qhReading = nullptr;
SemaphoreHandle_t semDisplay = nullptr;

uint32_t targetTime = 0;
uint32_t secondsSinceLastMessage = 0;

auto tft = TFT_eSPI();
auto tempSprite = TFT_eSprite(&tft);
auto humSprite = TFT_eSprite(&tft);
auto detailsSprite = TFT_eSprite(&tft);

WiFiClient wifiClient;
PubSubClient client(wifiClient);

uint8_t messageCount{};
uint32_t volt;

OneButton lowerButton(LOWER_BUTTON_PIN);
OneButton upperButton(UPPER_BUTTON_PIN);

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    log_d("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "esp32client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      log_d("connected");
      client.subscribe("/home/sensors");
    } else {
      log_e("Could not connect to MQTT server, rc=%d", client.state());
      delay(5000);
    }
  }
}

void displayTask(void *argp) {
  BaseType_t rc;
  reading_t reading;
  UBaseType_t hwmStack = 0;

  for (;;) {
    if (targetTime < millis()) {
      // Set next update for 1 second later
      targetTime = millis() + 1000;

      rc = xQueuePeek(qhReading, &reading, 0);
      if (rc == pdPASS) {
        String strTemperature{String(reading.temperature, 1) + "°C"};

        tempSprite.createSprite(90, 26);
        tempSprite.fillSprite(backgroundColor);
        tempSprite.loadFont(digits);
        tempSprite.setTextColor(TFT_BLACK, backgroundColor);
        tempSprite.drawString(strTemperature, 0, 0, 4);
        tempSprite.pushSprite(56, 22);

        String strHumidity{String(reading.humidity, 0) + "%"};

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
        // String strLocation{"Location: " + payload.location};
        // detailsSprite.drawString(strLocation, 4, y);

        // y += 17;
        // String strSample{"Sample: " + String(payload.bootCount)};
        // detailsSprite.drawString(strSample, 4, y);

        // y += 17;
        String strCounter{"Counter: " + String(secondsSinceLastMessage++)};
        detailsSprite.drawString(strCounter, 4, y);

        volt = (analogRead(4) * 2 * 3.3 * 1000) / 4096;
        y += 17;
        String strBattery{"Battery: " + String(volt) + "mv"};
        detailsSprite.drawString(strBattery, 4, y);
        detailsSprite.pushSprite(0, displayHeight - 80);
      }
    }

    auto hwmCurrent = uxTaskGetStackHighWaterMark(nullptr);
    if (!hwmStack || hwmCurrent > hwmStack) {
      hwmStack = hwmCurrent;
      log_d("displayTask stack highwater mark %u", hwmStack);
    }

    delay(10);
  }
}

void callback(char *topic, byte *payloadRaw, unsigned int length) {
  secondsSinceLastMessage = 0;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String json((const char *)payloadRaw, length);
  Serial.println(json);

  DynamicJsonDocument doc(1024);
  auto rc = deserializeJson(doc, json);
  assert(rc == DeserializationError::Ok);

  reading_t reading{.sensor = doc["sen"].as<String>(),
                    .location = doc["loc"].as<String>(),
                    .temperature = doc["temp"].as<float>(),
                    .humidity = doc["hum"].as<float>()};
  xQueueOverwrite(qhReading, &reading);

  // payload_t payload{.bootCount = doc["boot"].as<uint32_t>(),
  //                   .skipCount = doc["skip"].as<uint32_t>(),
  //                   .timestamp = doc["time"].as<String>(),
  //                   .location = doc["loc"].as<String>(),
  //                   .temperature = doc["temp"].as<float>(),
  //                   .humidity = doc["hum"].as<float>()};

  // displayTask(payload);
}

} // namespace

void handleLowerClick() { log_d("Lower button pressed"); }

void setup() {
  BaseType_t rc;

  app_cpu = xPortGetCoreID();

  semDisplay = xSemaphoreCreateBinary();
  assert(semDisplay);

  qhReading = xQueueCreate(1, sizeof(reading_t));

  Serial.begin(115200);

  // Enable battery
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  lowerButton.attachClick(handleLowerClick);
  // pinMode(LOWER_BUTTON_PIN, INPUT_PULLUP);
  // pinMode(UPPER_BUTTON_PIN, INPUT_PULLUP);

  tft.init();
  tft.fillScreen(TFT_WHITE);
  tft.setRotation(1);
  tft.setSwapBytes(true);

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

  // Connect to MQTT server
  client.setServer(mqttServer, MQTT_PORT);
  client.setCallback(callback);

  rc = xTaskCreatePinnedToCore(displayTask, "display", 2000, nullptr, 1,
                               nullptr, app_cpu);
  assert(rc == pdPASS);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();
  lowerButton.tick();
  upperButton.tick();

  taskYIELD();
}
