#include "display.h"
#include "pin_config.h"
#include <Arduino.h>

namespace Display {
TaskHandle_t brightnessTaskHandle;
uint32_t displayBrightness = 50;

void handleLongPressStart(void *param) {
  if (!brightnessTaskHandle) {
    BaseType_t rc =
        xTaskCreatePinnedToCore(changeBrightnessTask, "displayBrightness", 2400,
                                param, 1, &brightnessTaskHandle, 1);
    assert(rc == pdPASS);
  }
}

void handleLongPressStop() {
  if (brightnessTaskHandle) {
    vTaskDelete(brightnessTaskHandle);
    brightnessTaskHandle = nullptr;
  }
}

void changeBrightnessTask(void *param) {
  auto buttonPin = reinterpret_cast<unsigned>(param);

  for (;;) {
    if (buttonPin == UPPER_BUTTON_PIN && displayBrightness < 240) {
      displayBrightness++;
    } else if (buttonPin == LOWER_BUTTON_PIN && displayBrightness > 10) {
      displayBrightness--;
    }

    ledcWrite(0, displayBrightness);
    delay(20);
  }
}
} // namespace Display