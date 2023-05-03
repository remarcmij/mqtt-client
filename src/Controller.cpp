#include "Controller.h"
#include "display.h"
#include "pin_config.h"

Controller::Controller()
    : lowerButton_{OneButton(LOWER_BUTTON_PIN)}, upperButton_{OneButton(
                                                     UPPER_BUTTON_PIN)} {

  lowerButton_.attachLongPressStart(Display::handleLongPressStart,
                                    (void *)LOWER_BUTTON_PIN);
  lowerButton_.attachLongPressStop(Display::handleLongPressStop);

  upperButton_.attachLongPressStart(Display::handleLongPressStart,
                                    (void *)UPPER_BUTTON_PIN);
  upperButton_.attachLongPressStop(Display::handleLongPressStop);
};

void Controller::controllerTask(void *param) {
  for (;;) {
    lowerButton_.tick();
    upperButton_.tick();
    taskYIELD();
  }
}