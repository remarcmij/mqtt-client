#include "Controller.h"
#include "display.h"
#include "pin_config.h"
#include "util.h"

static BaseType_t stack_hwm{};

Controller::Controller(void *param)
    : lowerButton_{OneButton(LOWER_BUTTON_PIN)}, upperButton_{OneButton(
                                                     UPPER_BUTTON_PIN)} {

  lowerButton_.attachLongPressStart(handleLongPressStart,
                                    (void *)LOWER_BUTTON_PIN);
  lowerButton_.attachLongPressStop(handleLongPressStop);

  upperButton_.attachLongPressStart(handleLongPressStart,
                                    (void *)UPPER_BUTTON_PIN);
  upperButton_.attachLongPressStop(handleLongPressStop);
};

void Controller::controllerTask() {
  for (;;) {
    lowerButton_.tick();
    upperButton_.tick();
    stack_hwm = reportStackUsage(stack_hwm);
    taskYIELD();
  }
}