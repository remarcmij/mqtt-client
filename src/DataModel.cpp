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
  auto battery = doc["battery"].as<uint32_t>();

  // Extract sensor location = last path element of topic
  auto strTopic = String(topic);
  auto pos = strTopic.lastIndexOf('/');
  assert(pos != -1);
  auto strLocation = strTopic.substring(pos + 1);

  // Find the corresponding sensor pstats for the incoming MQTT message
  SensorStats *pstats{nullptr};

  auto it = std::find_if(sensorStats_.begin(), sensorStats_.end(),
                         [&](const auto &st) {
                           return st.sensorLocation == strLocation &&
                                  st.sensorTypeName == strSensorTypeName;
                         });

  if (it == sensorStats_.end()) {
    // Not found: create new entry
    pstats =
        &sensorStats_.emplace_back(++nextId_, strSensorTypeName, strLocation);
  } else {
    pstats = &(*it);
  }

  xSemaphoreTake(mutex_, portMAX_DELAY);

  pstats->temperature = temperature;
  pstats->humidity = humidity;
  pstats->battery = battery;

  pstats->samples.emplace_back(temperature, humidity);
  // Include a previous batch of samples to compute
  // some sort of running average.
  if (pstats->samples.size() > SAMPLES_PER_DATAPOINT * 2) {
    pstats->samples.pop_front();
  }
  pstats->sampleCount++;

  if (pstats->sampleCount == SAMPLES_PER_DATAPOINT) {
    // Compute average temperature and humidity for
    // the last 6 samples (= last 6 minutes)
    float temperatureTotal{};
    float humidityTotal{};
    for (const auto &sample : pstats->samples) {
      temperatureTotal += sample.temperature;
      humidityTotal += sample.humidity;
    }
    float temperatureAvg = temperatureTotal / pstats->samples.size();
    float humidityAvg = humidityTotal / pstats->samples.size();

    log_d("average temperature: %.2f", temperatureAvg);
    log_d("average sample humidity: %.2f", humidityAvg);

    pstats->datapoints.emplace_back(temperatureAvg, humidityAvg);
    if (pstats->datapoints.size() > MAX_DATAPOINTS) {
      pstats->datapoints.pop_front();
    }

    pstats->sampleCount = 0;
  }

  xSemaphoreGive(mutex_);

  view_->update(pstats->id);

  log_d("location: %s, samples: %u, datapoints: %u, sensors: %u",
        pstats->sensorLocation, pstats->sampleCount, pstats->datapoints.size(),
        sensorStats_.size());
}

std::vector<uint16_t> DataModel::getSensorIds() {
  std::vector<uint16_t> sensorIds;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  for (const auto &stats : sensorStats_) {
    sensorIds.push_back(stats.id);
  }
  xSemaphoreGive(mutex_);

  return sensorIds;
}

ViewModel DataModel::getViewModel(uint16_t sensorId) {
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
