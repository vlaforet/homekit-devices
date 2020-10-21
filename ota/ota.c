/*
 * rh_switch - Homekit switch for any standard esp8266 based outlet
 * 
 * Will create a hotspot and wait for the user to input their
 * network's credentials.
 *
 * A long press on the oulet's button will reset it to factory defaults.
 */

#define LED_PIN 5

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <wifi_config.h>

#include "ota-tftp.h"
#include "rboot-api.h"
#include "toggle.h"
#include "led_status.h"
#include "ws2812_i2s/ws2812_i2s.h"

#include <sntp.h>
#include <time.h>

#define SNTP_SERVERS 	"0.pool.ntp.org", "1.pool.ntp.org", \
						"2.pool.ntp.org", "3.pool.ntp.org"

led_status_pattern_t identify = { .n=12, .delay=(int[]){ 100, 100, 100, 350, 100, 100, 100, 350, 100, 100, 100, 350 } };
led_status_pattern_t test = { .n=2, .delay=(int[]){ 1000, 1000 } };
static led_status_t led_status;

#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_PERIOD_MS)

void sntp_tsk()
{
	const char *servers[] = {SNTP_SERVERS};

	/* Wait until we have joined AP and are assigned an IP */
	while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
		vTaskDelayMs(100);
	}

	/* Start SNTP */
	printf("Starting SNTP... ");
    LOCK_TCPIP_CORE();
	/* SNTP will request an update each 5 minutes */
	sntp_set_update_delay(5*60000);
	/* Set GMT+1 zone, daylight savings off */
	const struct timezone tz = {1*60, 0};
	/* SNTP initialization */
	sntp_initialize(&tz);
	/* Servers must be configured right after initialization */
	sntp_set_servers(servers, sizeof(servers) / sizeof(char*));
    UNLOCK_TCPIP_CORE();
	printf("DONE!\n");

	/* Print date and time each 5 seconds */
	while(1) {
		vTaskDelayMs(5000);
		time_t ts = time(NULL);
		printf("TIME: %s", ctime(&ts));
	}
}

/*
const uint32_t led_number = 90;
const uint32_t tail_fade_factor = 2;
const uint32_t tail_length = 15;

static void fade_pixel(ws2812_pixel_t *pixel, uint32_t factor)
{
    pixel->red = pixel->red / factor;
    pixel->green = pixel->green / factor;
    pixel->blue = pixel->blue / factor;
}

static int fix_index(int index)
{
    if (index < 0) {
        return (int)led_number + index;
    } else if (index >= led_number) {
        return index - led_number;
    } else {
        return index;
    }
}

static ws2812_pixel_t next_colour()
{
    ws2812_pixel_t colour = { {0, 0, 0, 0} };
    colour.red = rand() % 256;
    colour.green = rand() % 256;
    colour.blue = rand() % 256;

    return colour;
}

static void demo(void *pvParameters)
{
    ws2812_pixel_t pixels[led_number];
    int head_index = 0;

    ws2812_i2s_init(led_number, PIXEL_RGB);

    memset(pixels, 0, sizeof(ws2812_pixel_t) * led_number);    

    while (1) {
        pixels[head_index] = next_colour();
        for (int i = 0; i < led_number; i++) {
            head_index = fix_index(head_index + 1);
            pixels[head_index] = pixels[fix_index(head_index-1)];
            for (int ii = 1; ii < tail_length; ii++) {
                fade_pixel(&pixels[fix_index(head_index - ii)], tail_fade_factor);
            }
            memset(&pixels[fix_index(head_index - tail_length)], 0, 
                    sizeof(ws2812_pixel_t));

            ws2812_i2s_update(pixels, PIXEL_RGB);
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
    }
}*/

void toggle_callback(bool high, void *context) {
    printf("toggle is %s\n", high ? "high" : "low");
}

void user_init(void) {
    uart_set_baud(0, 115200);
    wifi_config_init("RH", NULL, NULL);
    ota_tftp_init_server(TFTP_PORT);
    led_status = led_status_init(2);

    int r = toggle_create(5, toggle_callback, NULL);
    if (r) {
        printf("Failed to initialize a toggle\n");
    }

    //xTaskCreate(demo, "strip demo", 256, NULL, 10, NULL);
    xTaskCreate(sntp_tsk, "SNTP", 1024, NULL, 1, NULL);
}
