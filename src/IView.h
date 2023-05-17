#pragma once
#include <Arduino.h>

class IView {
public:
  virtual void update(uint16_t sensorId) = 0;
};
