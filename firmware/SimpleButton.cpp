#include "SimpleButton.h"

SimpleButton::SimpleButton(int pin) {
  _pin = pin;
  pinMode(pin, INPUT_PULLUP);
}

bool SimpleButton::getState() {
  if (millis() - scanTimer > scanPeriod) {
    scanTimer = millis();
    bool currentState = !digitalRead(_pin); //lecture bouton. Low = appuyé
    if (currentState != lastState) {
      lastState = currentState;
      debounceTimer = millis(); //démarrage timer
      hold = false;
    } else if (currentState && !hold && (millis() - debounceTimer > debounceTime)) { //attente du timer
      hold = 1; //bonton appuyé depuis assez longtemps
      return true;
    }
    return false;
  }
  return false;
}

void SimpleButton::ignoreNextPress() {
  lastState = true;
  hold = true;
}
