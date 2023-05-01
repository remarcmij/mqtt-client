#include "DataManager.h"
#include "ArduinoJson.h"
#include <algorithm>
#include <optional>

#define MAX_DATAPOINTS 240 // Covers 24 hours

extern uint32_t updateCounter;

DataManager::DataManager() : sem_{xSemaphoreCreateBinary()}, sensorIndex_{-1} {}

void DataManager::mttqUpdate(char *topic, byte *payloadRaw,
                             unsigned int length) {
  Serial.print("[");
  Serial.print(topic);
  Serial.print("] ");

  String json((const char *)payloadRaw, length);
  Serial.println(json);

  DynamicJsonDocument doc(1024);
  auto rc = deserializeJson(doc, json);
  assert(rc == DeserializationError::Ok);

  auto strSensorTypeName = doc["sen"].as<String>();
  auto temperature = doc["temp"].as<float>();
  auto humidity = doc["hum"].as<float>();

  // Extract sensor location = last path element of topic
  auto strTopic = String(topic);
  auto pos = strTopic.lastIndexOf('/');
  assert(pos != -1);
  auto strLocation = strTopic.substring(pos + 1);

  // Find the corresponding sensor stats for the incoming MQTT message
  std::optional<sensorStats_t *> pFoundStats{};

  for (int i = 0; i < sensorStats_.size(); i++) {
    auto p = &sensorStats_[i];
    if (p->sensorLocation == strLocation &&
        p->sensorTypeName == strSensorTypeName) {
      pFoundStats = p;
      break;
    }
  }

  sensorStats_t *pStats{};

  if (!pFoundStats.has_value()) {
    // Create a new entry if not found
    pStats = &sensorStats_.emplace_back();
    pStats->sensorTypeName = strSensorTypeName;
    pStats->sensorLocation = strLocation;

    if (sensorIndex_ == -1) {
      sensorIndex_ = 0;
    }
  } else {
    // else use the existing entry
    pStats = pFoundStats.value();
  }

  datapoint_t sensorSample{.timestamp = time(nullptr),
                           .temperature = temperature,
                           .humidity = humidity};

  auto samplesSize = pStats->samples_.size();

  if (pStats->sampleCount_ >= samplesSize) {
    // Compute average temperature and humidity for
    // the last 6 samples (= last 6 minutes)
    float temperatureTotal{};
    float humidityTotal{};
    for (auto &sample : pStats->samples_) {
      temperatureTotal += sample.temperature;
      humidityTotal += sample.humidity;
    }
    float temperatureAvg = temperatureTotal / samplesSize;
    float humidityAvg = humidityTotal / samplesSize;

    log_d("average temperature: %.1f", temperatureAvg);
    log_d("average sample humidity: %.0f", humidityAvg);

    datapoint_t datapoint{.timestamp = pStats->samples_[0].timestamp,
                          .temperature = temperatureAvg,
                          .humidity = humidityAvg};

    pStats->datapoints.push_back(datapoint);
    if (pStats->datapoints.size() > MAX_DATAPOINTS) {
      pStats->datapoints.pop_front();
    }

    pStats->sampleCount_ = 0;
  }

  pStats->samples_[pStats->sampleCount_] = sensorSample;
  pStats->sampleCount_ += 1;

  if (pFoundStats.has_value()) {
    requestAccess();
  }

  // Start of shared data multation

  pStats->temperature = pStats->minTemperature = pStats->maxTemperature =
      temperature;
  pStats->humidity = pStats->minHumidity = pStats->maxHumidity = humidity;

  for (int i = 0; i < pStats->sampleCount_; i++) {
    const auto &sample = pStats->samples_[i];
    pStats->minTemperature =
        std::min(pStats->minTemperature, sample.temperature);
    pStats->maxTemperature =
        std::max(pStats->maxTemperature, sample.temperature);
    pStats->minHumidity = std::min(pStats->minHumidity, sample.humidity);
    pStats->maxHumidity = std::max(pStats->maxHumidity, sample.humidity);
  }

  for (const auto &datapoint : pStats->datapoints) {
    pStats->minTemperature =
        std::min(pStats->minTemperature, datapoint.temperature);
    pStats->maxTemperature =
        std::max(pStats->maxTemperature, datapoint.temperature);
    pStats->minHumidity = std::min(pStats->minHumidity, datapoint.humidity);
    pStats->maxHumidity = std::max(pStats->maxHumidity, datapoint.humidity);
  }

  // End of shared data multation

  releaseAccess();

  log_d("sample count: %u", pStats->sampleCount_);
  log_d("vector size: %u", sensorStats_.size());
  log_d("stored sensorSample count: %u", pStats->datapoints.size());

  updateCounter = 0;
}

const sensorStats_t &DataManager::getCurrentStats() {
  return sensorStats_[sensorIndex_];
}

void DataManager::requestAccess() {
  auto rc = xSemaphoreTake(sem_, portMAX_DELAY);
  assert(rc == pdPASS);
}

void DataManager::releaseAccess() {
  // Not an error to "give" if not "taken"
  xSemaphoreGive(sem_);
}