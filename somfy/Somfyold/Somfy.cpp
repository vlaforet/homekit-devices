#include <Arduino.h>
#include <Somfy/Somfy.hpp>

using namespace somf;
volatile uint16_t pulses;

void ICACHE_RAM_ATTR handleInterrupt() {
  static uint16_t last;

  pulses = micros() - last;
  last += pulses;
}

// Microseconds
const int _wakeup_pulse = 9415;
const int _wakeup_silence = 89565;
const int _synchro_hw = 2416;
const int _synchro_sw = 4550;
const int _half_symbol = 604;
const int _inter_frame_gap = 30415;

SomfyClass::SomfyClass() {}

void _send(int pin, unsigned char bit, int delayfirst, int delaysec) {
  digitalWrite(pin, (bit-1)%2);
  delayMicroseconds(delayfirst);
  digitalWrite(pin, bit);
  delayMicroseconds(delaysec);
}

void _send(int pin, unsigned char bit, int delay) {
  _send(pin, bit, delay, delay);
}

unsigned long SomfyClass::_transmit(unsigned long address, unsigned long code, unsigned char cmd, unsigned char hwsynchro) {
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
  for (int i = 0; i < 7; ++i) cksum = cksum ^ (data[i] & 0xF) ^ (data[i] >> 4);
  data[1] = data[1] | (cksum);

  // Obfuscation
  unsigned char datax[7];
  datax[0] = data[0];
  for (int i = 1; i < 7; ++i) datax[i] = datax[i - 1] ^ data[i];

  // Wakeup
  _send(_txPin, 0, _wakeup_pulse, _wakeup_silence);

  // Hardware synchronisation
  for (int i = 0; i < hwsynchro; ++i) {
    _send(_txPin, 0, _synchro_hw);
  }

  // Software synchronisation
  _send(_txPin, 0, _synchro_sw, _half_symbol);

  // Sending
  for (int i = 0; i < 56; ++i) {
    unsigned char bit = (datax[i / 8] >> (7 - i % 8)) & 0x01;
    _send(_txPin, bit, _half_symbol);
  }

  digitalWrite(_txPin, 0);
  delayMicroseconds(_inter_frame_gap);

  return code+1;
}

unsigned long SomfyClass::_repeat(int p, unsigned long address, unsigned long code, unsigned char cmd, unsigned char hwsynchro) {
  for (int i = 0; i < p-1; ++i) {
    _transmit(address, code, cmd, hwsynchro);
  }
  return _transmit(address, code, cmd, hwsynchro);
}

unsigned long SomfyClass::prog(unsigned long address, unsigned long code) {
  _transmit(address, code, 0x08, 2);
  return _repeat(20, address, code, 0x08, 7);
}

unsigned long SomfyClass::up(unsigned long address, unsigned long code) {
  _transmit(address, code, 0x02, 2);
  return _repeat(2, address, code, 0x02, 7);
}

unsigned long SomfyClass::stop(unsigned long address, unsigned long code) {
  _transmit(address, code, 0x01, 2);
  return _repeat(2, address, code, 0x01, 7);
}

unsigned long SomfyClass::down(unsigned long address, unsigned long code) {
  _transmit(address, code, 0x04, 2);
  return _repeat(2, address, code, 0x04, 7);
}

void SomfyClass::setPin(int txPin, int rxPin, bool setup) {
  _txPin = txPin;
  _rxPin = rxPin;
  if (setup) {
    pinMode(_txPin, OUTPUT);
    digitalWrite(_txPin, 0);

    pinMode(_rxPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(_rxPin), handleInterrupt, CHANGE);
  }
}

void SomfyClass::loop(loopCallback callback) {
  noInterrupts();
  uint16_t p = pulses;
  pulses = 0;
  interrupts();

  if (p != 0) {
    Message r = pulse(p);
    if (r.pressed != empty) {
      callback(r);
    }
  }
}

Message SomfyClass::_decode() {
  unsigned char frame[7];
  frame[0] = _payload[0];
  for(int i = 1; i < 7; ++i) frame[i] = _payload[i] ^ _payload[i-1];  

  unsigned char cksum = 0;
  for(int i = 0; i < 7; ++i) cksum = cksum ^ frame[i] ^ (frame[i] >> 4);
  cksum = cksum & 0x0F;
  if (cksum != 0) { return nullMessage; }

  Message received;
  switch(frame[1] & 0xF0) {
    case 0x10: received.pressed = myB; break;
    case 0x20: received.pressed = upB; break;
    case 0x40: received.pressed = downB; break;
    case 0x80: received.pressed = progB; break;
    default: return nullMessage; break;
  }
  
  received.rollingCode = (frame[2] << 8) + frame[3];
  received.address = ((unsigned long)frame[4] << 16) + (frame[5] << 8) + frame[6];

  return received;
}

Message SomfyClass::pulse(uint16_t p) {
  switch(_status) {
    case k_waiting_synchro:
      if (p > _synchro_hw*0.7 && p < _synchro_hw*1.3) ++_cpt_synchro_hw;
      else if (p > _synchro_sw*0.7 && p < _synchro_sw*1.3 && _cpt_synchro_hw >= 4) {
        _cpt_bits = 0;
        _previous_bit = 0;
        _waiting_half_symbol = false;
        for(int i=0; i<7; ++i) _payload[i] = 0;
        _status = k_receiving_data;
      } 
      else _cpt_synchro_hw = 0;
      break;
      
    case k_receiving_data:
      if (p > _half_symbol*2*0.7 && p < _half_symbol*2*1.3 && !_waiting_half_symbol) {
        _previous_bit = 1 - _previous_bit;
        _payload[_cpt_bits/8] += _previous_bit << (7 - _cpt_bits%8);
        ++_cpt_bits;
      } 
      else if (p > _half_symbol*0.7 && p < _half_symbol*1.3) {
        if (_waiting_half_symbol) {
          _waiting_half_symbol = false;
          _payload[_cpt_bits/8] += _previous_bit << (7 - _cpt_bits%8);
          ++_cpt_bits;
        } 
        else _waiting_half_symbol = true;
      } 
      else {
        _cpt_synchro_hw = 0;
        _status = k_waiting_synchro;
      }
      break;
  }
  
  if (_status == k_receiving_data && _cpt_bits == 56) {
    _status = k_waiting_synchro;
    return _decode();
  }
  return nullMessage;
}

SomfyClass Somfy;