#pragma once
#include <Arduino.h>
#include <list>
#include <vector>

struct datapoint_t {
  time_t timestamp;
  float temperature;
  float humidity;
};

struct sensorStats_t {
  // Share with displayTask
  String sensorTypeName;
  String sensorLocation;
  float temperature;
  float minTemperature;
  float maxTemperature;
  float humidity;
  float minHumidity;
  float maxHumidity;
  std::list<datapoint_t> datapoints;

  // private to DataManager
  size_t sampleCount_;
  std::array<datapoint_t, 6> samples_;
};

class DataManager {
public:
  DataManager();
  void mqttUpdate(char *topic, byte *payloadRaw, unsigned int length);
  const sensorStats_t &getCurrentStats();
  void requestAccess();
  void releaseAccess();

private:
  SemaphoreHandle_t sem_;
  volatile int sensorIndex_;
  std::vector<sensorStats_t> sensorStats_{};
};