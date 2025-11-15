#include "SimpleKeypad.h"
#include "SimpleButton.h"
#include "Anim.h"
#include <EEPROM.h>

#define potPin A0       // Potentiometer input pin
#define stepPin 10      // A4988 step signal
#define dirPin 9        // A4988 direction signal
#define enablePin 12    // A4988 enable signal
#define sleepPin 11     // A4988 sleep signal
#define closeBtnPin A4  // Close button
#define openBtnPin A5   // Open button

#define SLEEP 0
#define AWAKE 1
#define UNLOCKED 2

#define TIME_BEFORE_SLEEP 30000

#define MOTOR_STATE_STOP 0
#define MOTOR_STATE_CLOSE 1
#define MOTOR_STATE_OPEN 2
#define MOTOR_STATE_WAIT_CLOSE 3
#define MOTOR_STATE_WAIT_OPEN 4

#define MOTOR_FREQ_ZERO 10
#define MOTOR_FREQ_MIN 30
#define MOTOR_FREQ_MAX 10000
#define MOTOR_FREQ_DEFAULT 1000

#define MOTOR_ACCEL_MIN 500
#define MOTOR_ACCEL_MAX 60000
#define MOTOR_ACCEL_DEFAULT 10000

#define MOTOR_TIMEOUT_MIN 10
#define MOTOR_TIMEOUT_MAX 5000
#define MOTOR_TIMEOUT_DEFAULT 1000

#define INPUT_BUFFER_SIZE 20

// Menu and settings
byte state = SLEEP;
unsigned long sleepTimer = 0;
struct settings {
	char password[INPUT_BUFFER_SIZE];
	int nbStops;
	int stops[10];
	unsigned int speeds[9];
	unsigned int accel;
	int timeout;
};
settings stngs;
bool trajectoryResetted = false;
char input[INPUT_BUFFER_SIZE] = "";
int inputPointer = 0;

// Motor
byte motorState = MOTOR_STATE_STOP;
int depart = 0;
unsigned long motorTimer = 0;
volatile bool stepPinState = 0;
volatile int motorFreq = MOTOR_FREQ_MIN;
volatile int motorFreqTarget = MOTOR_FREQ_MIN;
volatile int remainder = 0;
unsigned long timeoutTimer;
int currentSegment = 0;

// Buttons and keypad
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
	{'1', '2', '3'},
	{'4', '5', '6'},
	{'7', '8', '9'},
	{'*', '0', '#'}
};
byte rowPins[ROWS] = {3, 8, 7, 5};
byte colPins[COLS] = {4, 2, 6};
SimpleKeypad clav((char*)keys, rowPins, colPins, ROWS, COLS);
SimpleButton closeBtn(closeBtnPin);   // Button on pin A4
SimpleButton openBtn(openBtnPin);     // Button on pin A5
SimpleButton starBtn(rowPins[3]);     // Button '*' of keypad. The star key is both a keypad key and a normal button, so that we can just check for this button in sleep mode instead of scanning the whole keypad.


void setup() {
	pinMode(stepPin, OUTPUT);
	pinMode(dirPin, OUTPUT);
	pinMode(enablePin, OUTPUT);
	pinMode(sleepPin, OUTPUT);
	pinMode(potPin, INPUT);
	pinMode(ledPin, OUTPUT);
	pinMode(buzzerPin, OUTPUT);

	digitalWrite(enablePin, HIGH);
	digitalWrite(sleepPin, LOW);
	digitalWrite(ledPin, HIGH);
	delay(500);
	digitalWrite(ledPin, LOW);
	pinMode(colPins[0], OUTPUT);    // on commance avec la touche * en mode bouton
	digitalWrite(colPins[0], LOW);  //

	// Timer 1 configuration
	TCCR1A = 0b00000000;
	TCCR1B = 0b00001000; // CTC mode (Clear Timer on Compare Match) with OCR1A as top. No clock source (timer stopped).
	TIMSK1 = 0b00000000; // All interrupts are disabled.
	TCNT1 = 0;           // Initializes the timer counter value to 0.

	loadSettings();
	resetCheckOnStartup();
}

void loop() {
	if (state != SLEEP) {
		// System is awake
		char key = clav.getKey();
		if (closeBtn.getState()) {
			// Close button
			if (motorState == MOTOR_STATE_STOP) {
				wakeUp();
				motorStart(MOTOR_STATE_CLOSE);
			} else {
				motorStop();
				beep(ALLUMAGE, false);
			}
		} else if (openBtn.getState()) {
			// Open button
			if (motorState == MOTOR_STATE_STOP) {
				wakeUp();
				motorStart(MOTOR_STATE_OPEN);
			} else {
				motorStop();
				beep(ALLUMAGE, false);
			}
		} else if (key) {
			// Keypad key
			wakeUp();
			if (key == '#') {
				handlePoundKey();
			} else if (key == '*') {
				handleStarKey();
			} else {
				inputAdd(key);
				beep(SHORT_BEEP, false);
			}
		}

		// Calling other loops
		if (motorState != MOTOR_STATE_STOP) {
			motorLoop();   // Motor loop
		} else {
			sleepLoop();   // Sleep management loop
		}
		animLoop();      // Animations loop
	} else {
		// System sleeping
		if (closeBtn.getState()) {
			// Close button
			wakeUp();
			motorStart(MOTOR_STATE_WAIT_CLOSE); // If the system is woken up by the open or close button, give it a bit of time before starting the motor
		} else if (openBtn.getState()) {
			// Open button
			wakeUp();
			motorStart(MOTOR_STATE_WAIT_OPEN);
		} else if (starBtn.getState()) {
			// Star button
			clav.ignoreNextKey('*'); // When the system wakes up, the scan of the keypad will resume, so we need to not register the star key since it's pushed down at this point.
			wakeUp();
			beep(ALLUMAGE, false);
		}
	}
}

void wakeUp() {
	if (state == SLEEP) {
		state = AWAKE;
		ledOn();
		digitalWrite(sleepPin, HIGH);
		digitalWrite(colPins[0], HIGH);
		pinMode(colPins[0], INPUT);
	}
	sleepTimer = millis();
}

void sleep() {
	state = SLEEP;
	digitalWrite(sleepPin, LOW);
	backgroundBlinkOff();
	ledOff();
	noBeep();
	inputReset();
	pinMode(colPins[0], OUTPUT);
	digitalWrite(colPins[0], LOW);
	starBtn.ignoreNextPress();
	trajectoryResetted = false;
}

void sleepLoop() {
	if (millis() - sleepTimer > TIME_BEFORE_SLEEP) {
		sleep();
	}
}

void inputAdd(char c) {
	if (inputPointer < INPUT_BUFFER_SIZE - 1) {
		input[inputPointer] = c;
		inputPointer++;
		input[inputPointer] = '\0'; // Null terminate string
	}
}

void inputReset() {
	inputPointer = 0;
	input[0] = '\0'; // Null terminate string
}

void handleStarKey() {
	if (input[0]) {
		inputReset();
		beep(ALLUMAGE, false);
	} else if (motorState != MOTOR_STATE_STOP) {
		motorStop();
		beep(ALLUMAGE, false);
	} else {
		sleep();
	}
}

void handlePoundKey() {
	if (!strncmp(input, stngs.password, INPUT_BUFFER_SIZE)) {
		state = UNLOCKED;
		motorStart(MOTOR_STATE_OPEN);
	} else if (state == UNLOCKED && !strncmp(input, "1", INPUT_BUFFER_SIZE) && motorState == MOTOR_STATE_STOP) {
		// open
		motorStart(MOTOR_STATE_OPEN);
	} else if (!strncmp(input, "0", INPUT_BUFFER_SIZE) && motorState == MOTOR_STATE_STOP) {
		// close
		motorStart(MOTOR_STATE_CLOSE);
	} else if (state == UNLOCKED && !strncmp(input, "7771", 4)) {
		// Password change
		int len = strnlen(input + 4, INPUT_BUFFER_SIZE - 4);
		if (len >= 4 && len <= 12 && strncmp(input + 4, "777", 3)) {
			strncpy(stngs.password, input + 4, INPUT_BUFFER_SIZE);
			saveSettings();
			beep(VALIDATION, false);
		} else {
			beep(SHORT_ERROR, true);
		}
	} else if (state == UNLOCKED && !strncmp(input, "7772", 4)) {
		// Key rotation trajectory / end stops
		if (!input[4]) {
			// Reset trajectory, save first point (door closed)
			trajectoryResetted = true;
			stngs.stops[0] = get_pos();
			stngs.nbStops = 1;
			saveSettings();
			beep(VALIDATION, false);
		} else {
			// Save next points with associated speeds in Hz (until door open)
			int pos = get_pos();
			long speed = strtol(input + 4, NULL, 10);                 // previous point
			if (trajectoryResetted && stngs.nbStops < 10 && pos > stngs.stops[stngs.nbStops - 1] + 1 && speed >= MOTOR_FREQ_MIN && speed <= MOTOR_FREQ_MAX) {
				stngs.stops[stngs.nbStops] = pos;
				stngs.speeds[stngs.nbStops - 1] = speed;
				stngs.nbStops += 1;
				saveSettings();
				beep(VALIDATION, false);
			} else {
				beep(SHORT_ERROR, true);
			}
		}
	} else if (state == UNLOCKED && !strncmp(input, "7773", 4)) {
		// Acceleration in Hz/s: enables smooth speed transitions during the trajectory
		long accel = strtol(input + 4, NULL, 10);
		if (accel >= MOTOR_ACCEL_MIN && accel <= MOTOR_ACCEL_MAX) {
			stngs.accel = accel;
			saveSettings();
			beep(VALIDATION, false);
		} else {
			beep(SHORT_ERROR, true);
		}
	} else if (state == UNLOCKED && !strncmp(input, "7774", 4)) {
		// Timeout in ms
		long timeout = strtol(input + 4, NULL, 10);
		if (timeout >= MOTOR_TIMEOUT_MIN && timeout <= MOTOR_TIMEOUT_MAX) {
			stngs.timeout = timeout;
			saveSettings();
			beep(VALIDATION, false);
		} else {
			beep(SHORT_ERROR, true);
		}
	} else {
		beep(SHORT_ERROR, true);
	}
	inputReset();
}

void resetCheckOnStartup() {
	int mustReset = 0;
	for (int i = 0; i < 5; i++) {
		delay(20);
		if (!digitalRead(closeBtnPin) && !digitalRead(openBtnPin)) { // 2 buttons pressed at the same time
			mustReset += 1;
		} else {
			break;
		}
	}
	if (mustReset == 5) {
		// Reset every setting to its default value
		strncpy(stngs.password, "001234500", INPUT_BUFFER_SIZE);
		stngs.nbStops = 4;
		stngs.stops[0] = 362; stngs.stops[1] = 462; stngs.stops[2] = 562; stngs.stops[3] = 662;                                     // Example trajectory points
		stngs.speeds[0] = 3 * MOTOR_FREQ_DEFAULT; stngs.speeds[1] = MOTOR_FREQ_DEFAULT; stngs.speeds[2] = 3 * MOTOR_FREQ_DEFAULT;   // Example trajectory speeds
		stngs.accel = MOTOR_ACCEL_DEFAULT;
		stngs.timeout = MOTOR_TIMEOUT_DEFAULT;
		saveSettings();
		tone(buzzerPin, 440, 1000);
		closeBtn.ignoreNextPress();  // do not register button press when the main loop starts
		openBtn.ignoreNextPress();
	}
}

void saveSettings() {
	EEPROM.put(0, stngs);
}

void loadSettings() {
	EEPROM.get(0, stngs);
	if (stngs.nbStops > 10 || stngs.nbStops < 0) stngs.nbStops = 0;
	for (int i = 0; i < stngs.nbStops - 1; i++) {
		if (stngs.speeds[i] < MOTOR_FREQ_MIN || stngs.speeds[i] > MOTOR_FREQ_MAX) {
			stngs.speeds[i] = MOTOR_FREQ_MIN;
		}
	}
	if (stngs.accel < MOTOR_ACCEL_MIN || stngs.accel > MOTOR_ACCEL_MAX) {
		stngs.accel = MOTOR_ACCEL_DEFAULT;
	}
	if (stngs.timeout < MOTOR_TIMEOUT_MIN || stngs.timeout > MOTOR_TIMEOUT_MAX) {
		stngs.timeout = MOTOR_TIMEOUT_DEFAULT;
	}
}

//================================== MOTOR ================================================================================

void motorStart(int direction) {
	// If we need to wait a few ms before starting (to let time to the potentiometer to reach its voltage) we call this function with MOTOR_STATE_WAIT_CLOSE. It will later automatically be called with MOTOR_STATE_CLOSE.
	if (direction == MOTOR_STATE_WAIT_CLOSE || direction == MOTOR_STATE_WAIT_OPEN) {
		motorState = direction;
		motorTimer = millis();
		return;
	}

	// Verify current position, cancel if key already in open or close position
	int pos = get_pos();                                        // first point
	if (stngs.nbStops < 2 || direction == MOTOR_STATE_CLOSE && pos < stngs.stops[0] + 2) {
		motorState = MOTOR_STATE_STOP;
		beep(CLOSE_SUCCESS, true);
		return;                                                   // last point
	} else if (direction == MOTOR_STATE_OPEN && pos > stngs.stops[stngs.nbStops - 1] - 2) {
		motorState = MOTOR_STATE_STOP;
		beep(OPEN_SUCCESS, true);
		return;
	}

	beep(VALIDATION, false);
	motorState = direction; // 1 si ouvrir, 2 si fermer
	backgroundBlinkOn(300);
	digitalWrite(enablePin, LOW);
	stepPinState = 0;
	motorTimer = millis();
	timeoutTimer = millis();
	if (direction == MOTOR_STATE_CLOSE) {
		digitalWrite(dirPin, LOW);
		currentSegment = stngs.nbStops - 2;
		depart = INT16_MAX;
	} else if (direction == MOTOR_STATE_OPEN) {
		digitalWrite(dirPin, HIGH);
		currentSegment = 0;
		depart = INT16_MIN;
	}
	TCCR1A = 0b00000000;
	TCCR1B = 0b00001011;  // CTC mode (Clear Timer on Compare Match) with OCR1A as top. Clock source = prescaler 64 (the timer increments every 64 CPU cycles and resets to 0 when it matches OCR1A.)
	TIMSK1 = 0b00000010;  // Output compare A interrupt enabled.
	TCNT1 = 0;            // Initializes the timer counter value to 0.
	motorFreq = MOTOR_FREQ_ZERO;                     // Current motor frequency (number of motor steps per sec, or number of timer interrupts per sec) (Hz)
	motorFreqTarget = stngs.speeds[currentSegment];  // Motor frequency we want to reach
	OCR1A = 16000000 / 64 / MOTOR_FREQ_ZERO;         // Timer increments 16000000 / 64 = 250000 times per sec. If we set OCR1A to 250000 it will trigger an interrupt every 1 sec. So we divide again by the required frequency.
}

void motorStop() {
	TCCR1A = 0b00000000;
	TCCR1B = 0b00001000;  // CTC mode (Clear Timer on Compare Match) with OCR1A as top. No clock source (timer stopped).
	TIMSK1 = 0b00000000;  // All interrupts are disabled.
	TCNT1 = 0;            // Initializes the timer counter value to 0.
	motorState = MOTOR_STATE_STOP;
	backgroundBlinkOff();
	digitalWrite(dirPin, LOW);
	digitalWrite(enablePin, HIGH);
	wakeUp();
}

ISR(TIMER1_COMPA_vect) { // Interrupt handler comparaison timer1 OCR1A
	stepPinState = !stepPinState;
	digitalWrite(stepPin, stepPinState);

	if (motorFreq < motorFreqTarget) {
		int acceleration = (stngs.accel + remainder) / motorFreq; // The change in frequency according to acceleration is done at every interrupt. We multiply acceleration (Hz/s) by the time sice last interrupt (1 / motorFreq) (s) to find by how many Hz the speed should change.
		remainder = (stngs.accel + remainder) % motorFreq;        // We keep the reminder and add it to the next division, to have less rounding errors, and to prevent the division result from being zero forever.
		motorFreq += acceleration;
		if (motorFreq > motorFreqTarget) motorFreq = motorFreqTarget;
	} else if (motorFreq > motorFreqTarget) {
		int acceleration = (stngs.accel + remainder) / motorFreq;
		remainder = (stngs.accel + remainder) % motorFreq;
		motorFreq -= acceleration;
		if (motorFreq < motorFreqTarget) motorFreq = motorFreqTarget;
	}
	OCR1A = 16000000 / 64 / motorFreq;
}

void motorLoop() {
	if (motorState == MOTOR_STATE_WAIT_CLOSE || motorState == MOTOR_STATE_WAIT_OPEN) {
		if (millis() - motorTimer > 100) {
			motorStart(motorState == MOTOR_STATE_WAIT_OPEN ? MOTOR_STATE_OPEN : MOTOR_STATE_CLOSE);
		}
		return;
	}

	if (millis() - motorTimer > 10) {
		motorTimer = millis();
		int pos = get_pos();

		if (motorState == MOTOR_STATE_CLOSE) {
			// Change speed according to current position and trajectory settings
			if (currentSegment >= 0 && pos < stngs.stops[currentSegment]) {
				currentSegment -= 1;
				motorFreqTarget = currentSegment >= 0 ? stngs.speeds[currentSegment] : MOTOR_FREQ_ZERO;
			}
			// Detection of motor not spinning
			if (pos < depart) {
				depart = pos;
				timeoutTimer = millis();
			}
		} else if (motorState == MOTOR_STATE_OPEN) {
			// Change speed according to current position and trajectory settings
			if (currentSegment < stngs.nbStops - 1 && pos > stngs.stops[currentSegment + 1]) {
				currentSegment += 1;
				motorFreqTarget = currentSegment < stngs.nbStops - 1 ? stngs.speeds[currentSegment] : MOTOR_FREQ_ZERO;
			}
			// Detection of motor not spinning
			if (pos > depart) {
				depart = pos;
				timeoutTimer = millis();
			}
		}

		// Stop motor if not spinning
		if (millis() - timeoutTimer > stngs.timeout) {
			motorStop();
			beep(LONG_ERROR, true);
		}

		// Stop motor when deceleration complete / speed reached zero
		if (motorFreqTarget == MOTOR_FREQ_ZERO && motorFreq == MOTOR_FREQ_ZERO) {
			int lastState = motorState;
			motorStop();
			beep(lastState == MOTOR_STATE_OPEN ? OPEN_SUCCESS : CLOSE_SUCCESS, false);
		}
	}
}

int get_pos() {
	return analogRead(potPin);
}