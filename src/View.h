#pragma once
#include "DataModel.h"
#include "IView.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

enum class GraphType { Temperature, Humidity };

class View : IView {
public:
  View(uint32_t width, uint32_t height);
  void init();
  virtual void update(const ViewModel &viewModel) override;
  void updateTask(void *param);
  void nextPage();

private:
  uint32_t width_;
  uint32_t height_;
  TFT_eSPI tft_;
  SemaphoreHandle_t mutex_;
  uint32_t updateCounter_{0};
  uint32_t pageIndex_{0};
  ViewModel viewModel_;

  void renderMainPage_(const ViewModel &vm);
  void renderGraphPage_(const ViewModel &vm, GraphType graphType);
};