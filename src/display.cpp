#include "display.h"
#include "DataManager.h"
#include "humidity.h"
#include "pin_config.h"
#include "thermometer.h"
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "Calibri32.h"
#include "NotoSansBold15.h"
#define digits Calibri32
#define small NotoSansBold15

extern QueueHandle_t qhReading;
extern uint32_t updateCounter;

namespace {
auto tft = TFT_eSPI();
auto tempSprite = TFT_eSprite(&tft);
auto humSprite = TFT_eSprite(&tft);
auto detailsSprite = TFT_eSprite(&tft);

const uint32_t backgroundColor = TFT_WHITE;

uint32_t displayHeight = TFT_HEIGHT;
uint32_t displayWidth = TFT_WIDTH;
uint32_t displayBrightness = 50;
} // namespace

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

void displayTask(void *pvParameters) {
  auto *dataManager = static_cast<DataManager *>(pvParameters);
  UBaseType_t hwmStack = 0;
  uint32_t freeHeapSize = 0;

  auto ticktime = xTaskGetTickCount();

  for (;;) {
    dataManager->requestAccess();
    const auto &stats = dataManager->getCurrentStats();
    auto strTemperature = String(stats.temperature, 1) + "°C";

    tempSprite.createSprite(90, 26);
    tempSprite.fillSprite(backgroundColor);
    tempSprite.loadFont(digits);
    tempSprite.setTextColor(TFT_BLACK, backgroundColor);
    tempSprite.drawString(strTemperature, 0, 0, 4);
    tempSprite.pushSprite(56, 22);

    auto strHumidity = String(stats.humidity, 0) + "%";

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
    auto strLocation = "Location: " + stats.sensorLocation;
    detailsSprite.drawString(strLocation, 4, y);

    y += 17;
    auto strCounter = "Counter: " + String(updateCounter++);
    detailsSprite.drawString(strCounter, 4, y);

    y += 17;
    auto strMinMax = "MIN: " + String(stats.minTemperature, 1);
    strMinMax += "°C, MAX: " + String(stats.maxTemperature, 1) + "°C";
    detailsSprite.drawString(strMinMax, 4, y);

    auto battery_mv = (analogRead(4) * 2 * 3.3 * 1000) / 4096;
    y += 17;
    auto strBattery = "Battery: " + String(battery_mv / 1000., 2) + "V";
    detailsSprite.drawString(strBattery, 4, y);
    detailsSprite.pushSprite(0, displayHeight - 80);

    dataManager->releaseAccess();

    vTaskDelayUntil(&ticktime, 1000);
  }
}

void initDisplay() {
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
}
