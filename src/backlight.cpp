#include "backlight.h"
#include "pin_config.h"
#include <Arduino.h>

namespace Backlight {
TaskHandle_t taskHandle;
uint32_t brightness = 50;

void init() {
  // Set initial TFT brightness
  ledcSetup(0, 10000, 8);
  ledcAttachPin(38, 0);
  ledcWrite(0, brightness);
}

void changeBrightnessTask(void *param) {
  auto delta = reinterpret_cast<int>(param);

  for (;;) {
    brightness += delta;
    ledcWrite(0, brightness);
    delay(100);
  }
}

void startIncreaseBrightness() {
  if (!taskHandle) {
    BaseType_t rc = xTaskCreatePinnedToCore(changeBrightnessTask, "backlight",
                                            2400, (void *)1, 1, &taskHandle, 1);
    assert(rc == pdPASS);
  }
}

void stopIncreaseBrightness() {
  if (taskHandle) {
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
}

void startDecreaseBrightness() {
  if (!taskHandle) {
    BaseType_t rc = xTaskCreatePinnedToCore(
        changeBrightnessTask, "backlight", 2400, (void *)-1, 1, &taskHandle, 1);
    assert(rc == pdPASS);
  }
}
void stopDecreaseBrightness() {
  if (taskHandle) {
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
}
} // namespace Backlight