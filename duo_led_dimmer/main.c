/*
 * duo_led_dimmer - Homekit dimmer for two 12v led lights with wired switches
 * 
 * Will create a hotspot and wait for the user to input their
 * network's credentials.
 *
 * A long press on the oulet's button will reset it to factory defaults.
 */

// tools/gen_qrcode 5 HK_PASSWORD SETUPID qrcode.png

#define HK_PASSWORD "733-01-973"
#define SETUPID "9BF3"
#define SERIAL_PREFIX "DLP1"

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

#include <button.h>
#include <multipwm.h>
#include <wifi_config.h>

#define MODEL "RH-DLD1"

#define OUTPUT1_PIN 5
#define OUTPUT2_PIN 4
#define INPUT1_PIN 14
#define INPUT2_PIN 12
#define INTERNAL_LED_PIN 2

pwm_info_t pwm_info;
uint8_t levels[] = {100, 100};
bool states[] = {false, false};

void check_notify();

void internal_led_write(bool on) {
    gpio_write(INTERNAL_LED_PIN, on ? 0 : 1);
}

void setOutputLevel(uint8_t output, uint8_t level) {
    levels[output] = level;
    multipwm_stop(&pwm_info);
    multipwm_set_duty(&pwm_info, output, UINT16_MAX*level/100);
    multipwm_start(&pwm_info);

    check_notify();
}

void setOutputState(uint8_t output, bool state) {
    states[output] = state;
    multipwm_stop(&pwm_info);

    if (state) {
        if (levels[output] <= 0 || levels[output] > 100) {
            levels[output] = 100;
        }
        multipwm_set_duty(&pwm_info, output, UINT16_MAX*levels[output]/100);
    } else {
        multipwm_set_duty(&pwm_info, output, 0);
    }
    multipwm_start(&pwm_info);
    check_notify();
}

void identify_task(void *_args) {
    uint8_t prev_levels[] = {levels[0], levels[1]};
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            internal_led_write(true);
            setOutputLevel(0, 100);
            setOutputLevel(1, 100);
            vTaskDelay(100 / portTICK_PERIOD_MS);

            internal_led_write(false);
            setOutputLevel(0, 0);
            setOutputLevel(1, 0);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    internal_led_write(false);
    setOutputLevel(0, prev_levels[0]);
    setOutputLevel(1, prev_levels[1]);
    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    printf("Identify\n");
    xTaskCreate(identify_task, "Identify", 128, NULL, 2, NULL);
}

void output1_state_set(homekit_value_t value) {
    setOutputState(0, value.bool_value);
}

void output2_state_set(homekit_value_t value) {
    setOutputState(1, value.bool_value);
}

homekit_value_t output1_state_get() {return HOMEKIT_BOOL(states[0]);}
homekit_value_t output2_state_get() {return HOMEKIT_BOOL(states[1]);}

void output1_level_set(homekit_value_t value) {
    if (value.format != homekit_format_int) {
        printf("Invalid level\n");
        return;
    }
    setOutputLevel(0, value.int_value);
}

void output2_level_set(homekit_value_t value) {
    if (value.format != homekit_format_int) {
        printf("Invalid level\n");
        return;
    }
    setOutputLevel(1, value.int_value);
}

homekit_value_t output1_level_get() {return HOMEKIT_INT(levels[0]);}
homekit_value_t output2_level_get() {return HOMEKIT_INT(levels[1]);}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "LED");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, SERIAL_PREFIX);

homekit_characteristic_t output1_state = HOMEKIT_CHARACTERISTIC_(ON, false,
    .getter=output1_state_get, .setter=output1_state_set);
homekit_characteristic_t output1_level = HOMEKIT_CHARACTERISTIC_(BRIGHTNESS, 100,
    .getter=output1_level_get, .setter=output1_level_set);

homekit_characteristic_t output2_state = HOMEKIT_CHARACTERISTIC_(ON, false,
    .getter=output2_state_get, .setter=output2_state_set);
homekit_characteristic_t output2_level = HOMEKIT_CHARACTERISTIC_(BRIGHTNESS, 100,
    .getter=output2_level_get, .setter=output2_level_set);

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_lightbulb, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "RockyHill"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, MODEL),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1.5"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Light 1"),
            &output1_state,
            &output1_level,
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Light 2"),
            &output2_state,
            &output2_level,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = HK_PASSWORD,
    .setupId = SETUPID
};

void reset_configuration_task() {
    for (int j=0; j<5; j++) {
            internal_led_write(true);
            setOutputLevel(0, 100);
            setOutputLevel(1, 100);
            vTaskDelay(100 / portTICK_PERIOD_MS);

            internal_led_write(false);
            setOutputLevel(0, 0);
            setOutputLevel(1, 0);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    sdk_system_restart();
    vTaskDelete(NULL);
}

void check_notify() {
    if (levels[0] != output1_level.value.int_value) {
        output1_level.value.int_value = levels[0];
        homekit_characteristic_notify(&output1_level, output1_level.value);
    }

    if (states[0] != output1_state.value.bool_value) {
        output1_state.value.bool_value = states[0];
        homekit_characteristic_notify(&output1_state, output1_state.value);
    }

    if (levels[1] != output2_level.value.int_value) {
        output2_level.value.int_value = levels[1];
        homekit_characteristic_notify(&output2_level, output2_level.value);
    }

    if (states[1] != output2_state.value.bool_value) {
        output2_state.value.bool_value = states[1];
        homekit_characteristic_notify(&output2_state, output2_state.value);
    }
}

void handle_double_press(uint8_t output) {
    uint8_t lev = levels[output];
    if (!states[output] || (lev!=25 && lev!=50 && lev!=75 && lev!=100)) {
        lev = 50;
    } else {
        lev += 25;
        if (lev > 100) {
            lev = 25;
        }
    }
    setOutputLevel(output, lev);
    setOutputState(output, true);
}

void button1_callback(button_event_t event, void* context) {
    switch (event) {
        case button_event_single_press:
            setOutputState(0, !states[0]);
            break;
        case button_event_long_press:
            xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
            break;
        case button_event_double_press:
            handle_double_press(0);
            break;
        default:
            break;
    }
}

void button2_callback(button_event_t event, void* context) {
    switch (event) {
        case button_event_single_press:
            setOutputState(1, !states[1]);
            break;
        case button_event_long_press:
            break;
        case button_event_double_press:
            handle_double_press(1);
            break;
        default:
            break;
    }
}

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "RHDuoLed-%02X%02X%02X",macaddr[3],macaddr[4],macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "RHDuoLed-%02X%02X%02X",macaddr[3],macaddr[4],macaddr[5]);
    
    int serial_len = snprintf(NULL, 0, "%s%02X%02X%02X",SERIAL_PREFIX,macaddr[3],macaddr[4],macaddr[5]);
    char *serial_value = malloc(serial_len+1);
    snprintf(serial_value, serial_len+1, "%s%02X%02X%02X",SERIAL_PREFIX,macaddr[3],macaddr[4],macaddr[5]);

    name.value = HOMEKIT_STRING(name_value);
    serial.value = HOMEKIT_STRING(serial_value);
}

void on_wifi_ready() {
    homekit_server_init(&config);
}

void gpio_init() {
    gpio_enable(INTERNAL_LED_PIN, GPIO_OUTPUT);
    internal_led_write(false);

    pwm_info.channels = 2;
    multipwm_init(&pwm_info);
    multipwm_set_pin(&pwm_info, 0, OUTPUT1_PIN);
    multipwm_set_pin(&pwm_info, 1, OUTPUT2_PIN);

    setOutputState(0, false);
    setOutputState(1, false);

    gpio_enable(INPUT1_PIN, GPIO_INPUT);
    gpio_enable(INPUT2_PIN, GPIO_INPUT);
}

void user_init(void) {
    uart_set_baud(0, 115200);

    create_accessory_name();

    wifi_config_init("RHDuoLed", NULL, on_wifi_ready);
    gpio_init();

    button_config_t config = BUTTON_CONFIG(
        .active_level = button_active_low,
        .long_press_time = 15000,
    //    .debounce_time = 0,
        .double_press_time = 500,
    );

    if (button_create(INPUT1_PIN, config, button1_callback, NULL)) {
        printf("Failed to initialize button 1\n");
    }

    if (button_create(INPUT2_PIN, config, button2_callback, NULL)) {
        printf("Failed to initialize button 2\n");
    }
}