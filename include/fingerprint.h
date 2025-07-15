#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <Adafruit_Fingerprint.h>
#include <Arduino.h>
#include <HardwareSerial.h>

extern Adafruit_Fingerprint finger;
extern HardwareSerial SerialX;

// Function prototypes
void initFingerprint();
uint8_t getFingerprintEnroll(uint8_t id);
int getFingerprintID();
void enrollFingerprint();
void enrollMode();
void attendanceMode();
void clearAllFingerprints();
void showFingerprintCount();
uint8_t readnumber();

#endif  // FINGERPRINT_H