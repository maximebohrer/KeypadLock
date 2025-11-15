#ifndef anim_maxime
#define anim_maxime

#include <Arduino.h>

#define ledPin 13
#define buzzerPin A3

extern int ALLUMAGE[];
extern int VALIDATION[];
extern int SHORT_BEEP[];
extern int SHORT_ERROR[];
extern int LONG_ERROR[];
extern int OPEN_SUCCESS[];
extern int CLOSE_SUCCESS[];

extern bool currentLedState;
extern bool defaultLedState;
extern int backgroundBlink;
extern unsigned long blinkTimer;
extern int *sound;
extern bool errorBlink;
extern unsigned long beepTimer;

void ledOn();
void ledOff();
void backgroundBlinkOn(int freq);
void backgroundBlinkOff();
void blinkStart();
void animLoop();
void beep(int *sound, bool error);
void noBeep();

#endif
