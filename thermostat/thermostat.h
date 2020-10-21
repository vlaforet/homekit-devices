#pragma once

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <dht/dht.h>

void compute_state();
void update(homekit_characteristic_t *ch, homekit_value_t value, void *context);
void relay(bool on);
void sensor_task();