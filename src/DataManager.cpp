#include "DataManager.h"
#include "ArduinoJson.h"
#include <algorithm>
#include <optional>

#define MAX_DATAPOINTS 240 // Covers 24 hours

DataManager::DataManager() : sensorIndex_{-1} {}

void DataManager::setView(IView *view) { view_ = view; }

void DataManager::mqttUpdate(char *topic, byte *payloadRaw,
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
  std::optional<sensorStats_t *> optStats{};

  for (int i = 0; i < sensorStats_.size(); i++) {
    auto p = &sensorStats_[i];
    if (p->sensorLocation == strLocation &&
        p->sensorTypeName == strSensorTypeName) {
      optStats = p;
      break;
    }
  }

  sensorStats_t *stats{};

  if (!optStats.has_value()) {
    // Create a new entry if not found
    stats = &sensorStats_.emplace_back();
    stats->sensorTypeName = strSensorTypeName;
    stats->sensorLocation = strLocation;

    if (sensorIndex_ == -1) {
      sensorIndex_ = 0;
    }
  } else {
    // else use the existing entry
    stats = optStats.value();
  }

  datapoint_t sensorSample{.timestamp = time(nullptr),
                           .temperature = temperature,
                           .humidity = humidity};

  auto samplesSize = stats->samples_.size();

  if (stats->sampleCount_ >= samplesSize) {
    // Compute average temperature and humidity for
    // the last 6 samples (= last 6 minutes)
    float temperatureTotal{};
    float humidityTotal{};
    for (auto &sample : stats->samples_) {
      temperatureTotal += sample.temperature;
      humidityTotal += sample.humidity;
    }
    float temperatureAvg = temperatureTotal / samplesSize;
    float humidityAvg = humidityTotal / samplesSize;

    log_d("average temperature: %.1f", temperatureAvg);
    log_d("average sample humidity: %.0f", humidityAvg);

    datapoint_t datapoint{.timestamp = stats->samples_[0].timestamp,
                          .temperature = temperatureAvg,
                          .humidity = humidityAvg};

    stats->datapoints.push_back(datapoint);
    if (stats->datapoints.size() > MAX_DATAPOINTS) {
      stats->datapoints.pop_front();
    }

    stats->sampleCount_ = 0;
  }

  stats->samples_[stats->sampleCount_] = sensorSample;
  stats->sampleCount_ += 1;

  ViewModel viewModel{};
  viewModel.sensorTypeName = stats->sensorTypeName;
  viewModel.sensorLocation = stats->sensorLocation;
  viewModel.temperature = viewModel.minTemperature = viewModel.maxTemperature =
      temperature;
  viewModel.humidity = viewModel.minHumidity = viewModel.maxHumidity = humidity;

  for (int i = 0; i < stats->sampleCount_; i++) {
    const auto &sample = stats->samples_[i];
    viewModel.minTemperature =
        std::min(viewModel.minTemperature, sample.temperature);
    viewModel.maxTemperature =
        std::max(viewModel.maxTemperature, sample.temperature);
    viewModel.minHumidity = std::min(viewModel.minHumidity, sample.humidity);
    viewModel.maxHumidity = std::max(viewModel.maxHumidity, sample.humidity);
  }

  for (const auto &datapoint : stats->datapoints) {
    viewModel.minTemperature =
        std::min(viewModel.minTemperature, datapoint.temperature);
    viewModel.maxTemperature =
        std::max(viewModel.maxTemperature, datapoint.temperature);
    viewModel.minHumidity = std::min(viewModel.minHumidity, datapoint.humidity);
    viewModel.maxHumidity = std::max(viewModel.maxHumidity, datapoint.humidity);
  }

  viewModel.datapoints.reserve(viewModel.datapoints.size());

  // Trying this causes an exception
  // std::copy(stats->datapoints.begin(), stats->datapoints.end(),
  //           viewModel.datapoints.begin());

  for (const auto &datapoint : stats->datapoints) {
    viewModel.datapoints.push_back(datapoint);
  }

  view_->update(viewModel);

  log_d("sample count: %u", stats->sampleCount_);
  log_d("vector size: %u", sensorStats_.size());
  log_d("sensorStats count: %u", stats->datapoints.size());
}