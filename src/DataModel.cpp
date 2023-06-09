#include "DataModel.h"
#include "ArduinoJson.h"
#include <algorithm>
#include <ctime>

namespace {
void dpMinMax(ViewModel &vm, const Datapoint &dp) {
  vm.minTemperature = std::min(vm.minTemperature, dp.temperature);
  vm.maxTemperature = std::max(vm.maxTemperature, dp.temperature);
  vm.minHumidity = std::min(vm.minHumidity, dp.humidity);
  vm.maxHumidity = std::max(vm.maxHumidity, dp.humidity);
}
}; // namespace

void DataModel::setView(IView *view) { view_ = view; }

void DataModel::mqttUpdate(char *topic, byte *payloadRaw, unsigned int length) {
  String payload((const char *)payloadRaw, length);
  log_d("[%s] %s", topic, payload.c_str());

  DynamicJsonDocument doc(1024);
  auto rc = deserializeJson(doc, payload);
  assert(rc == DeserializationError::Ok);

  auto strSensorTypeName = doc["sen"].as<String>();
  auto temperature = doc["temp"].as<float>();
  auto humidity = doc["hum"].as<float>();
  auto battery = doc["battery"].as<uint32_t>();

  // Extract sensor location = last path element of topic
  auto strTopic = String(topic);
  auto pos = strTopic.lastIndexOf('/');
  assert(pos != -1);
  auto strLocation = strTopic.substring(pos + 1);

  // Find the corresponding sensor stats for the incoming MQTT message
  auto it = std::find_if(sensorStats_.begin(), sensorStats_.end(),
                         [&](const auto &st) {
                           return st.sensorLocation == strLocation &&
                                  st.sensorTypeName == strSensorTypeName;
                         });

  // Either use an existing sensor stats object or append a new one
  auto &stats =
      it == sensorStats_.end()
          ? sensorStats_.emplace_back(++nextId_, strSensorTypeName, strLocation)
          : *it;

  xSemaphoreTake(mutex_, portMAX_DELAY);

  stats.temperature = temperature;
  stats.humidity = humidity;
  stats.battery = battery;

  stats.samples.emplace_back(temperature, humidity);
  // Include a previous batch of samples to compute
  // some sort of running average.
  if (stats.samples.size() > SAMPLES_PER_DATAPOINT * 2) {
    stats.samples.pop_front();
  }
  stats.sampleCount++;

  if (stats.sampleCount == SAMPLES_PER_DATAPOINT) {
    // Compute average temperature and humidity for
    // the last 6 samples (= last 6 minutes)
    float temperatureTotal{};
    float humidityTotal{};
    for (const auto &sample : stats.samples) {
      temperatureTotal += sample.temperature;
      humidityTotal += sample.humidity;
    }
    float temperatureAvg = temperatureTotal / stats.samples.size();
    float humidityAvg = humidityTotal / stats.samples.size();

    log_d("average temperature: %.2f", temperatureAvg);
    log_d("average sample humidity: %.2f", humidityAvg);

    stats.datapoints.emplace_back(temperatureAvg, humidityAvg);
    if (stats.datapoints.size() > MAX_DATAPOINTS) {
      stats.datapoints.pop_front();
    }

    stats.sampleCount = 0;
  }

  xSemaphoreGive(mutex_);

  view_->update(stats.id);

  log_d("location: %s, samples: %u, datapoints: %u", stats.sensorLocation,
        stats.sampleCount, stats.datapoints.size());
}

std::vector<uint16_t> DataModel::getSensorIds() const {
  std::vector<uint16_t> sensorIds;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  for (const auto &stats : sensorStats_) {
    sensorIds.push_back(stats.id);
  }
  xSemaphoreGive(mutex_);

  return sensorIds;
}

ViewModel DataModel::getViewModel(uint16_t sensorId) const {
  xSemaphoreTake(mutex_, portMAX_DELAY);

  auto it = std::find_if(sensorStats_.begin(), sensorStats_.end(),
                         [=](const auto &st) { return st.id == sensorId; });
  assert(it != sensorStats_.end());

  const SensorStats &stats = *it;

  ViewModel vm{.sensorTypeName = stats.sensorTypeName,
               .sensorLocation = stats.sensorLocation,
               .battery = stats.battery,
               .temperature = stats.temperature,
               .minTemperature = stats.temperature,
               .maxTemperature = stats.temperature,
               .humidity = stats.humidity,
               .minHumidity = stats.humidity,

               .maxHumidity = stats.humidity};

  for (const auto &dp : stats.samples) {
    dpMinMax(vm, dp);
  }

  for (const auto &dp : stats.datapoints) {
    vm.datapoints.emplace_back(dp);
    dpMinMax(vm, dp);
  }

  xSemaphoreGive(mutex_);

  return vm;
}
