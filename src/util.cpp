#include "util.h"

UBaseType_t reportStackUsage(UBaseType_t stack_hwm) {
  auto current = uxTaskGetStackHighWaterMark(nullptr);
  if (stack_hwm == 0 || current < stack_hwm) {
    stack_hwm = current;
    Serial.print("Task '");
    Serial.print(pcTaskGetName(nullptr));
    Serial.print("' has stack hwm ");
    Serial.println(stack_hwm);
  }
  return stack_hwm;
}