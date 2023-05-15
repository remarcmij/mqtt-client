#pragma once
#include "DataModel.h"
#include "IView.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

enum class GraphType { Temperature, Humidity };

class View : IView {
public:
  View(uint32_t width, uint32_t height, DataModel &dataModel);
  void init();
  virtual void update() override;
  void updateTask(void *param);
  void nextPage();
  void incrementDisconnects();

private:
  uint32_t width_;
  uint32_t height_;
  DataModel &dataModel_;
  TFT_eSPI tft_;
  uint32_t updateCounter_{0};
  uint32_t pageIndex_{0};
  uint32_t disconnectCount_{0};
  ViewModel vm_;
  SemaphoreHandle_t mutex_{xSemaphoreCreateMutex()};

  void render_();
  void renderMainPage_();
  void renderGraphPage_(GraphType graphType);
};