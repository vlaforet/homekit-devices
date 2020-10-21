/*
 * clock - LED strip clock
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
#include <string.h>

#include "led_status.h"
#include "ota-tftp.h"
#include "rboot-api.h"
#include "ws2812_i2s/ws2812_i2s.h"

#include <sntp.h>
#include <time.h>
#include <math.h>

#define MODEL "RH-C1"
#define SERIAL_PREFIX "OTL"
#define FIRMWARE_REVISION "0.0.1"
#define LED_PIN 2
#define LED_NUMBER 60

#define SNTP_SERVERS 	"0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org", "3.pool.ntp.org"

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define STATE_WAITING 0
#define STATE_READY 1
#define STATE_CLOCK 2
#define STATE_OFF 3

static led_status_t led_status;
led_status_pattern_t identify = { 4, (int[]){ 100, 100, 100, 350 } };
led_status_pattern_t activity = { 2, (int[]){ 100, 100 } };
led_status_pattern_t waiting =  { 2, (int[]){ 300, 150 } };

ws2812_pixel_t pixels[LED_NUMBER];
int state = STATE_WAITING;
int brightness = 25;
bool show_seconds = true;

int rgb_sec[3] = {0, 0, 255};
int rgb_min[3] = {0, 255, 0};
int rgb_hours[3] = {255, 0, 0};

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Clock");
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, SERIAL_PREFIX);

ws2812_pixel_t colorForBrightness(int red, int green, int blue, int bright) {
    float b = bright/100.;
    ws2812_pixel_t c = { {b*blue, b*green, b*red, 255} };
    return c;
}

ws2812_pixel_t add(ws2812_pixel_t c1, ws2812_pixel_t c2) {
    c1.red = MIN(c1.red + c2.red, 255);
    c1.green = MIN(c1.green + c2.green, 255);
    c1.blue = MIN(c1.blue + c2.blue, 255);
    return c1;
}

void reset_pixels() {
    ws2812_pixel_t black = colorForBrightness(0, 0, 0, 0);
    for (int i=0;i<LED_NUMBER;i++) {
        pixels[i] = black;
    }
}

void update_clock() {
    time_t ct = sntp_get_rtc_time(NULL);
    struct tm *t = localtime(&ct);

    int hours_led = LED_NUMBER - (((t->tm_hour % 12) * 5) % LED_NUMBER);
    int min_led = LED_NUMBER - (t->tm_min % LED_NUMBER);
    int sec_led = LED_NUMBER - (t->tm_sec % LED_NUMBER);

    reset_pixels();
    ws2812_pixel_t hours_color = colorForBrightness(rgb_hours[0], rgb_hours[1], rgb_hours[2], brightness);
    ws2812_pixel_t min_color = colorForBrightness(rgb_min[0], rgb_min[1], rgb_min[2], brightness);
    ws2812_pixel_t sec_color = pixels[sec_led];

    if (show_seconds) {
        sec_color = colorForBrightness(rgb_sec[0], rgb_sec[1], rgb_sec[2], brightness);
    }

    pixels[sec_led] = add(pixels[sec_led], sec_color);
    pixels[min_led] = add(pixels[min_led], min_color);
    pixels[hours_led] = add(pixels[hours_led], hours_color);

    ws2812_i2s_update(pixels, PIXEL_RGB);
}

void sntp_task() {
	const char *servers[] = {SNTP_SERVERS};

	printf("Starting SNTP...\n");
    LOCK_TCPIP_CORE();
	sntp_set_update_delay(5*60000); // 5 minutes
	const struct timezone tz = {1*60, 0};
	sntp_initialize(&tz);
	sntp_set_servers(servers, sizeof(servers) / sizeof(char*));
    UNLOCK_TCPIP_CORE();
    state = STATE_CLOCK;

	while(true) {
		vTaskDelay(500 / portTICK_PERIOD_MS);
        if (state == STATE_CLOCK) {
		    update_clock();
        } else {
            reset_pixels();
            ws2812_i2s_update(pixels, PIXEL_RGB);
        }
	}
}

void waiting_task() {
    ws2812_pixel_t white = colorForBrightness(255, 255, 255, brightness);
    ws2812_pixel_t black = colorForBrightness(0, 0, 0, 0);
    int i = 0;

    while (state != STATE_CLOCK) {
        pixels[i] = black;
        i++;
        if (i >= LED_NUMBER)
            i = 0;
        pixels[i] = white;

        ws2812_i2s_update(pixels, PIXEL_RGB);
        if (state == STATE_WAITING) {vTaskDelay(100 / portTICK_PERIOD_MS);}
        else {vTaskDelay(25 / portTICK_PERIOD_MS);}
    }

    pixels[i] = black;
    ws2812_i2s_update(pixels, PIXEL_RGB);
    vTaskDelete(NULL);
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

void hk_identify(homekit_value_t _value) {
    led_status_set_repeat(led_status, &identify, 3);
}

homekit_value_t get_on() {return HOMEKIT_BOOL(state == STATE_CLOCK);}
void set_on(homekit_value_t value) {state = value.bool_value ? STATE_CLOCK : STATE_OFF;}

homekit_value_t get_brightness() {return HOMEKIT_INT(brightness);}
void set_brightness(homekit_value_t value) {brightness = value.int_value;}

// SECONDS
homekit_value_t get_sec_on() {return HOMEKIT_BOOL(show_seconds);}
void set_sec_on(homekit_value_t value) {show_seconds = value.bool_value;}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_lightbulb, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "RockyHill"),
            &serial,
            HOMEKIT_CHARACTERISTIC(MODEL, MODEL),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, FIRMWARE_REVISION),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, hk_identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Brightness"),
            HOMEKIT_CHARACTERISTIC(
                ON, true,
                .getter=get_on,
                .setter=set_on
            ),
            HOMEKIT_CHARACTERISTIC(
                BRIGHTNESS, 100,
                .getter=get_brightness,
                .setter=set_brightness
            ),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Seconds"),
            HOMEKIT_CHARACTERISTIC(
                ON, true,
                .getter=get_sec_on,
                .setter=set_sec_on
            ),
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
    state = STATE_READY;
    xTaskCreate(sntp_task, "SNTP", 1024, NULL, 1, NULL);
}

void setup_leds() {
    ws2812_i2s_init(LED_NUMBER, PIXEL_RGB);
    memset(pixels, 0, sizeof(ws2812_pixel_t) * LED_NUMBER);
}

void serial_construct() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "Clock-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Clock-%02X%02X%02X",
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
    setup_leds();
    led_status = led_status_init(2);

    led_status_set(led_status, &waiting);
    xTaskCreate(waiting_task, "waiting", 256, NULL, 10, NULL);

    ota_tftp_init_server(TFTP_PORT);
    serial_construct();
    wifi_config_init("RH", NULL, on_wifi_ready);
}
