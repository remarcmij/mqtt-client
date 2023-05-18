#include "View.h"
#include "DataModel.h"
#include "esp_adc_cal.h"
#include "humidity.h"
#include "thermometer.h"
#include <Arduino.h>
#include <cmath>
#include <ctime>

#include "Calibri32.h"
#include "CalibriBold20.h"
#include "NotoSansBold15.h"
#define small NotoSansBold15
#define medium CalibriBold20
#define large Calibri32

#define NUM_PAGES 3
#define HOURS_PER_DIVISION 4
#define SECS_PER_HOUR 3600
#define PIXELS_PER_HOUR 15
#define BAT_ADC 4

uint32_t getBatteryCharge(uint32_t voltage);

#define SAMPLE_INTERVAL_SECS 60 // one minute
const uint32_t datapointInterval_secs =
    SAMPLES_PER_DATAPOINT * SAMPLE_INTERVAL_SECS;

const uint32_t backgroundColor = TFT_WHITE;

static uint32_t readADC_Cal(int ADC_Raw) {
  esp_adc_cal_characteristics_t adc_chars;

  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100,
                           &adc_chars);
  return (esp_adc_cal_raw_to_voltage(ADC_Raw, &adc_chars));
}

/* clang-format off */
View::View(uint32_t width, uint32_t height, DataModel& dataModel) : 
  width_{width}, 
  height_{height}, 
  dataModel_{dataModel},
  tft_{TFT_eSPI()},
  customGreen_{tft_.color565(0, 204, 0)} {
    dataModel.setView(this);
  }
/* clang-format on */

void View::init() {
  tft_.init();
  tft_.setRotation(1);
  std::swap(width_, height_);
}

void View::update(uint16_t sensorId) {
  if (currentSensorId_ == 0) {
    currentSensorId_ = sensorId;
  }
  if (sensorId == currentSensorId_) {
    vm_ = dataModel_.getViewModel(sensorId);
    render_();
    updateCounter_ = 0;
  }
}

void View::updateTask(void *param) {
  auto ticktime = xTaskGetTickCount();
  for (;;) {
    render_();
    vTaskDelayUntil(&ticktime, 1000);
  }
}

void View::nextPage() {
  pageIndex_ = (pageIndex_ + 1) % NUM_PAGES;
  render_();
}

void View::nextSensor() {
  auto sensorIds = dataModel_.getSensorIds();

  for (int i = 0; i < sensorIds.size(); i++) {
    if (sensorIds[i] == currentSensorId_) {
      int next = (i + 1) % sensorIds.size();
      currentSensorId_ = sensorIds[next];
      // Reset to initial page
      pageIndex_ = 0;
      update(currentSensorId_);
      return;
    }
  }

  assert(false);
}

void View::incrementDisconnects() { disconnectCount_++; }

void View::render_() {
  xSemaphoreTake(mutex_, portMAX_DELAY);

  switch (pageIndex_) {
  case 1:
    renderGraphPage_(GraphType::Temperature);
    break;
  case 2:
    renderGraphPage_(GraphType::Humidity);
    break;
  default:
    renderMainPage_();
  }

  xSemaphoreGive(mutex_);
}

void View::renderMainPage_() {
  auto displaySprite = TFT_eSprite(&tft_);
  auto detailSprite = TFT_eSprite(&tft_);

  displaySprite.createSprite(width_, height_);
  displaySprite.setSwapBytes(true);
  displaySprite.fillSprite(TFT_WHITE);

  displaySprite.pushImage(16, 8, 32, 64, thermometer);
  displaySprite.pushImage(160, 12, 32, 40, humidity);

  displaySprite.loadFont(large);
  displaySprite.setTextColor(TFT_BLACK, backgroundColor);
  auto strTemperature = String(vm_.temperature, 1) + "°C";
  displaySprite.drawString(strTemperature, 56, 22);
  auto strHumidity = String(vm_.humidity, 0) + "%";
  displaySprite.drawString(strHumidity, 198, 22);
  displaySprite.drawString(vm_.sensorLocation, 56, 52);

  displaySprite.loadFont(small);
  displaySprite.setTextDatum(TR_DATUM);
  auto chargePercent = getBatteryCharge(vm_.battery);
  if (chargePercent <= 10) {
    displaySprite.setTextColor(TFT_RED);
  } else if (chargePercent <= 15) {
    displaySprite.setTextColor(TFT_ORANGE);
  } else {
    displaySprite.setTextColor(customGreen_, TFT_WHITE);
  }
  auto strBatttery = "BAT: " + String(chargePercent) + "%";

  displaySprite.drawString(strBatttery, width_ - 4, 4);

  detailSprite.createSprite(width_, 80);
  detailSprite.fillSprite(TFT_DARKGREY);
  detailSprite.loadFont(small);
  detailSprite.setTextColor(TFT_WHITE, TFT_BLACK);

  int32_t y = 4;
  auto strMinMax = "MIN: " + String(vm_.minTemperature, 1);
  strMinMax += "°C, MAX: " + String(vm_.maxTemperature, 1) + "°C";
  detailSprite.drawString(strMinMax, 4, y);

  y += 17;
  auto strCounter = "Counter: " + String(updateCounter_++);
  strCounter += ", Disconnects: " + String(disconnectCount_);
  detailSprite.drawString(strCounter, 4, y);

  y += 17;
  uint32_t battery_mv = readADC_Cal(analogRead(BAT_ADC)) * 2;

  // auto battery_mv = (analogRead(4) * 2 * 3.3 * 1000) / 4096;
  auto strBattery2 =
      "Display BAT: " + String(getBatteryCharge(battery_mv)) + "%";
  detailSprite.drawString(strBattery2, 4, y);

  y += 17;
  tm timeinfo;
  char buf[64];
  getLocalTime(&timeinfo);
  strftime(buf, sizeof(buf), "%c", &timeinfo);
  detailSprite.drawString(buf, 4, y);

  detailSprite.pushToSprite(&displaySprite, 0, height_ - 80);
  detailSprite.deleteSprite();

  displaySprite.pushSprite(0, 0);
}

void View::renderGraphPage_(GraphType graphType) {
  const float temperatureMargin = 0.5;
  const float humidityMargin = 1.0;
  const int axis_px = 20;

  float minValue, maxValue;
  if (graphType == GraphType::Temperature) {
    minValue = std::floor(vm_.minTemperature - temperatureMargin);
    maxValue = std::ceil(vm_.maxTemperature + temperatureMargin);
  } else {
    minValue = std::floor(vm_.minHumidity - humidityMargin);
    maxValue = std::ceil(vm_.maxHumidity + humidityMargin);
  }

  auto valueRange = maxValue - minValue;

  float scalingFactor = static_cast<float>(height_ - axis_px) / (valueRange);

  auto displaySprite = TFT_eSprite(&tft_);
  auto graphSprite = TFT_eSprite(&tft_);

  displaySprite.createSprite(width_, height_);
  displaySprite.setSwapBytes(true);
  displaySprite.fillSprite(TFT_WHITE);

  graphSprite.createSprite(width_, height_);
  graphSprite.fillSprite(TFT_BLACK);

  graphSprite.drawFastVLine(axis_px, 0, height_ - axis_px, TFT_LIGHTGREY);

  auto step = (height_ - axis_px) / static_cast<uint32_t>(valueRange);
  // log_d("scalingFactor: %.2f, valueRange: %.2f, step: %u", scalingFactor,
  //       valueRange, step);

  // draw horizontal grid lines with their y-axis value label
  uint32_t y;
  graphSprite.setTextDatum(MR_DATUM);
  graphSprite.loadFont(small);
  for (uint32_t i = 0; i < height_ - axis_px; i += step) {
    y = height_ - axis_px - i;
    graphSprite.drawFastHLine(axis_px, y, width_, TFT_LIGHTGREY);
    auto labelValue = (i / scalingFactor) + minValue;
    graphSprite.drawFloat(labelValue, 0, axis_px - 2, y);
  }

  int graphWidth = width_ - axis_px;
  time_t per_division_secs = HOURS_PER_DIVISION * SECS_PER_HOUR;
  float xScalingFactor =
      graphWidth / static_cast<float>(datapointInterval_secs * MAX_DATAPOINTS);

  time_t now = time(nullptr);
  tm time_buf;
  localtime_r(&now, &time_buf);
  time_buf.tm_min = 0;
  time_buf.tm_sec = 0;

  time_t wholeHour = mktime(&time_buf);
  int x = width_ - 1 - PIXELS_PER_HOUR * (now - wholeHour) / SECS_PER_HOUR;

  int hour = time_buf.tm_hour;
  graphSprite.setTextDatum(BC_DATUM);
  char buf[8];

  while (x > axis_px) {
    graphSprite.drawFastVLine(x, 0, height_ - axis_px, TFT_LIGHTGREY);
    snprintf(buf, sizeof(buf), "%02d:00", hour);
    graphSprite.drawString(buf, x, height_ - 1);

    hour -= HOURS_PER_DIVISION;
    if (hour < 0) {
      hour += 24;
    }

    x -= 60;
  }

  x = width_;
  std::for_each(
      vm_.datapoints.rbegin(), vm_.datapoints.rend(), [&](const auto &dp) {
        float value =
            graphType == GraphType::Temperature ? dp.temperature : dp.humidity;
        float scaledValue = std::round((value - minValue) * scalingFactor);
        uint32_t y = static_cast<uint32_t>(scaledValue);
        graphSprite.fillCircle(--x, height_ - y - axis_px, 2, TFT_RED);
      });

  graphSprite.pushToSprite(&displaySprite, 0, 0);

  displaySprite.pushSprite(0, 0);
}
