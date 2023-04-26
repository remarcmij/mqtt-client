#include "ArduinoJson.h"
#include "humidity.h"
#include "secrets.h"
#include "thermometer.h"
#include <Arduino.h>
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

uint32_t targetTime = 0;
uint32_t secondsSinceLastMessage = 0;

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

auto tft = TFT_eSPI();
auto tempSprite = TFT_eSprite(&tft);
auto humSprite = TFT_eSprite(&tft);
auto detailsSprite = TFT_eSprite(&tft);

WiFiClient wifiClient;
PubSubClient client(wifiClient);

uint8_t messageCount{};

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

void drawData(const payload_t &payload) {
  String strTemperature{String(payload.temperature, 1) + "°C"};

  tempSprite.createSprite(90, 26);
  tempSprite.fillSprite(backgroundColor);
  tempSprite.loadFont(digits);
  tempSprite.setTextColor(TFT_BLACK, backgroundColor);
  tempSprite.drawString(strTemperature, 0, 0, 4);
  tempSprite.pushSprite(56, 22);

  String strHumidity{String(payload.humidity, 0) + "%"};

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
  String strLocation{"Location: " + payload.location};
  detailsSprite.drawString(strLocation, 4, y);

  y += 17;
  String strSample{"Sample: " + String(payload.bootCount)};
  detailsSprite.drawString(strSample, 4, y);

  y += 17;
  String strCounter{"Counter: " + String(secondsSinceLastMessage)};
  detailsSprite.drawString(strCounter, 4, y);

  detailsSprite.pushSprite(0, displayHeight - 80);
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

  payload_t payload{.bootCount = doc["boot"].as<uint32_t>(),
                    .skipCount = doc["skip"].as<uint32_t>(),
                    .timestamp = doc["time"].as<String>(),
                    .location = doc["loc"].as<String>(),
                    .temperature = doc["temp"].as<float>(),
                    .humidity = doc["hum"].as<float>()};

  drawData(payload);
}

void setup() {
  Serial.begin(115200);

  pinMode(LOWER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(UPPER_BUTTON_PIN, INPUT_PULLUP);

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
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (targetTime < millis()) {
    // Set next update for 1 second later
    targetTime = millis() + 1000;
    secondsSinceLastMessage++;
    log_d("counter %u", secondsSinceLastMessage);
  }

  if (digitalRead(UPPER_BUTTON_PIN) == 0) {
    log_d("Upper button pressed");
  }

  if (digitalRead(LOWER_BUTTON_PIN) == 0) {
    log_d("Lower button pressed");
  }
}
