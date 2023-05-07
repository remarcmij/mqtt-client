#pragma once
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
  QueueHandle_t qhViewModel_;
  uint32_t updateCounter_{0};
  uint32_t pageIndex_{0};

  void renderMainPage_(const ViewModel &viewModel);
  void renderGraphPage_(const ViewModel &viewModel, GraphType graphType);
};