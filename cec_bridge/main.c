/*
 * duo_led_dimmer - Homekit dimmer for two 12v led lights with wired switches
 * 
 * Will create a hotspot and wait for the user to input their
 * network's credentials.
 *
 * A long press on the oulet's button will reset it to factory defaults.
 */

// tools/gen_qrcode 5 HK_PASSWORD SETUPID qrcode.png

#define HK_PASSWORD "111-11-111"
#define SETUPID "9DF3"
#define SERIAL_PREFIX "RHC"

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
#include <wifi_config.h>

#define MODEL "RH-CEC1"

#define CEC_PIN 5
#define BUTTON_PIN 14
#define INTERNAL_LED_PIN 2

void internal_led_write(bool on) {
    gpio_write(INTERNAL_LED_PIN, on ? 0 : 1);
}

void identify_task(void *_args) {
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            internal_led_write(true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            internal_led_write(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    internal_led_write(false);
    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    printf("Identify\n");
    xTaskCreate(identify_task, "Identify", 128, NULL, 2, NULL);
}

void on_volume_selector(homekit_characteristic_t *ch, homekit_value_t value, void *arg) {
    printf("VOLUME");
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "LED");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, SERIAL_PREFIX);

homekit_service_t input_source1 = 
    HOMEKIT_SERVICE_(INPUT_SOURCE, .characteristics=(homekit_characteristic_t*[]) {
        HOMEKIT_CHARACTERISTIC(NAME, "hdmi1"),
        HOMEKIT_CHARACTERISTIC(IDENTIFIER, 1),
        HOMEKIT_CHARACTERISTIC(CONFIGURED_NAME, "HDMI 1"),
        HOMEKIT_CHARACTERISTIC(INPUT_SOURCE_TYPE, HOMEKIT_INPUT_SOURCE_TYPE_HDMI),
        HOMEKIT_CHARACTERISTIC(IS_CONFIGURED, true),
        HOMEKIT_CHARACTERISTIC(CURRENT_VISIBILITY_STATE, HOMEKIT_CURRENT_VISIBILITY_STATE_SHOWN),
        NULL
    });

homekit_service_t input_source2 = 
    HOMEKIT_SERVICE_(INPUT_SOURCE, .characteristics=(homekit_characteristic_t*[]) {
        HOMEKIT_CHARACTERISTIC(NAME, "hdmi2"),
        HOMEKIT_CHARACTERISTIC(IDENTIFIER, 2),
        HOMEKIT_CHARACTERISTIC(CONFIGURED_NAME, "HDMI 2"),
        HOMEKIT_CHARACTERISTIC(INPUT_SOURCE_TYPE, HOMEKIT_INPUT_SOURCE_TYPE_HDMI),
        HOMEKIT_CHARACTERISTIC(IS_CONFIGURED, true),
        HOMEKIT_CHARACTERISTIC(CURRENT_VISIBILITY_STATE, HOMEKIT_CURRENT_VISIBILITY_STATE_SHOWN),
        NULL
    });

homekit_service_t input_source3 = 
    HOMEKIT_SERVICE_(INPUT_SOURCE, .characteristics=(homekit_characteristic_t*[]) {
        HOMEKIT_CHARACTERISTIC(NAME, "av"),
        HOMEKIT_CHARACTERISTIC(IDENTIFIER, 3),
        HOMEKIT_CHARACTERISTIC(CONFIGURED_NAME, "AV"),
        HOMEKIT_CHARACTERISTIC(INPUT_SOURCE_TYPE, HOMEKIT_INPUT_SOURCE_TYPE_COMPOSITE_VIDEO),
        HOMEKIT_CHARACTERISTIC(IS_CONFIGURED, true),
        HOMEKIT_CHARACTERISTIC(CURRENT_VISIBILITY_STATE, HOMEKIT_CURRENT_VISIBILITY_STATE_SHOWN),
        NULL
    });

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_television, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "RockyHill"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, MODEL),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(TELEVISION, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Television"),
            HOMEKIT_CHARACTERISTIC(ACTIVE, false),
            HOMEKIT_CHARACTERISTIC(ACTIVE_IDENTIFIER, 1),
            HOMEKIT_CHARACTERISTIC(CONFIGURED_NAME, "My TV"),
            HOMEKIT_CHARACTERISTIC(SLEEP_DISCOVERY_MODE, HOMEKIT_SLEEP_DISCOVERY_MODE_ALWAYS_DISCOVERABLE),
            HOMEKIT_CHARACTERISTIC(REMOTE_KEY),
            HOMEKIT_CHARACTERISTIC(PICTURE_MODE, HOMEKIT_PICTURE_MODE_STANDARD),
            HOMEKIT_CHARACTERISTIC(POWER_MODE_SELECTION),
            NULL
        }, .linked=(homekit_service_t*[]){
            &input_source1,
            &input_source2,
            &input_source3,
            NULL
        }),
        &input_source1,
        &input_source2,
        &input_source3,
        HOMEKIT_SERVICE(TELEVISION_SPEAKER, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(MUTE, false),
            HOMEKIT_CHARACTERISTIC(ACTIVE, true),
            HOMEKIT_CHARACTERISTIC(VOLUME_CONTROL_TYPE, HOMEKIT_VOLUME_CONTROL_TYPE_ABSOLUTE),
            HOMEKIT_CHARACTERISTIC(VOLUME_SELECTOR, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_volume_selector)),
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
        vTaskDelay(100 / portTICK_PERIOD_MS);
        internal_led_write(false);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    sdk_system_restart();
    vTaskDelete(NULL);
}

void button_callback(button_event_t event, void* context) {
    switch (event) {
        case button_event_single_press:
            break;
        case button_event_long_press:
            xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
            break;
        case button_event_double_press:
            break;
        default:
            break;
    }
}

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "TV-%02X%02X%02X",macaddr[3],macaddr[4],macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "TV-%02X%02X%02X",macaddr[3],macaddr[4],macaddr[5]);
    
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
    gpio_enable(BUTTON_PIN, GPIO_INPUT);

    internal_led_write(false);
}

void user_init(void) {
    uart_set_baud(0, 115200);

    create_accessory_name();

    wifi_config_init("RHCEC", NULL, on_wifi_ready);
    gpio_init();

    button_config_t config = BUTTON_CONFIG(
        .active_level = button_active_low,
        .long_press_time = 15000,
    //    .debounce_time = 0,
        .double_press_time = 500,
    );

    if (button_create(BUTTON_PIN, config, button_callback, NULL)) {
        printf("Failed to initialize button\n");
    }
}