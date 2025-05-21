#include <Arduino.h>
#include "config.h"
#include "fingerprint.h"
#include "storage.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "indicators.h"
#include "sync.h"

// Global variables
int u = 0;
int v = 0;
int count = 0;

// Function prototypes
void showMainMenu();

void setup()
{
  Serial.begin(115200);
  delay(1000); // Allow serial to stabilize

  Serial.println("System initializing...");

  // Setup BLE
  setupBLE();

  // Initialize SPIFFS
  initSPIFFS();

  // Initialize fingerprint sensor
  initFingerprint();

  // Initialize RGB LED
  setupRGB();

  delay(2000);

  // Prompt user to select mode
  showMainMenu();
}

void showMainMenu()
{
  printBoth("\n=== Attendance System Menu ===");
  printBoth("1. Enroll Mode");
  printBoth("2. Attendance Mode");
  printBoth("3. Clear All Fingerprints");
  printBoth("4. View Stored Records");
  printBoth("5. Sync to Google Sheets");
  printBoth("6. Clear Attendance Data");
  printBoth("7. Set Current Date");
  printBoth("8. Update WiFi Settings");
  printBoth("9. Show Menu (Help)");
  printBoth("==============================");
}

void loop()
{
  // Handle BLE connectivity changes
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // Give the bluetooth stack time to catch up
    pServer->startAdvertising(); // Restart advertising
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected)
  {
    // Device just connected
    oldDeviceConnected = deviceConnected;
  }

  // Check for input from either Serial or BLE
  String mode = readInput();

  if (mode.length() > 0)
  {
    if (mode == "1")
    {
      enrollMode();
      showMainMenu();
    }
    else if (mode == "2")
    {
      attendanceMode();
      showMainMenu();
    }
    else if (mode == "3")
    {
      clearAllFingerprints();
      showMainMenu();
    }
    else if (mode == "4")
    {
      viewStoredRecords();
      showMainMenu();
    }
    else if (mode == "5")
    {
      printBoth("Syncing data to Google Sheets...");
      syncToGoogle();
      showMainMenu();
    }
    else if (mode == "6")
    {
      clearAttendanceData();
      showMainMenu();
    }
    else if (mode == "7")
    {
      setCurrentDate();
      showMainMenu();
    }
    else if (mode == "8")
    {
      updateWiFiSettings();
      showMainMenu();
    }
    else if (mode == "9" || mode == "?" || mode.equalsIgnoreCase("help"))
    {
      // Allow multiple inputs to trigger the help menu
      showMainMenu();
    }
    else
    {
      printBoth("Invalid choice. Please enter 1-7.");
    }
  }

  delay(20); // Short delay to avoid taxing the CPU
}