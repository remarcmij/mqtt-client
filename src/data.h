#pragma once
#include <Arduino.h>
#include <array>
#include <list>

struct reading_t {
  String sensor;
  String location;
  float temperature;
  float minTemperature;
  float maxTemperature;
  float humidity;
  float minHumidity;
  float maxHumidity;
};

struct datapoint_t {
  time_t timestamp;
  float temperature;
  float humidity;
};

struct sensorStats_t {
  String sensorLocation;
  float minTemp;
  float maxTemp;
  float minHumi;
  float maxHumi;
  size_t sampleCount;
  std::array<datapoint_t, 6> samples;
  std::list<datapoint_t> datapoints;
};

void mqttCallback(char *topic, byte *payloadRaw, unsigned int length);