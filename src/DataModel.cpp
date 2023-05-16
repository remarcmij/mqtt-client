#include "DataModel.h"
#include "ArduinoJson.h"
#include <algorithm>

void DataModel::setView(IView *view) { view_ = view; }

void DataModel::nextSensor() {
  if (!sensorStats_.empty()) {
    sensorIndex_ = (sensorIndex_ + 1) % sensorStats_.size();
    view_->update();
  }
}

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
  int foundIndex = -1;

  int index{0};
  for (auto &st : sensorStats_) {
    if (st.sensorLocation == strLocation &&
        st.sensorTypeName == strSensorTypeName) {
      pstats = &st;
      foundIndex = index;
      break;
    }
    index++;
  }

  if (!pstats) {
    // Create a new entry if not found
    pstats = &sensorStats_.emplace_back(strSensorTypeName, strLocation);
    foundIndex = sensorStats_.size() - 1;
  }

  xSemaphoreTake(mutex_, portMAX_DELAY);

  pstats->temperature = temperature;
  pstats->humidity = humidity;
  pstats->battery = battery;

  pstats->samples.emplace_back(temperature, humidity);

  if (pstats->samples.size() == SAMPLES_PER_DATAPOINT) {
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

    pstats->samples.clear();
  }

  xSemaphoreGive(mutex_);

  if (foundIndex == sensorIndex_) {
    view_->update();
  }

  log_d("samples: %u, datapoints: %u, sensors: %u", pstats->samples.size(),
        pstats->datapoints.size(), sensorStats_.size());
}

ViewModel DataModel::getViewModel() {
  xSemaphoreTake(mutex_, portMAX_DELAY);

  const SensorStats &stats = sensorStats_[sensorIndex_];
  ViewModel vm{};

  vm.sensorTypeName = stats.sensorTypeName;
  vm.sensorLocation = stats.sensorLocation;
  vm.battery = stats.battery;

  vm.temperature = vm.minTemperature = vm.maxTemperature = stats.temperature;
  vm.humidity = vm.minHumidity = vm.maxHumidity = stats.humidity;

  for (const auto &sample : stats.samples) {
    vm.minTemperature = std::min(vm.minTemperature, sample.temperature);
    vm.maxTemperature = std::max(vm.maxTemperature, sample.temperature);
    vm.minHumidity = std::min(vm.minHumidity, sample.humidity);
    vm.maxHumidity = std::max(vm.maxHumidity, sample.humidity);
  }

  for (const auto &dp : stats.datapoints) {
    vm.datapoints.emplace_back(dp);
    vm.minTemperature = std::min(vm.minTemperature, dp.temperature);
    vm.maxTemperature = std::max(vm.maxTemperature, dp.temperature);
    vm.minHumidity = std::min(vm.minHumidity, dp.humidity);
    vm.maxHumidity = std::max(vm.maxHumidity, dp.humidity);
  }

  xSemaphoreGive(mutex_);

  return vm;
}
