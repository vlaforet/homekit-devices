/*
 * thermostat - A simple Homekit thermostat
 * 
 * Will create a hotspot and wait for the user to input their
 * network's credentials.
 */
#include <thermostat.h>

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
#include "rockyhill_common.h"

#define MODEL "RH-TH1"
#define SERIAL_PREFIX "4GD6"
#define FIRMWARE_REVISION "0.0.2"

#define SENSOR_PIN 4
#define HEAT_RELAY_PIN 5
#define RESET_BUTTON_PIN 14

#define TEMP_APPROX 0.5

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Thermostat");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, SERIAL_PREFIX);
homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t current_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);
homekit_characteristic_t current_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATING_COOLING_STATE, 0);
homekit_characteristic_t target_state = HOMEKIT_CHARACTERISTIC_(
    TARGET_HEATING_COOLING_STATE,
    0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(update), .max_value=(float[]) {1}
);
homekit_characteristic_t target_temperature = HOMEKIT_CHARACTERISTIC_(
    TARGET_TEMPERATURE, 20, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(update)
);

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "RockyHill"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, MODEL),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, FIRMWARE_REVISION),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, rh_common_identify),
            NULL
        }),
        HOMEKIT_SERVICE(THERMOSTAT, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Thermostat"),
            &current_temperature,
            &target_temperature,
            &current_state,
            &target_state,
            HOMEKIT_CHARACTERISTIC(TEMPERATURE_DISPLAY_UNITS, 0),
            &current_humidity,
            NULL
        }),
        NULL
    }),
    NULL
};

void relay(bool on) {gpio_write(HEAT_RELAY_PIN, on); printf("{--heating//%i--}\n", on ? 1 : 0);}

void update(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    compute_state();
}

void compute_state() {
    printf("{--targtemp//%.1f--}\n", target_temperature.value.float_value);

    if (current_state.value.int_value == HOMEKIT_CURRENT_HEATING_COOLING_STATE_HEAT) {
        if (target_state.value.int_value != HOMEKIT_TARGET_HEATING_COOLING_STATE_HEAT ||
            current_temperature.value.float_value > target_temperature.value.float_value + TEMP_APPROX) {
                // STOP (temp higher than requested + approx)
                current_state.value = HOMEKIT_UINT8(0);
                homekit_characteristic_notify(&current_state, current_state.value);
                relay(false);
        }
    } else {
        if (target_state.value.int_value == HOMEKIT_TARGET_HEATING_COOLING_STATE_HEAT &&
            current_temperature.value.float_value < target_temperature.value.float_value - TEMP_APPROX) {
                // HEAT (temp lower than requested - approx)
                current_state.value = HOMEKIT_UINT8(1);
                homekit_characteristic_notify(&current_state, current_state.value);
                relay(true);
        }
    }
}

void sensor_task() {
    float humidity_value, temperature_value;
    while (1) {
        bool success = dht_read_float_data(
            DHT_TYPE_DHT22, SENSOR_PIN,
            &humidity_value, &temperature_value
        );
        if (success) {
            current_temperature.value.float_value = temperature_value;
            current_humidity.value.float_value = humidity_value;

            homekit_characteristic_notify(&current_temperature, HOMEKIT_FLOAT(temperature_value));
            homekit_characteristic_notify(&current_humidity, HOMEKIT_FLOAT(humidity_value));

            printf("{--currtemp//%.1f--}\n", temperature_value);
            printf("{--humidity//%.1f--}\n", humidity_value);

            compute_state();
        } else {
            printf("Couldnt read data from sensor\n");
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void user_init(void) {
    uart_set_baud(0, 115200);

    rh_common_init(accessories);
    rh_common_reset_button(RESET_BUTTON_PIN, true, false);
    rh_common_name_serial_construct(name, serial, SERIAL_PREFIX);

    gpio_enable(HEAT_RELAY_PIN, GPIO_OUTPUT);
    gpio_set_pullup(SENSOR_PIN, false, false);
    relay(false);
    xTaskCreate(sensor_task, "Sensor", 256, NULL, 2, NULL);
}
