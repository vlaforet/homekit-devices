#pragma once

#include "led_status.h"
#include <esp8266.h>
#include <homekit/characteristics.h>

void rh_common_reset_button(const uint8_t gpio, const bool pullup_resistor, const bool inverted);
void rh_common_init();
void rh_common_name_serial_construct(homekit_characteristic_t name, homekit_characteristic_t serial, char *prefix);
void rh_common_identify(homekit_value_t _value);
