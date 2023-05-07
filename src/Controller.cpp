#include "Controller.h"
#include "pin_config.h"

Controller::Controller()
    : io14_button_{OneButton(IO14_BUTTON_PIN)}, boot_button_{OneButton(
                                                    BOOT_BUTTON_PIN)} {};

void Controller::setHandlers(const button_handlers_t &handlers) {
  io14_button_.attachClick(handlers.io14_handleClick);
  io14_button_.attachDoubleClick(handlers.io14_handleDoubleClick);
  io14_button_.attachLongPressStart(handlers.io14_handleLongPressStart);
  io14_button_.attachLongPressStop(handlers.io14_handleLongPressStop);
  boot_button_.attachClick(handlers.boot_handleClick);
  boot_button_.attachDoubleClick(handlers.boot_handleDoubleClick);
  boot_button_.attachLongPressStart(handlers.boot_handleLongPressStart);
  boot_button_.attachLongPressStop(handlers.boot_handleLongPressStop);
}

void Controller::controllerTask(void *param) {
  for (;;) {
    io14_button_.tick();
    boot_button_.tick();
    taskYIELD();
  }
}