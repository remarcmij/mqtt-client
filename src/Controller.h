#pragma once
#include <OneButton.h>

struct button_handlers_t {
  callbackFunction io14_handleClick;
  callbackFunction io14_handleDoubleClick;
  callbackFunction io14_handleLongPressStart;
  callbackFunction io14_handleLongPressStop;
  callbackFunction boot_handleClick;
  callbackFunction boot_handleDoubleClick;
  callbackFunction boot_handleLongPressStart;
  callbackFunction boot_handleLongPressStop;
};

class Controller {
public:
  Controller();
  void setHandlers(const button_handlers_t &handlers);
  void controllerTask(void *param);

private:
  OneButton io14_button_;
  OneButton boot_button_;
};