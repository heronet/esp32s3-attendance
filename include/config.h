#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pin Definitions
#define NEOPIXEL_PIN 48
#define NUM_PIXELS 1
#define SERIAL1RX 17
#define SERIAL1TX 16

// WiFi and Google Sheets Configuration
#define WIFI_CONFIG_FILE "/wifi_config.txt"
#define ATTENDANCE_FILE_PATH "/attendance.csv"
#define GSCRIPT_ID "AKfycby_2izhGidfcOPhpAfs7zhAWXHcK7oeZnUniauozbuc9rR52E7b_BaRJW4IgwTPPsz_rQ"
#define HOST "script.google.com"
#define HTTPS_PORT 443

// BLE UUIDs
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"           // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // RX Characteristic UUID
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // TX Characteristic UUID

#endif // CONFIG_H