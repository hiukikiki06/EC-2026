#ifndef MY_QTR8C_H
#define MY_QTR8C_H

#include <Arduino.h>

class MyQTR8C {
private:
  uint8_t _s[3]; // S0, S1, S2
  uint8_t _sig;
  uint16_t _min[8], _max[8];
  int _lastPos;

public:
  MyQTR8C(const uint8_t s[3], uint8_t sig);
  void begin();
  void calibrate();
  uint16_t readLine(uint16_t *vals);
};

#endif