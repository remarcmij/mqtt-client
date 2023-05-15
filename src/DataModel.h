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
  float temperature;
  float humidity;
  std::deque<datapoint_t> samples;
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
  void nextSensor();
  ViewModel getViewModel();

private:
  IView *view_;
  int sensorIndex_{0};
  std::vector<sensorStats_t> sensorStats_{};
  SemaphoreHandle_t mutex_{xSemaphoreCreateMutex()};
};