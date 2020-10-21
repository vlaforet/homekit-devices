#pragma once

void set_rts_pin(int pin, bool setup);
void rts_transmit(int32_t address, int32_t code, unsigned char cmd, unsigned char hwsynchro);
void rts_up(int32_t address, int32_t code);
void rts_down(int32_t address, int32_t code);
void rts_stop(int32_t address, int32_t code);
void rts_prog(int32_t address, int32_t code);