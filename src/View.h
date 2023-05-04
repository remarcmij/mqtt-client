#pragma once
#include "IView.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

class View : IView {
public:
  View(uint32_t width, uint32_t height);
  void init();
  virtual void update(const ViewModel &viewModel) override;
  void updateTask(void *param);

private:
  uint32_t width_;
  uint32_t height_;
  TFT_eSPI tft_;
  TFT_eSprite displaySprite_;
  TFT_eSprite tempSprite_;
  TFT_eSprite humSprite_;
  TFT_eSprite detailsSprite_;
  uint32_t updateCounter_;
  QueueHandle_t qhViewModel_;
};