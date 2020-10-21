/*
 * rh_dual_relay - Homekit switch for blinds (or two relays) (SHELLY 2.5)
 * 
 * Will create a hotspot and wait for the user to input their
 * network's credentials.
 *
 * A long press on the oulet's button will reset it to factory defaults.
 */
 //ci=14
 // 1 - 193-27-379 - 7C2A
 // 2 - 947-38-271 - 1EA6

#define FIRMWARE_REVISION "0.1.2"
#define MODEL "RH-WC1"

#define HK_PASSWORD "947-39-271
#define SETUPID "1EA6"
#define SERIAL_PREFIX "RQT2"

#define RELAY1_PIN 15
#define RELAY2_PIN 4
#define LED_PIN 0
#define BUTTON_RESET 2
#define BUTTON1_PIN 5 // UP
#define BUTTON2_PIN 13 // DOWN

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

#include "adv_button.h"
#include "led_status.h"
#include "ota-tftp.h"
#include "rboot-api.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define POSITION_OPEN 100
#define POSITION_CLOSED 0
#define POSITION_STATE_CLOSING 0
#define POSITION_STATE_OPENING 1
#define POSITION_STATE_STOPPED 2

// number of seconds the blinds take to move from fully open to fully closed position
#define SECONDS_FROM_CLOSED_TO_OPEN 22.5

static led_status_t led_status;
led_status_pattern_t identify = { 4, (int[]){ 100, 100, 100, 350 } };
led_status_pattern_t activity = { 2, (int[]){ 100, 100 } };
led_status_pattern_t waiting =  { 2, (int[]){ 300, 150 } };

TaskHandle_t updateStateTask;
homekit_characteristic_t current_position;
homekit_characteristic_t target_position;
homekit_characteristic_t position_state;
homekit_accessory_t *accessories[];

void target_position_changed();

void relay_write(int relay, bool on) {
    gpio_write(relay, on ? 1 : 0);
}

void relays_write(int state) {
    switch (state){
	case POSITION_STATE_CLOSING:
	    gpio_write(RELAY1_PIN, 0);
	    gpio_write(RELAY2_PIN, 1);
	    break;
	case POSITION_STATE_OPENING:
	    gpio_write(RELAY2_PIN, 0);
	    gpio_write(RELAY1_PIN, 1);
	    break;
	default:
	    gpio_write(RELAY2_PIN, 0);
	    gpio_write(RELAY1_PIN, 0);
    }
}

void reset_configuration_task() {
    led_status_set(led_status, &activity);

    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    sdk_system_restart();
    vTaskDelete(NULL);
}

void reset_configuration() {
    printf("Resetting configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}

void gpio_init() {
    gpio_enable(BUTTON1_PIN, GPIO_INPUT);
    gpio_enable(BUTTON2_PIN, GPIO_INPUT);

    gpio_enable(RELAY1_PIN, GPIO_OUTPUT);
    gpio_enable(RELAY2_PIN, GPIO_OUTPUT);
    relay_write(RELAY1_PIN, false);
    relay_write(RELAY2_PIN, false);
}

void down_callback(const uint8_t gpio, void *args, const uint8_t param) {
    target_position.value.int_value = POSITION_CLOSED;
    target_position_changed();
}

void up_callback(const uint8_t gpio, void *args, const uint8_t param) {
    target_position.value.int_value = POSITION_OPEN;
    target_position_changed();
}

void stop_callback(const uint8_t gpio, void *args, const uint8_t param) {
    target_position.value.int_value = current_position.value.int_value;
	target_position_changed();
}

void update_state() {
    while (true) {
        printf("update_state\n");
	    relays_write(position_state.value.int_value);
        uint8_t position = current_position.value.int_value;
        int8_t direction = position_state.value.int_value == POSITION_STATE_OPENING ? 1 : -1;
        int16_t newPosition = position + direction;

        printf("position %u, target %u\n", newPosition, target_position.value.int_value);

        current_position.value.int_value = newPosition;
        homekit_characteristic_notify(&current_position, current_position.value);

        if (newPosition == target_position.value.int_value) {
            printf("reached destination %u\n", newPosition);
            position_state.value.int_value = POSITION_STATE_STOPPED;

	        relays_write(position_state.value.int_value);
            homekit_characteristic_notify(&position_state, position_state.value);
            vTaskSuspend(updateStateTask);
        }

        vTaskDelay(pdMS_TO_TICKS(SECONDS_FROM_CLOSED_TO_OPEN * 10));
    }
}

void update_state_init() {
    xTaskCreate(update_state, "UpdateState", 256, NULL, tskIDLE_PRIORITY, &updateStateTask);
    vTaskSuspend(updateStateTask);
}

void window_covering_identify(homekit_value_t _value) {
    printf("Curtain identify\n");
}

void on_update_target_position(homekit_characteristic_t *ch, homekit_value_t value, void *context);

homekit_characteristic_t current_position = {
    HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_POSITION(POSITION_CLOSED)
};

homekit_characteristic_t target_position = {
    HOMEKIT_DECLARE_CHARACTERISTIC_TARGET_POSITION(POSITION_CLOSED, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update_target_position))
};

homekit_characteristic_t position_state = {
    HOMEKIT_DECLARE_CHARACTERISTIC_POSITION_STATE(POSITION_STATE_STOPPED)
};

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Blind");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, SERIAL_PREFIX);

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_window_covering, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "RockyHill"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, MODEL),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, FIRMWARE_REVISION),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, window_covering_identify),
            NULL
        }),
        HOMEKIT_SERVICE(WINDOW_COVERING, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Window blind"),
            &current_position,

            &target_position,
            &position_state,
            NULL
        }),
        NULL
    }),
    NULL
};

void on_update_target_position(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    target_position_changed();
}

void target_position_changed(){
    printf("Update target position to: %u\n", target_position.value.int_value);

    if (target_position.value.int_value == current_position.value.int_value) {
        printf("Current position equal to target. Stopping.\n");
        position_state.value.int_value = POSITION_STATE_STOPPED;
	    relays_write(position_state.value.int_value);
        homekit_characteristic_notify(&position_state, position_state.value);
        vTaskSuspend(updateStateTask);
    } else {
        position_state.value.int_value = target_position.value.int_value > current_position.value.int_value
            ? POSITION_STATE_OPENING
            : POSITION_STATE_CLOSING;

        homekit_characteristic_notify(&position_state, position_state.value);
        vTaskResume(updateStateTask);
    }
}

homekit_server_config_t config = {
    .accessories = accessories,
    .password = HK_PASSWORD,
    .setupId = SETUPID
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void create_accessory_name() {
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
    create_accessory_name();
    wifi_config_init("RH", NULL, on_wifi_ready);
    gpio_init();

    update_state_init();

    adv_button_set_evaluate_delay(10);

    adv_button_create(BUTTON1_PIN, false, false);
    adv_button_register_callback_fn(BUTTON1_PIN, stop_callback, INVSINGLEPRESS_TYPE, NULL, 0);
    adv_button_register_callback_fn(BUTTON1_PIN, up_callback, SINGLEPRESS_TYPE, NULL, 0);

    adv_button_create(BUTTON2_PIN, false, false);
    adv_button_register_callback_fn(BUTTON2_PIN, stop_callback, INVSINGLEPRESS_TYPE, NULL, 0);
    adv_button_register_callback_fn(BUTTON2_PIN, down_callback, SINGLEPRESS_TYPE, NULL, 0);
}