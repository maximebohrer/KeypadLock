#ifndef SIMPLEBUTTON_H
#define SIMPLEBUTTON_H

#include <Arduino.h>

class SimpleButton {
  public:
    SimpleButton(int pin);
    bool getState();
    void ignoreNextPress();

  private:
    byte _pin;
    bool lastState = false;
    bool hold = false;
    unsigned long debounceTimer = 0;
    unsigned long scanTimer = 0;
    static const int debounceTime = 10; //temps minimum à appuyer
    static const int scanPeriod = 2;
};

#endif
