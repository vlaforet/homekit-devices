#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <espressif/esp_common.h>

#define SLEEPMICROSEC(delay) sdk_os_delay_us(delay)

int tx_pin = 0;

// MICROSECONDS
const float _wakeup_pulse = 9415;
const float _wakeup_silence = 89565;
const float _synchro_hw = 2416;
const float _synchro_sw = 4550;
const float _half_symbol = 604;
const float _inter_frame_gap = 30415;

void rts_send_bit2(char bit, float delayfirst, float delaysec) {
    gpio_write(tx_pin, (bit-1)%2);
    SLEEPMICROSEC(delayfirst);
    gpio_write(tx_pin, bit);
    SLEEPMICROSEC(delaysec);
}

void rts_send_bit(char bit, float delay) {
    rts_send_bit2(bit, delay, delay);
}

void rts_transmit(int32_t address, int32_t code, unsigned char cmd, unsigned char hwsynchro) {
    unsigned char data[7];
    data[0] = 0xA7;
    data[1] = cmd << 4;
    data[2] = (code & 0xFF00) >> 8;
    data[3] = code & 0x00FF;
    data[4] = address >> 16;
    data[5] = address >> 8;
    data[6] = address;

    // Checksum
    unsigned char cksum = 0;
    for (int i = 0; i < 7; ++i) { cksum = cksum ^ (data[i] & 0xF) ^ (data[i] >> 4); }
    data[1] = data[1] | (cksum);

    // Obfuscation
    unsigned char datax[7];
    datax[0] = data[0];
    for (int i = 1; i < 7; ++i) { datax[i] = datax[i - 1] ^ data[i]; }

    // Wakeup
    rts_send_bit2(0, _wakeup_pulse, _wakeup_silence);

    // Hardware synchronisation
    for (int i = 0; i < hwsynchro; ++i) rts_send_bit(0, _synchro_hw);

    // Software synchronisation
    rts_send_bit2(0, _synchro_sw, _half_symbol);

    // Sending
    for (int i = 0; i < 56; ++i) {
        unsigned char bit = (datax[i / 8] >> (7 - i % 8)) & 0x01;
        rts_send_bit(bit, _half_symbol);
    }

    gpio_write(tx_pin, 0);
    SLEEPMICROSEC(_inter_frame_gap);
}

void rts_repeat(int p, int32_t address, int32_t code, unsigned char cmd, unsigned char hwsynchro) {
    for (int i = 0; i < p; ++i) {
        rts_transmit(address, code, cmd, hwsynchro);
    }
}

void rts_up(int32_t address, int32_t code) {
    rts_transmit(address, code, 0x02, 2);
    rts_repeat(2, address, code, 0x02, 7);
}

void rts_down(int32_t address, int32_t code) {
    rts_transmit(address, code, 0x04, 2);
    rts_repeat(2, address, code, 0x04, 7);
}

void rts_stop(int32_t address, int32_t code) {
    rts_transmit(address, code, 0x01, 2);
    rts_repeat(2, address, code, 0x01, 7);
}

void rts_prog(int32_t address, int32_t code) {
    rts_transmit(address, code, 0x08, 2);
    rts_repeat(20, address, code, 0x08, 7);
}

void set_rts_pin(int pin, bool setup) {
    tx_pin = pin;
    if (setup) {
        gpio_enable(tx_pin, GPIO_OUTPUT);
        gpio_write(tx_pin, 0);
    }
}