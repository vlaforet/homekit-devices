#include "rockyhill_common.h"
#include "adv_button.h"
#include "led_status.h"
#include "ota-tftp.h"
#include "rboot-api.h"
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <espressif/esp_wifi.h>

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

led_status_t led_status;
led_status_pattern_t rhc_status_identify = { 4, (int[]){ 100, 100, 100, 350 } };
led_status_pattern_t rhc_status_activity = { 2, (int[]){ 100, 100 } };
led_status_pattern_t rhc_status_waiting =  { 2, (int[]){ 300, 150 } };

void reset_configuration_task() {
    led_status_set(led_status, &rhc_status_activity);

    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    sdk_system_restart();
    vTaskDelete(NULL);
}

void reset_callback(const uint8_t gpio, void *args, const uint8_t param) {
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}

void rh_common_reset_button(const uint8_t gpio, const bool pullup_resistor, const bool inverted) {
    adv_button_set_evaluate_delay(10);

    adv_button_create(gpio, pullup_resistor, inverted);
    adv_button_register_callback_fn(gpio, reset_callback, VERYLONGPRESS_TYPE, NULL, 0);
}

homekit_server_config_t config = {
    .password = STRINGIZE_VALUE_OF(HK_PASSWORD),
    .setupId = STRINGIZE_VALUE_OF(SETUPID)
};

void on_wifi_ready() {
    homekit_server_init(&config);

    led_status_set_repeat(led_status, &rhc_status_activity, 5);
}

void rh_common_name_serial_construct(homekit_characteristic_t name, homekit_characteristic_t serial, char *prefix) {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "RH-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "RH-%02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);
    
    int serial_len = snprintf(NULL, 0, "%s%02X%02X%02X", prefix,
                            macaddr[3], macaddr[4], macaddr[5]);
    char *serial_value = malloc(serial_len+1);
    snprintf(serial_value, serial_len+1, "%s%02X%02X%02X", prefix,
             macaddr[3], macaddr[4], macaddr[5]);

    name.value = HOMEKIT_STRING(name_value);
    serial.value = HOMEKIT_STRING(serial_value);
}

void rh_common_identify(homekit_value_t _value) {
    led_status_set_repeat(led_status, &rhc_status_identify, 3);
}

void rh_common_init(homekit_accessory_t *accesso[]) {
    config.accessories = accesso;
    led_status = led_status_init(2);
    led_status_set(led_status, &rhc_status_waiting);

    ota_tftp_init_server(TFTP_PORT);
    wifi_config_init("RH", NULL, on_wifi_ready);
}