#pragma once

struct sensorStats_t;

class IView {
public:
  virtual void update() = 0;
};
