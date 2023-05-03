#pragma once

namespace Display {
void handleLongPressStart(void *param);
void handleLongPressStop();
void changeBrightnessTask(void *param);
void displayTask(void *argp);
void init();
} // namespace Display