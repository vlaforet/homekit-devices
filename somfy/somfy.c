/*
 * somfy - Blaster for Somfy 433.92MHz rollershutters
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
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <sysparam.h>
#include <etstimer.h>
#include <esplibs/libmain.h>

#include "led_status.h"
#include "ota-tftp.h"
#include "rboot-api.h"
#include "button.h"

#include "somfy_rts.h"

#define MODEL_NAME "RH-SO1"
#define SERIAL_PREFIX "DF53"
#define FIRMWARE_REVISION_VALUE "0.0.1"
#define LED_PIN 13
#define BUTTON_PIN 5
#define RTS_PIN 4
#define BASE_ADDRESS 0x1695F7


#define POSITION_CLOSED 0
#define POSITION_OPEN 100
#define POSITION_DEFAULT 50
#define POSITION_STATE_CLOSING 0
#define POSITION_STATE_OPENING 1
#define POSITION_STATE_STOPPED 2

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)


typedef struct {
    int id;
    homekit_characteristic_t* target_position;
    homekit_characteristic_t* position_state;
    ETSTimer timer;
} remote_t;

static led_status_t led_status;
led_status_pattern_t identify = { 4, (int[]){ 100, 100, 100, 350 } };
led_status_pattern_t activity = { 2, (int[]){ 100, 100 } };
led_status_pattern_t waiting  = { 2, (int[]){ 300, 150 } };

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Rollershutter");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, SERIAL_PREFIX);
homekit_accessory_t *accessories[2];
homekit_server_config_t config = {
    .accessories = accessories,
    .password = STRINGIZE_VALUE_OF(HK_PASSWORD),
    .setupId = STRINGIZE_VALUE_OF(SETUPID)
};
bool prog_mode = false;

void reset_configuration_task() {
    led_status_set(led_status, &activity);

    sysparam_get_int8("remote_count", 0);
    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    sdk_system_restart();
    vTaskDelete(NULL);
}

void reset_configuration() {xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL); }
void accessory_identify(homekit_value_t _value) { led_status_set_repeat(led_status, &identify, 3); }

void back_to_default(remote_t *remote) {
    remote->target_position->value.int_value = 50;
    remote->position_state->value.int_value = POSITION_STATE_STOPPED;
    homekit_characteristic_notify(remote->target_position, remote->target_position->value);
    homekit_characteristic_notify(remote->position_state, remote->position_state->value);
    printf("Reset\n");
}

void get_rolling_code(int id, int32_t *rolling_code, bool increment) {
    int8_t remote_count = 0;
    sysparam_get_int8("remote_count", &remote_count);

    if (id >= remote_count) return;

    int len = snprintf(NULL, 0, "rolcode_%i", id);
    char *rolcode_name = malloc(len+1);
    snprintf(rolcode_name, len+1, "rolcode_%i", id);

    sysparam_get_int32(rolcode_name, rolling_code);

    if (increment) {
        sysparam_set_int32(rolcode_name, (int32_t)(*rolling_code+1));
    }

    printf("rol: %i\n", *rolling_code);
}

void target_position_callback(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    remote_t *remote = (remote_t *) context;
    sdk_os_timer_disarm(&remote->timer);

    if (value.int_value == POSITION_DEFAULT) return;

    int32_t rolling_code = 0;

    if (remote->position_state->value.int_value != POSITION_STATE_STOPPED) {
        if (remote->target_position->value.int_value == POSITION_OPEN
            || remote->target_position->value.int_value == POSITION_CLOSED) {
            return;
        }

        printf("Stopping\n");
        get_rolling_code(remote->id, &rolling_code, true);
        rts_stop(BASE_ADDRESS + remote->id, rolling_code);
        back_to_default(remote);
        return;
    }

    get_rolling_code(remote->id, &rolling_code, true);
    if (value.int_value > POSITION_DEFAULT) {
        printf("Going up\n");
        remote->position_state->value.int_value = POSITION_STATE_OPENING;
        remote->target_position->value.int_value = POSITION_OPEN;
        rts_up(BASE_ADDRESS + remote->id, rolling_code);

    } else if (value.int_value < POSITION_DEFAULT) {
        printf("Going down\n");
        remote->position_state->value.int_value = POSITION_STATE_CLOSING;
        remote->target_position->value.int_value = POSITION_CLOSED;
        rts_down(BASE_ADDRESS + remote->id, rolling_code);
    }

    homekit_characteristic_notify(remote->target_position, remote->target_position->value);
    homekit_characteristic_notify(remote->position_state, remote->position_state->value);
    sdk_os_timer_arm(&remote->timer, 10000, 0);
}

void init_services() {
    int8_t remote_count = 0;
    sysparam_get_int8("remote_count", &remote_count);

    homekit_service_t* services[remote_count + 2];
    homekit_service_t** s = services;
    * (s++) = NEW_HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
        &name,
        NEW_HOMEKIT_CHARACTERISTIC(MANUFACTURER, "RockyHill"),
        &serial,
        NEW_HOMEKIT_CHARACTERISTIC(MODEL, MODEL_NAME),
        NEW_HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, FIRMWARE_REVISION_VALUE),
        NEW_HOMEKIT_CHARACTERISTIC(IDENTIFY, accessory_identify),
        NULL
    });

    int32_t rolling_code = 0;
    for (int i = 0; i < remote_count; i++) {
        get_rolling_code(i, &rolling_code, false);
        printf("Remote id=%i \nAddress: %i\nRolling Code: %i\n\n", i, BASE_ADDRESS + i, rolling_code);
    
        int len = snprintf(NULL, 0, "Shade %i", i);
        char *service_name = malloc(len+1);
        snprintf(service_name, len+1, "Shade %i", i);

        remote_t *remote = malloc(sizeof(remote_t));
        remote->id = i;
        remote->target_position = NEW_HOMEKIT_CHARACTERISTIC(TARGET_POSITION, POSITION_DEFAULT,
                .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(target_position_callback, 
                    .context=(void*)remote)
                );
        remote->position_state = NEW_HOMEKIT_CHARACTERISTIC(POSITION_STATE, POSITION_STATE_STOPPED);

        sdk_os_timer_setfn(&(remote->timer), (void(*)(void*))back_to_default, remote);
        back_to_default(remote);

        *(s++) = NEW_HOMEKIT_SERVICE(WINDOW_COVERING, .characteristics=(homekit_characteristic_t*[]) {
            NEW_HOMEKIT_CHARACTERISTIC(NAME, service_name),
            NEW_HOMEKIT_CHARACTERISTIC(CURRENT_POSITION, POSITION_DEFAULT),
            remote->target_position,
            remote->position_state,
            NULL
        });
    }

    *(s++) = NULL;
    accessories[0] = NEW_HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_window_covering, .services=services);
    accessories[1] = NULL;
}

void prog() {
    if (prog_mode) return;
    prog_mode = true;
    printf("Prog mode\n");
    int8_t remote_count = 0;
    sysparam_get_int8("remote_count", &remote_count);
    sysparam_set_int8("remote_count", remote_count+1);

    int len = snprintf(NULL, 0, "rolcode_%i", remote_count);
    char *rolcode_name = malloc(len+1);
    snprintf(rolcode_name, len+1, "rolcode_%i", remote_count);
    sysparam_set_int32(rolcode_name, 14);

    printf("-- Setup remote id=%i --\nAddress: %i \nRolling Code: %i\n\n",
        remote_count, BASE_ADDRESS + remote_count, 14);

    for (int i = 0; i < 3; i++) {
        rts_prog(BASE_ADDRESS + remote_count, 13);
    }

    prog_mode = false;
    vTaskDelete(NULL);
}

void button_callback(button_event_t event, void* context) {
    switch (event) {
        case button_event_long_press:
            reset_configuration();
        case button_event_double_press:
            led_status_set_repeat(led_status, &activity, 5);
            xTaskCreate(prog, "Prog", 256, NULL, 1, NULL);
        default:
            led_status_set_repeat(led_status, &activity, 1);
    }
}

void on_wifi_ready() {
    init_services();

    homekit_server_init(&config);
    led_status_set_repeat(led_status, &activity, 5);
}

void serial_construct() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "RH-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "RH-%02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);
    
    int serial_len = snprintf(NULL, 0, "%s%02X%02X%02X", SERIAL_PREFIX,
                            macaddr[3], macaddr[4], macaddr[5]);
    char *serial_value = malloc(serial_len+1);
    snprintf(serial_value, serial_len+1, "%s%02X%02X%02X", SERIAL_PREFIX,
             macaddr[3], macaddr[4], macaddr[5]);

    name.value = HOMEKIT_STRING(name_value);
    serial.value = HOMEKIT_STRING(serial_value);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    led_status = led_status_init(2);
    led_status_set(led_status, &waiting);

    ota_tftp_init_server(TFTP_PORT);
    serial_construct();
    wifi_config_init("RH", NULL, on_wifi_ready);

    set_rts_pin(RTS_PIN, true);
    
    button_config_t bconf = BUTTON_CONFIG(
        button_active_low,
        .long_press_time = 15000,
        .max_repeat_presses = 2,
    );

    if (button_create(BUTTON_PIN, bconf, button_callback, NULL)) {
        printf("Failed to initialize button\n");
    }
}
