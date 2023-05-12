#pragma once
#include "IView.h"
#include <Arduino.h>
#include <deque>
#include <vector>

#define SAMPLES_PER_DATAPOINT 4
#define MAX_DATAPOINTS 300

struct datapoint_t {
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
  std::vector<datapoint_t> datapoints;
};

typedef void (*displayCallback_t)(const sensorStats_t &sensorStats);

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