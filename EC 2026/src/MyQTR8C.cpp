#include "MyQTR8C.h"

MyQTR8C::MyQTR8C(const uint8_t s[3], uint8_t sig) : _sig(sig), _lastPos(3500) {
  for (int i = 0; i < 3; i++)
    _s[i] = s[i];
}

void MyQTR8C::begin() {
  for (int i = 0; i < 3; i++)
    pinMode(_s[i], OUTPUT);
  analogReadResolution(12);
  for (int i = 0; i < 8; i++) {
    _min[i] = 4095;
    _max[i] = 0;
  }
}

void MyQTR8C::calibrate() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(_s[0], (i >> 0) & 1);
    digitalWrite(_s[1], (i >> 1) & 1);
    digitalWrite(_s[2], (i >> 2) & 1);
    delayMicroseconds(20);
    int v = analogRead(_sig);
    if (v < _min[i])
      _min[i] = v;
    if (v > _max[i])
      _max[i] = v;
  }
}

uint16_t MyQTR8C::readLine(uint16_t *vals) {
  long avg = 0, sum = 0;
  bool onLine = false;
  for (int i = 0; i < 8; i++) {
    digitalWrite(_s[0], (i >> 0) & 1);
    digitalWrite(_s[1], (i >> 1) & 1);
    digitalWrite(_s[2], (i >> 2) & 1);
    delayMicroseconds(20);

    int raw = analogRead(_sig);
    // Đảo ngược logic: 1000 - map để Trắng = 0, Đen = 1000
    vals[i] = 1000 - constrain(map(raw, _min[i], _max[i], 0, 1000), 0, 1000);

    if (vals[i] > 250)
      onLine = true;
    if (vals[i] > 50) {
      avg += (long)vals[i] * (i * 1000);
      sum += vals[i];
    }
  }
  if (!onLine)
    return (_lastPos < 3500) ? 0 : 7000;
  return _lastPos = avg / sum;
}