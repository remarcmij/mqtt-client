#include "View.h"
#include "DataModel.h"
#include "humidity.h"
#include "thermometer.h"
#include <cmath>

#include "Calibri32.h"
#include "CalibriBold20.h"
#include "NotoSansBold15.h"
#define small NotoSansBold15
#define medium CalibriBold20
#define large Calibri32

#define NUM_PAGES 2

const uint32_t backgroundColor = TFT_WHITE;

/* clang-format off */
View::View(uint32_t width, uint32_t height) : 
  width_{width}, height_{height}, 
  tft_{TFT_eSPI()},
  qhViewModel_{xQueueCreate(1, sizeof(ViewModel))} {
    // Nothing to do here
  }
/* clang-format on */

void View::init() {
  tft_.init();
  tft_.setRotation(1);
  std::swap(width_, height_);
}

void View::update(const ViewModel &viewModel) {
  auto rc = xQueueOverwrite(qhViewModel_, &viewModel);
  assert(rc == pdPASS);
  updateCounter_ = 0;
}

void View::updateTask(void *param) {
  auto *dataModel = static_cast<DataModel *>(param);
  dataModel->setView(this);

  ViewModel viewModel;

  auto ticktime = xTaskGetTickCount();

  for (;;) {
    auto rc = xQueuePeek(qhViewModel_, &viewModel, 0);
    if (rc == pdPASS) {
      switch (pageIndex_) {
      case 1:
        renderGraphPage_(viewModel, GraphType::Temperature);
        break;
      case 2:
        renderGraphPage_(viewModel, GraphType::Humidity);
        break;
      default:
        renderMainPage_(viewModel);
      }
    }

    vTaskDelayUntil(&ticktime, 1000);
  }
}

void View::nextPage() { pageIndex_ = (pageIndex_ + 1) % NUM_PAGES; }

void View::renderMainPage_(const ViewModel &viewModel) {
  auto fullScreenSprite = TFT_eSprite(&tft_);
  auto sprite = TFT_eSprite(&tft_);

  fullScreenSprite.createSprite(width_, height_);
  fullScreenSprite.setSwapBytes(true);
  fullScreenSprite.fillSprite(TFT_WHITE);

  fullScreenSprite.pushImage(16, 8, 32, 64, thermometer);
  auto strTemperature = String(viewModel.temperature, 1) + "°C";
  // sprite.createSprite(90, 26);
  // sprite.fillSprite(backgroundColor);
  fullScreenSprite.loadFont(large);
  fullScreenSprite.setTextColor(TFT_BLACK, backgroundColor);
  fullScreenSprite.drawString(strTemperature, 56, 22, 4);
  // sprite.pushToSprite(&fullScreenSprite, 56, 22);
  // sprite.deleteSprite();

  fullScreenSprite.pushImage(160, 12, 32, 40, humidity);
  auto strHumidity = String(viewModel.humidity, 0) + "%";
  sprite.createSprite(60, 26);
  sprite.fillSprite(backgroundColor);
  sprite.loadFont(large);
  sprite.setTextColor(TFT_BLACK, backgroundColor);
  sprite.drawString(strHumidity, 0, 0, 4);
  sprite.pushToSprite(&fullScreenSprite, 198, 22);
  sprite.deleteSprite();

  sprite.createSprite(width_, 80);
  sprite.fillSprite(TFT_DARKGREY);
  sprite.loadFont(small);
  sprite.setTextColor(TFT_WHITE, TFT_DARKGREY);

  int32_t y = 4;
  auto strLocation = "Location: " + viewModel.sensorLocation;
  sprite.drawString(strLocation, 4, y);

  y += 17;
  auto strCounter = "Counter: " + String(updateCounter_++);
  sprite.drawString(strCounter, 4, y);

  y += 17;
  auto strMinMax = "MIN: " + String(viewModel.minTemperature, 1);
  strMinMax += "°C, MAX: " + String(viewModel.maxTemperature, 1) + "°C";
  sprite.drawString(strMinMax, 4, y);

  auto battery_mv = (analogRead(4) * 2 * 3.3 * 1000) / 4096;
  y += 17;
  auto strBattery = "Battery: " + String(battery_mv / 1000., 2) + "V";
  sprite.drawString(strBattery, 4, y);
  sprite.pushToSprite(&fullScreenSprite, 0, height_ - 80);
  sprite.deleteSprite();

  fullScreenSprite.pushSprite(0, 0);
}

void View::renderGraphPage_(const ViewModel &viewModel, GraphType graphType) {
  const uint32_t graphHeight = 120;
  const float temperatureMargin = 1.0;
  const float humidityMargin = 2.0;

  float minValue, maxValue;
  if (graphType == GraphType::Temperature) {
    minValue = std::floor(viewModel.minTemperature - temperatureMargin);
    maxValue = std::ceil(viewModel.maxTemperature + temperatureMargin);
  } else {
    minValue = std::floor(viewModel.minHumidity - humidityMargin);
    maxValue = std::ceil(viewModel.maxHumidity + humidityMargin);
  }

  float scalingFactor = static_cast<float>(graphHeight) / (maxValue - minValue);

  auto fullScreenSprite = TFT_eSprite(&tft_);
  auto graphSprite = TFT_eSprite(&tft_);

  fullScreenSprite.createSprite(width_, height_);
  fullScreenSprite.setSwapBytes(true);
  fullScreenSprite.fillSprite(TFT_WHITE);

  graphSprite.createSprite(width_, graphHeight);
  graphSprite.fillSprite(TFT_BLACK);

  uint32_t x = 20;
  for (const auto &datapoint : *(viewModel.datapoints)) {
    float value = graphType == GraphType::Temperature ? datapoint.temperature
                                                      : datapoint.humidity;
    float scaledValue = std::round((value - minValue) * scalingFactor);
    uint32_t y = static_cast<uint32_t>(scaledValue);
    graphSprite.drawFastVLine(x, graphHeight - y - 1, 3, TFT_RED);
    x++;
  }

  graphSprite.pushToSprite(&fullScreenSprite, 0, height_ - graphHeight);

  fullScreenSprite.pushSprite(0, 0);
}
