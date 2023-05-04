#include "View.h"
#include "DataManager.h"
#include "humidity.h"
#include "thermometer.h"

#include "Calibri32.h"
#include "NotoSansBold15.h"
#define digits Calibri32
#define small NotoSansBold15

const uint32_t backgroundColor = TFT_WHITE;

View::View(uint32_t width, uint32_t height)
    : width_{width}, height_{height}, tft_{TFT_eSPI()},
      displaySprite_{TFT_eSprite(&tft_)}, tempSprite_{TFT_eSprite(&tft_)},
      humSprite_{TFT_eSprite(&tft_)}, detailsSprite_{TFT_eSprite(&tft_)},
      updateCounter_{0}, qhViewModel_{xQueueCreate(1, sizeof(ViewModel))} {}

void View::init() {
  tft_.init();
  tft_.setRotation(1);
  std::swap(width_, height_);

  // Set initial TFT displayBrightness
  // ledcSetup(0, 10000, 8);
  // ledcAttachPin(38, 0);
  // ledcWrite(0, displayBrightness);
}

void View::update(const ViewModel &viewModel) {
  auto rc = xQueueOverwrite(qhViewModel_, &viewModel);
  assert(rc == pdPASS);
  updateCounter_ = 0;
}

void View::updateTask(void *param) {

  auto *dataManager = static_cast<DataManager *>(param);
  dataManager->setView(this);

  UBaseType_t hwmStack = 0;
  uint32_t freeHeapSize = 0;
  ViewModel viewModel;

  auto ticktime = xTaskGetTickCount();

  for (;;) {
    auto rc = xQueuePeek(qhViewModel_, &viewModel, 0);
    if (rc == pdPASS) {
      displaySprite_.createSprite(width_, height_);
      displaySprite_.setSwapBytes(true);
      displaySprite_.fillSprite(TFT_WHITE);

      displaySprite_.pushImage(16, 8, 32, 64, thermometer);
      auto strTemperature = String(viewModel.temperature, 1) + "°C";
      tempSprite_.createSprite(90, 26);
      tempSprite_.fillSprite(backgroundColor);
      tempSprite_.loadFont(digits);
      tempSprite_.setTextColor(TFT_BLACK, backgroundColor);
      tempSprite_.drawString(strTemperature, 0, 0, 4);
      tempSprite_.pushToSprite(&displaySprite_, 56, 22);

      displaySprite_.pushImage(160, 12, 32, 40, humidity);
      auto strHumidity = String(viewModel.humidity, 0) + "%";
      humSprite_.createSprite(60, 26);
      humSprite_.fillSprite(backgroundColor);
      humSprite_.loadFont(digits);
      humSprite_.setTextColor(TFT_BLACK, backgroundColor);
      humSprite_.drawString(strHumidity, 0, 0, 4);
      humSprite_.pushToSprite(&displaySprite_, 198, 22);

      detailsSprite_.createSprite(width_, 80);
      detailsSprite_.fillSprite(TFT_DARKGREY);
      detailsSprite_.loadFont(small);
      detailsSprite_.setTextColor(TFT_WHITE, TFT_DARKGREY);

      int32_t y = 4;
      auto strLocation = "Location: " + viewModel.sensorLocation;
      detailsSprite_.drawString(strLocation, 4, y);

      y += 17;
      auto strCounter = "Counter: " + String(updateCounter_++);
      detailsSprite_.drawString(strCounter, 4, y);

      y += 17;
      auto strMinMax = "MIN: " + String(viewModel.minTemperature, 1);
      strMinMax += "°C, MAX: " + String(viewModel.maxTemperature, 1) + "°C";
      detailsSprite_.drawString(strMinMax, 4, y);

      auto battery_mv = (analogRead(4) * 2 * 3.3 * 1000) / 4096;
      y += 17;
      auto strBattery = "Battery: " + String(battery_mv / 1000., 2) + "V";
      detailsSprite_.drawString(strBattery, 4, y);
      detailsSprite_.pushToSprite(&displaySprite_, 0, height_ - 80);

      displaySprite_.pushSprite(0, 0);
    }

    vTaskDelayUntil(&ticktime, 1000);
  }
}