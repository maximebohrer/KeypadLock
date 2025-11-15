#include "Anim.h"

int ALLUMAGE[] = {440, 50, 659, 50, -1};
int VALIDATION[] = {554, 50, 659, 50, 880, 100, -1};
int SHORT_BEEP[] = {659, 100, -1};
int SHORT_ERROR[] = {200, 100, 0, 50, 200, 100, -1};
int LONG_ERROR[] = {200, 100, 0, 50, 200, 100, 0, 50, 200, 1000, -1};
int OPEN_SUCCESS[] = {440, 50, 554, 50, 659, 50, 880, 50, 1108, 50, 1319, 50, 1760, 100, -1};
int CLOSE_SUCCESS[] = {1760, 50, 1319, 50, 1108, 50, 880, 50, 659, 50, 554, 50, 440, 100, -1};

bool currentLedState = false;

bool defaultLedState = false;  // Priority 0

int backgroundBlink = 0;       // Priority 1
unsigned long blinkTimer = 0;

int *sound = NULL;             // Priority 2
bool errorBlink = false;
unsigned long beepTimer = 0;


void ledOn() {
	defaultLedState = true;
	if (!backgroundBlink && !sound) {
		currentLedState = true;
		digitalWrite(ledPin, true);
	}
}

void ledOff() {
	defaultLedState = false;
	if (!backgroundBlink && !sound) {
		currentLedState = false;
		digitalWrite(ledPin, false);
	}
}

void backgroundBlinkOn(int freq) {
	backgroundBlink = freq;
	if (!sound) {
		blinkStart();
	}
}

void backgroundBlinkOff() {
	backgroundBlink = 0;
	if (!sound) {
		currentLedState = defaultLedState;
		digitalWrite(ledPin, defaultLedState);
	}
}

void blinkStart() {
	currentLedState = !currentLedState;
	digitalWrite(ledPin, currentLedState);
	blinkTimer = millis();
}

void animLoop() {
	if (sound) {
		if (millis() - beepTimer > *(sound+1)) {
			sound += 2;
			if (*sound > -1) {
				beepTimer = millis();
				if (*sound) tone(buzzerPin, *sound, *(sound+2) > 0 ? 2000 : *(sound+1));
			} else {
				noBeep();
			}
		}
		if (errorBlink) {
			if (millis() - blinkTimer > 50) {
				blinkTimer = millis();
				currentLedState = !currentLedState;
				digitalWrite(ledPin, currentLedState);
			}
		}
	} else if (backgroundBlink) {
		if (millis() - blinkTimer > backgroundBlink) {
			blinkTimer = millis();
			currentLedState = !currentLedState;
			digitalWrite(ledPin, currentLedState);
		}
	}
}

void beep(int *song, bool error) {
	errorBlink = error;
	sound = song;
	beepTimer = millis();
	if (*sound) tone(buzzerPin, *sound, *(sound+2) > 0 ? 2000 : *(sound+1));
	if (!error) {
		currentLedState = false;
		digitalWrite(ledPin, false);
	} else {
		blinkStart();
	}
}

void noBeep() {
	noTone(buzzerPin);
	sound = NULL;
	errorBlink = false;
	if (backgroundBlink) {
		blinkStart();
	} else {
		currentLedState = defaultLedState;
		digitalWrite(ledPin, defaultLedState);
	}
}