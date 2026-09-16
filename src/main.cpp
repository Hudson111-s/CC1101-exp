#include <SPI.h>

#include "SmartRC_CC1101.h"

SmartRC_CC1101 radio;

const int PIN_GDO = 2;
const int MAX_PULSES = 400;
const unsigned long TIMEOUT = 50000;
const unsigned long MIN_DUR_LEN = 60;

volatile unsigned int pulseDurations[MAX_PULSES];
volatile bool pulseStates[MAX_PULSES]; // HIGH and LOW

volatile int pulseCount = 0;
volatile unsigned long lastTime = 0;
volatile bool overflowed = false;

unsigned long captureMillis = 0;

void configureRadio() {
    radio.Init();
    radio.setMHZ(314.50);

    radio.setModulation(2); // OOK
    radio.setPktFormat(3); // Async raw
    radio.setPA(10); // TX power

    radio.setRxBW(270);
    radio.setSyncMode(0); // No sync
}

// Triggers on any signal change.
void IRAM_ATTR handlePulse() {
    unsigned long now = micros();
    unsigned long duration = now - lastTime;

    if (duration < MIN_DUR_LEN) return;

    lastTime = now;
    bool currentPinState = digitalRead(PIN_GDO);

    if (pulseCount < MAX_PULSES) {
        pulseDurations[pulseCount] = duration;
        pulseStates[pulseCount] = !currentPinState;
        pulseCount++;
    } else {
        overflowed = true; // buffer full
    }
}

void printCapture() {
    Serial.print("\n--- Captured Burst (");
    Serial.print(pulseCount);
    Serial.println(" transitions) ---");

    if (overflowed) Serial.println("WARNING: buffer filled.");

    for (int i = 1; i < pulseCount; i++) {
        Serial.print(pulseDurations[i]);
        Serial.print(pulseStates[i] ? "H" : "L");
        Serial.print(pulseDurations[i]);
        if (i % 8 == 0) Serial.println();
    }
    Serial.println();
}

void transmitBurst() {
    radio.SetTx();
    delay(10);
    pinMode(PIN_GDO, OUTPUT);

    for (int repeat = 0; repeat < 3; repeat++) {
        for (int i = 1; i < pulseCount; i++) {
            digitalWrite(PIN_GDO, pulseStates[i]);
            delayMicroseconds(pulseDurations[i]);
        }
        delay(12);
    }

    digitalWrite(PIN_GDO, LOW);
    pinMode(PIN_GDO, INPUT);
    radio.setSidle();
    delay(10);
    radio.SetRx();
    delay(10);
}

void setup() {
    Serial.begin(115200);
    delay(300);

    configureRadio();

    if (radio.getCC1101()) {
        Serial.println("CC1101 hardware ready.");
    } else {
        Serial.println("CC1101 hardware NOT found.");
        while (1);
    }

    pinMode(PIN_GDO, INPUT);
    radio.SetRx();

    attachInterrupt(digitalPinToInterrupt(PIN_GDO), handlePulse, CHANGE);
    lastTime = micros();
}

void loop() {
    if (pulseCount > 0 && (micros() - lastTime > TIMEOUT)) {
        detachInterrupt(digitalPinToInterrupt(PIN_GDO));

        if (pulseCount > 20) {
            printCapture();

            captureMillis = millis();
            overflowed = false;

            Serial.println("Type 'y' and press Enter to REPLAY this captured burst...");

            while (!Serial.available()) delay(10);

            char input = Serial.read();
            while (Serial.available()) Serial.read(); // Flush CR/LF

            if (input == 'y' || input == 'Y') {
                Serial.println("Transmiting...");
                transmitBurst();
            } else {
                Serial.println("Transmission CANCELLED.");
            }
        }

        pulseCount = 0;
        overflowed = false;
        attachInterrupt(digitalPinToInterrupt(PIN_GDO), handlePulse, CHANGE);
        lastTime = micros();
    }
}
