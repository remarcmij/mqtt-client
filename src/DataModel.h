#pragma once
#include "IView.h"
#include <Arduino.h>
#include <deque>
#include <vector>

#define SAMPLES_PER_DATAPOINT 4
#define MAX_DATAPOINTS 300

struct Datapoint {
  float temperature;
  float humidity;
  Datapoint(float temperature, float humidity)
      : temperature{temperature}, humidity{humidity} {}
};

struct SensorStats {
  String sensorTypeName;
  String sensorLocation;
  float temperature;
  float humidity;
  uint32_t battery;
  std::deque<Datapoint> samples;
  std::deque<Datapoint> datapoints;
  SensorStats(const String &sensorTypeName, const String &sensorLocation)
      : sensorTypeName{sensorTypeName}, sensorLocation{sensorLocation} {}
};

struct ViewModel {
  String sensorTypeName;
  String sensorLocation;
  float temperature;
  uint32_t battery;
  float minTemperature;
  float maxTemperature;
  float humidity;
  float minHumidity;
  float maxHumidity;
  std::vector<Datapoint> datapoints;
};

typedef void (*displayCallback_t)(const SensorStats &sensorStats);

class DataModel {
public:
  void setView(IView *view);
  void mqttUpdate(char *topic, byte *payloadRaw, unsigned int length);
  void nextSensor();
  ViewModel getViewModel();

private:
  IView *view_;
  int sensorIndex_{0};
  std::vector<SensorStats> sensorStats_{};
  SemaphoreHandle_t mutex_{xSemaphoreCreateMutex()};
};