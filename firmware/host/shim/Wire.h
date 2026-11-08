#pragma once

#include <stdint.h>

class TwoWireShim {
 public:
  bool begin(int sda, int scl);
  void end() { started_ = false; }
  void setClock(uint32_t hz) { clock_ = hz; }
  void setTimeOut(uint16_t) {}
  void beginTransmission(uint8_t address) { address_ = address; }
  uint8_t endTransmission();

 private:
  bool started_ = false;
  int sda_ = -1, scl_ = -1;
  uint32_t clock_ = 100000;
  uint8_t address_ = 0;
  unsigned long count_ = 0;
};
extern TwoWireShim Wire;
