#include "DataModel.h"
#include "ArduinoJson.h"
#include <algorithm>
#include <optional>

void DataModel::setView(IView *view) { view_ = view; }

void DataModel::mqttUpdate(char *topic, byte *payloadRaw, unsigned int length) {
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

  datapoint_t sensorSample{.temperature = temperature, .humidity = humidity};

  stats->samples_.push_back(sensorSample);
  sampleCount_++;
  if (stats->samples_.size() > SAMPLES_PER_DATAPOINT * 2) {
    stats->samples_.pop_front();
  }

  if (sampleCount_ == SAMPLES_PER_DATAPOINT) {
    // Compute average temperature and humidity for
    // the last 6 samples (= last 6 minutes)
    float temperatureTotal{};
    float humidityTotal{};
    for (const auto &sample : stats->samples_) {
      temperatureTotal += sample.temperature;
      humidityTotal += sample.humidity;
    }
    float temperatureAvg = temperatureTotal / stats->samples_.size();
    float humidityAvg = humidityTotal / stats->samples_.size();

    log_d("average temperature: %.1f", temperatureAvg);
    log_d("average sample humidity: %.0f", humidityAvg);

    datapoint_t dp{.temperature = temperatureAvg, .humidity = humidityAvg};

    stats->datapoints.push_back(dp);
    if (stats->datapoints.size() > MAX_DATAPOINTS) {
      stats->datapoints.pop_front();
    }
    sampleCount_ = 0;
  }

  ViewModel vm{};
  vm.sensorTypeName = stats->sensorTypeName;
  vm.sensorLocation = stats->sensorLocation;
  vm.temperature = vm.minTemperature = vm.maxTemperature = temperature;
  vm.humidity = vm.minHumidity = vm.maxHumidity = humidity;

  for (const auto &sample : stats->samples_) {
    vm.minTemperature = std::min(vm.minTemperature, sample.temperature);
    vm.maxTemperature = std::max(vm.maxTemperature, sample.temperature);
    vm.minHumidity = std::min(vm.minHumidity, sample.humidity);
    vm.maxHumidity = std::max(vm.maxHumidity, sample.humidity);
  }

  for (const auto &dp : stats->datapoints) {
    vm.minTemperature = std::min(vm.minTemperature, dp.temperature);
    vm.maxTemperature = std::max(vm.maxTemperature, dp.temperature);
    vm.minHumidity = std::min(vm.minHumidity, dp.humidity);
    vm.maxHumidity = std::max(vm.maxHumidity, dp.humidity);
  }

  for (const auto &dp : stats->datapoints) {
    vm.datapoints.emplace_back(dp);
  }

  view_->update(vm);

  log_d("samples: %u, datapoints: %u, sensors: %u", sampleCount_,
        stats->datapoints.size(), sensorStats_.size());
}