#include "data.h"
#include "ArduinoJson.h"
#include <Arduino.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <time.h>
#include <vector>

#define MAX_DATAPOINTS 240

extern QueueHandle_t qhReading;
extern uint32_t updateCounter;

namespace {
int sensorIndex{-1};
std::vector<std::shared_ptr<sensorStats_t>> sensorStats{};
} // namespace

void mqttCallback(char *topic, byte *payloadRaw, unsigned int length) {
  Serial.print("[");
  Serial.print(topic);
  Serial.print("] ");

  String json((const char *)payloadRaw, length);
  Serial.println(json);

  // Extract sensor location = last path element of topic
  auto strTopic = String(topic);
  auto pos = strTopic.lastIndexOf('/');
  assert(pos != -1);
  auto strLocation = strTopic.substring(pos + 1);

  // Find sensor stats if present
  auto iter = std::find_if(sensorStats.begin(), sensorStats.end(),
                           [strLocation](std::shared_ptr<sensorStats_t> item) {
                             return item->sensorLocation == strLocation;
                           });

  std::shared_ptr<sensorStats_t> stats;

  if (iter == sensorStats.end()) {
    // Create a new entry if not found
    stats = std::make_shared<sensorStats_t>();
    stats->sensorLocation = strLocation;
    stats->minTemp = std::numeric_limits<float>::max();
    stats->maxTemp = std::numeric_limits<float>::min();
    stats->minHumi = std::numeric_limits<float>::max();
    stats->maxTemp = std::numeric_limits<float>::min();
    sensorStats.push_back(stats);
  } else {
    // else use the existing entry
    stats = *iter;
  }

  DynamicJsonDocument doc(1024);
  auto rc = deserializeJson(doc, json);
  assert(rc == DeserializationError::Ok);

  auto strSensor = doc["sen"].as<String>();
  auto temperature = doc["temp"].as<float>();
  auto humidity = doc["hum"].as<float>();

  stats->minTemp = std::min(stats->minTemp, temperature);
  stats->maxTemp = std::max(stats->maxTemp, temperature);
  stats->minHumi = std::min(stats->minHumi, humidity);
  stats->maxHumi = std::max(stats->maxHumi, humidity);

  datapoint_t sensorSample{.timestamp = time(nullptr),
                           .temperature = temperature,
                           .humidity = humidity};

  auto samplesSize = stats->samples.size();

  if (stats->sampleCount >= samplesSize) {
    // Compute average temperature and humidity for
    // the last 6 samples (= last 6 minutes)
    float temperatureTotal{};
    float humidityTotal{};
    for (auto &sample : stats->samples) {
      temperatureTotal += sample.temperature;
      humidityTotal += sample.humidity;
    }
    float temperatureAvg = temperatureTotal / samplesSize;
    float humidityAvg = humidityTotal / samplesSize;

    log_d("average temperature: %.1f", temperatureAvg);
    log_d("average sample humidity: %.0f", humidityAvg);

    datapoint_t datapoint{.timestamp = stats->samples[0].timestamp,
                          .temperature = temperatureAvg,
                          .humidity = humidityAvg};

    stats->datapoints.push_back(datapoint);
    if (stats->datapoints.size() > MAX_DATAPOINTS) {
      stats->datapoints.pop_front();
    }

    stats->sampleCount = 0;
  }

  stats->samples[stats->sampleCount] = sensorSample;
  stats->sampleCount += 1;

  log_d("sample count: %u", stats->sampleCount);
  log_d("vector size: %u", sensorStats.size());
  log_d("stored sensorSample count: %u", stats->datapoints.size());

  updateCounter = 0;

  reading_t reading{.sensor = strSensor,
                    .location = strLocation,
                    .temperature = temperature,
                    .minTemperature = stats->minTemp,
                    .maxTemperature = stats->maxTemp,
                    .humidity = humidity,
                    .minHumidity = stats->minHumi,
                    .maxHumidity = stats->maxHumi};
  xQueueOverwrite(qhReading, &reading);
}