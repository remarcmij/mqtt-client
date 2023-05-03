#pragma once
#include <OneButton.h>

class Controller {
public:
  Controller();
  void controllerTask(void *param);

private:
  OneButton lowerButton_;
  OneButton upperButton_;
};