/*
 * rh_relay - Homekit switch for any standard esp8266 based outlet
 * 
 * Will create a hotspot and wait for the user to input their
 * network's credentials.
 *
 * A long press on the oulet's button will reset it to factory defaults.
 */

// tools/gen_qrcode 8 HK_PASSWORD SETUPID qrcode.png
/* Model this firmware will be used on
 * e.g.
 *  - 0 for Sonoff outlet
 *  - 1 for Blitzwolf outlet
 *  - 2 for Shelly 1
 */

#define FIRMWARE_REVISION "0.1.6"

#define SERIAL_PREFIX "DQS3"

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

#if MODELID == 0
    #define MODEL "RH-S1-S"

    #define RELAY_PIN 12
    #define LED_PIN 13
    #define BUTTON_PIN 0

#elif MODELID == 1
    #define MODEL "RH-S1-B"

    #define RELAY_PIN 15
    #define LED_PIN 0
    #define BUTTON_PIN 13

#elif MODELID == 2
    #define MODEL "RH-S1-SH"

    #define RELAY_PIN 4
    #define LED_PIN -1
    #define TOGGLE_PIN 5

#endif

#ifdef BUTTON_PIN
    #include "button.h"
#elif defined TOGGLE_PIN
    #include "toggle.h"
#endif
#include "led_status.h"
#include "ota-tftp.h"
#include "rboot-api.h"

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

static led_status_t led_status;
led_status_pattern_t identify = { 4, (int[]){ 100, 100, 100, 350 } };
led_status_pattern_t activity = { 2, (int[]){ 100, 100 } };
led_status_pattern_t waiting =  { 2, (int[]){ 300, 150 } };

homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(
    ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback)
);
homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Relay");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, SERIAL_PREFIX);

void relay_write(bool on) {
    gpio_write(RELAY_PIN, on ? 1 : 0);
    led_status_set_repeat(led_status, &activity, 2);
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
    gpio_enable(RELAY_PIN, GPIO_OUTPUT);
    relay_write(false);
#ifdef BUTTON_PIN
    gpio_enable(BUTTON_PIN, GPIO_INPUT);
#elif defined TOGGLE_PIN
    gpio_enable(TOGGLE_PIN, GPIO_INPUT);
#endif
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write(switch_on.value.bool_value);
}

void toggle() {
    printf("Toggling relay\n");
    switch_on.value.bool_value = !switch_on.value.bool_value;
    relay_write(switch_on.value.bool_value);
    homekit_characteristic_notify(&switch_on, switch_on.value);
}

#ifdef BUTTON_PIN
void button_callback(button_event_t event, void* context) {
    switch (event) {
        case button_event_long_press:
            reset_configuration();
        default:
            toggle();
    }
}
#elif defined TOGGLE_PIN
void toggle_callback(bool high, void *context) {
    toggle();
}
#endif

void switch_identify(homekit_value_t _value) {
    led_status_set_repeat(led_status, &identify, 3);
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "RockyHill"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, MODEL),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, FIRMWARE_REVISION),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, switch_identify),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Relay"),
            &switch_on,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = STRINGIZE_VALUE_OF(HK_PASSWORD),
    .setupId = STRINGIZE_VALUE_OF(SETUPID)
};

void on_wifi_ready() {
    homekit_server_init(&config);
    led_status_set_repeat(led_status, &activity, 5);
}

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "Relay-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Relay-%02X%02X%02X",
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
    led_status = led_status_init(LED_PIN);
    led_status_set(led_status, &waiting);

    ota_tftp_init_server(TFTP_PORT);
    create_accessory_name();
    wifi_config_init("RH", NULL, on_wifi_ready);
    gpio_init();

#ifdef BUTTON_PIN
    button_config_t config = BUTTON_CONFIG (
        button_active_low,
        .long_press_time = 15000,
    );

    if (button_create(BUTTON_PIN, config, button_callback, NULL)) {
        printf("Failed to initialize button\n");
    }
#elif defined TOGGLE_PIN
    if (toggle_create(TOGGLE_PIN, toggle_callback, NULL)) {
        printf("Failed to initialize toggle\n");
    }
#endif
}
