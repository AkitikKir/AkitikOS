#ifndef THEME_H
#define THEME_H

#include <Arduino.h>

struct Theme {
  uint16_t bg;
  uint16_t bg2;
  uint16_t fg;
  uint16_t accent;
  uint16_t panel;
  uint16_t prompt;
  uint16_t shadow;
  uint16_t dim;
};

#endif
