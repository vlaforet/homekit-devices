/*
 * temperature sensor reporting to homekit
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
#include <dht/dht.h>

#include "led_status.h"
#include "ota-tftp.h"
#include "rboot-api.h"

#define MODEL "RH-TS1"
#define SERIAL_PREFIX "HYE"
#define FIRMWARE_REVISION "0.0.1"
#define LED_PIN 13
#define SENSOR_PIN 4

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

static led_status_t led_status;
led_status_pattern_t identify = { 4, (int[]){ 100, 100, 100, 350 } };
led_status_pattern_t activity = { 2, (int[]){ 100, 100 } };
led_status_pattern_t waiting =  { 2, (int[]){ 300, 150 } };

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Temperature");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, SERIAL_PREFIX);
homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

void sensor_task() {
    gpio_set_pullup(SENSOR_PIN, false, false);

    float humidity_value, temperature_value;
    while (1) {
        bool success = dht_read_float_data(
            DHT_TYPE_DHT22, SENSOR_PIN,
            &humidity_value, &temperature_value
        );
        if (success) {
            temperature.value.float_value = temperature_value;
            humidity.value.float_value = humidity_value;

            homekit_characteristic_notify(&temperature, HOMEKIT_FLOAT(temperature_value));
            homekit_characteristic_notify(&humidity, HOMEKIT_FLOAT(humidity_value));
        } else {
            printf("Couldnt read data from sensor\n");
        }

        vTaskDelay(3000 / portTICK_PERIOD_MS);
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
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}

void identify_func(homekit_value_t _value) {
    led_status_set_repeat(led_status, &identify, 3);
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "RockyHill"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, MODEL),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, FIRMWARE_REVISION),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify_func),
            NULL
        }),
        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Temperature"),
            &temperature,
            NULL
        }),
        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Humidity"),
            &humidity,
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

    xTaskCreate(sensor_task, "Sensor", 256, NULL, 2, NULL);
}
