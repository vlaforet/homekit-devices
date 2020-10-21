#pragma once

namespace somf {
  enum t_status {
    k_waiting_synchro,
    k_receiving_data
  };

  enum button {
    myB,
    upB,
    downB,
    progB,
    empty
  };

  struct Message {
    unsigned long address;
    unsigned long rollingCode;
    button pressed;
  };

  typedef void (*loopCallback)(Message m);

  class SomfyClass {
    public:
      Message nullMessage = Message{0, 0, empty};

      SomfyClass();
      void setPin(int txPin, int rxPin, bool setup);

      unsigned long prog(unsigned long address, unsigned long code);
      unsigned long up(unsigned long address, unsigned long code);
      unsigned long stop(unsigned long address, unsigned long code);
      unsigned long down(unsigned long address, unsigned long code);

      Message pulse(uint16_t p);
      void loop(loopCallback callback);

    private:
      t_status _status;
      byte _cpt_synchro_hw;
      byte _cpt_bits;
      byte _previous_bit;
      bool _waiting_half_symbol;
      byte _payload[7];
      int _txPin;
      int _rxPin;
      Message _decode();

      unsigned long _transmit(unsigned long address, unsigned long codep, byte cmd, byte first);
      unsigned long _repeat(int p, unsigned long address, unsigned long code, byte cmd, byte first);
  };
};

extern somf::SomfyClass Somfy;
