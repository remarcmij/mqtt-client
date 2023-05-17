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
  long id;
  String sensorTypeName;
  String sensorLocation;
  uint32_t sampleCount;
  float temperature;
  float humidity;
  uint32_t battery;
  std::deque<Datapoint> samples;
  std::deque<Datapoint> datapoints;
  SensorStats(long id, const String &sensorTypeName,
              const String &sensorLocation)
      : id{id}, sensorTypeName{sensorTypeName}, sensorLocation{sensorLocation},
        sampleCount{0} {}
};

struct ViewModel {
  String sensorTypeName;
  String sensorLocation;
  uint32_t battery;
  float temperature;
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
  std::vector<uint16_t> getSensorIds();
  ViewModel getViewModel(uint16_t sensorId);

private:
  IView *view_;
  uint16_t nextId_{0};
  std::vector<SensorStats> sensorStats_{};
  SemaphoreHandle_t mutex_{xSemaphoreCreateMutex()};
};