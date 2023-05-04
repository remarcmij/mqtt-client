#pragma once
#include "IView.h"
#include <Arduino.h>
#include <list>
#include <vector>

struct datapoint_t {
  time_t timestamp;
  float temperature;
  float humidity;
};

struct sensorStats_t {
  String sensorTypeName;
  String sensorLocation;
  std::list<datapoint_t> datapoints;
  std::array<datapoint_t, 6> samples_;
  size_t sampleCount_;
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
  std::vector<datapoint_t> datapoints;
};

typedef void (*displayCallback_t)(const ViewModel &viewModel);

class DataManager {
public:
  DataManager();
  void setView(IView *view);
  void mqttUpdate(char *topic, byte *payloadRaw, unsigned int length);

private:
  IView *view_;
  volatile int sensorIndex_;
  std::vector<sensorStats_t> sensorStats_{};
};