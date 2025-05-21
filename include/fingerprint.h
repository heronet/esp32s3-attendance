#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
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
uint8_t readnumber();

#endif // FINGERPRINT_H