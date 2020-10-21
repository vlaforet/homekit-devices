/*
 * pattern - demo project
 * 
 * Will create a hotspot and wait for the user to input their
 * network's credentials.
 */

#include <stdio.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include "tftspi.h"
#include "tft.h"

#define SPI_BUS TFT_HSPI_HOST

void user_init(void) {
    uart_set_baud(0, 115200);

    TFT_PinsInit();

    spi_lobo_device_handle_t spi;
	
    spi_lobo_bus_config_t buscfg={
        .mosi_io_num=13,				// set SPI MOSI pin
        .sclk_io_num=14,				// set SPI CLK pin
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
		.max_transfer_sz = 6*1024,
    };
    spi_lobo_device_interface_config_t devcfg={
        .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
        .mode=3,                                // SPI mode 0
		.flags=LB_SPI_DEVICE_HALFDUPLEX,        // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
    };

    ret=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
    assert(ret==ESP_OK);
	printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
	tft_disp_spi = spi;

    TFT_display_init();

    static char tmp_buff[64];
    sprintf(tmp_buff, "Hello");
    TFT_print(tmp_buff, CENTER, CENTER);
}
