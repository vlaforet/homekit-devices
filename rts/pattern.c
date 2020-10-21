/*
 * pattern - demo project
 * 
 * Will create a hotspot and wait for the user to input their
 * network's credentials.
 */

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include "somfy_rts.h"


void user_init(void) {
    uart_set_baud(0, 115200);

    set_rts_pin(4, true);

    printf("YES READY\n");
    rts_down(0x1695F7, 35);
    printf("DONE\n");
}
