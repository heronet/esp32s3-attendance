#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

// Globals
extern String storedSSID;
extern String storedPassword;

// Function prototypes
void loadWiFiCredentials();
void saveWiFiCredentials(const String &newSSID, const String &newPassword);
void updateWiFiSettings();
void connectToWiFi();
void disconnectWiFi();

#endif // WIFI_MANAGER_H