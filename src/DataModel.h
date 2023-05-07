#pragma once
#include "IView.h"
#include <Arduino.h>
#include <deque>
#include <vector>

struct datapoint_t {
  time_t timestamp;
  float temperature;
  float humidity;
};

struct sensorStats_t {
  String sensorTypeName;
  String sensorLocation;
  std::deque<datapoint_t> samples_;
  std::deque<datapoint_t> datapoints;
};

struct ViewModel {
  String sensorTypeName;
  String sensorLocation;
  float temperature;
  float minTemperature;
  float maxTemperature;
  float humidity;
  float minHumidity;
  float maxHumidity;
  std::deque<datapoint_t> *datapoints;
};

typedef void (*displayCallback_t)(const ViewModel &viewModel);

class DataModel {
public:
  void setView(IView *view);
  void mqttUpdate(char *topic, byte *payloadRaw, unsigned int length);

private:
  IView *view_;
  volatile int sensorIndex_{-1};
  uint16_t sampleCount_{0};
  std::vector<sensorStats_t> sensorStats_{};
};