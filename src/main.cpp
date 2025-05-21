#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// WiFi credentials
const char *wifiConfigFile = "/wifi_config.txt";

String storedSSID = "";
String storedPassword = "";

// Google Script Deployment ID
const char *GScriptId = "AKfycby_2izhGidfcOPhpAfs7zhAWXHcK7oeZnUniauozbuc9rR52E7b_BaRJW4IgwTPPsz_rQ";

// Google Sheets setup
const char *host = "script.google.com";
const int httpsPort = 443;
String url = String("/macros/s/") + GScriptId + "/exec";

HardwareSerial SerialX(1); // define a Serial for UART1
const int Serial1RX = 17;
const int Serial1TX = 16;

// On ESP32, use Serial2 for hardware serial to fingerprint sensor
#define FINGERPRINT_SERIAL SerialX

// Create a fingerprint sensor object
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FINGERPRINT_SERIAL);

// Global variables
int u = 0;
int v = 0;
int count = 0;
String currentDate = "19/5"; // Default date (today's date)

// CSV file path in SPIFFS
const char *attendanceFilePath = "/attendance.csv";

// BLE server and service/characteristic definitions
BLEServer *pServer = nullptr;
BLECharacteristic *pTxCharacteristic = nullptr;
BLECharacteristic *pRxCharacteristic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;
std::string receivedCommand = "";

// UUIDs for BLE service and characteristics
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"           // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // RX Characteristic UUID
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // TX Characteristic UUID

// Command buffer for BLE commands
String bleCommandBuffer = "";

// NeoPixel LED settings
#define NEOPIXEL_PIN 48
#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Function prototypes
void initSPIFFS();
void syncToGoogle();
void saveAttendanceToFile(String studentId);
void showMainMenu();
void setupRGB();
void indicateSuccess();
void indicateFailure();
void clearAttendanceData();
void connectToWiFi();
void disconnectWiFi();
void setupBLE();
void printBoth(String message);

// BLE server callbacks to handle connections
class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    Serial.println("BLE Client connected");
  }

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    Serial.println("BLE Client disconnected");

    // Start advertising again so client can reconnect
    pServer->getAdvertising()->start();
  }
};

// BLE characteristic callbacks to handle received data
class CharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0)
    {
      Serial.println("*********");
      Serial.print("Received Value: ");

      // Convert received command to String and add to buffer
      for (int i = 0; i < rxValue.length(); i++)
      {
        Serial.print(rxValue[i]);
        bleCommandBuffer += rxValue[i];
      }
      Serial.println();

      // Check if the command is complete (ends with newline)
      if (bleCommandBuffer.endsWith("\n"))
      {
        // Remove newline character
        bleCommandBuffer.trim();
        receivedCommand = bleCommandBuffer.c_str();
        bleCommandBuffer = ""; // Clear buffer for next command
      }

      Serial.println("*********");
    }
  }
};

// Helper function to read input from Serial or BLE
String readInput()
{
  String input = "";

  bool blinkState = false;
  unsigned long lastBlinkTime = 0;

  // Indicate waiting for input with RGB LED pulse
  pixels.setPixelColor(0, pixels.Color(0, 0, 32)); // Soft blue to indicate waiting
  pixels.show();

  // Keep checking until we get input or timeout
  while (input.length() == 0)
  {
    // Check if there's a BLE command waiting
    if (receivedCommand.length() > 0)
    {
      input = String(receivedCommand.c_str());
      receivedCommand = ""; // Clear the received command
      break;
    }

    // Otherwise check Serial
    if (Serial.available())
    {
      input = Serial.readStringUntil('\n');
      input.trim();
      break;
    }

    // Handle BLE connection events
    if (!deviceConnected && oldDeviceConnected)
    {
      delay(100);                  // Give the bluetooth stack time to process
      pServer->startAdvertising(); // Restart advertising
      oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected)
    {
      oldDeviceConnected = deviceConnected;
    }

    // Pulse the LED while waiting to indicate system is ready for input
    if (millis() - lastBlinkTime > 500)
    {
      lastBlinkTime = millis();
      blinkState = !blinkState;
      if (blinkState)
      {
        pixels.setPixelColor(0, pixels.Color(0, 0, 48)); // Brighter blue
      }
      else
      {
        pixels.setPixelColor(0, pixels.Color(0, 0, 16)); // Dimmer blue
      }
      pixels.show();
    }

    delay(10); // Short delay to prevent CPU hogging
  }

  // Turn off the LED after input is received
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.show();

  return input;
}

// Helper function to print messages to both Serial and BLE
void printBoth(String message)
{
  Serial.println(message);

  // Send to BLE if connected
  if (deviceConnected && pTxCharacteristic != nullptr)
  {
    // BLE can only send chunks of up to 20 bytes, so we may need to split longer messages
    const int chunkSize = 20;
    int messageLength = message.length();

    for (int i = 0; i < messageLength; i += chunkSize)
    {
      int endIndex = min(i + chunkSize, messageLength);
      String chunk = message.substring(i, endIndex);
      pTxCharacteristic->setValue(chunk.c_str());
      pTxCharacteristic->notify();
      // delay(10); // Small delay to ensure proper transmission
    }

    // Send a newline to mark the end of the message
    pTxCharacteristic->setValue("\n");
    pTxCharacteristic->notify();
  }
}

uint8_t readnumber()
{
  uint8_t num = 0;

  while (num == 0)
  {
    String input = readInput();
    if (input.length() > 0)
    {
      num = input.toInt();

      // If conversion resulted in 0, check if it was actually "0"
      if (num == 0 && input != "0")
      {
        printBoth("Please enter a valid number");
      }
    }
  }
  return num;
}

void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    printBoth("SPIFFS Mount Failed");
    return;
  }

  // Check if attendance file exists, if not create it with headers
  if (!SPIFFS.exists(attendanceFilePath))
  {
    File file = SPIFFS.open(attendanceFilePath, FILE_WRITE);
    if (!file)
    {
      printBoth("Failed to create file");
      return;
    }
    // Write CSV headers
    file.println("date,student_id,status,synced");
    file.close();
    printBoth("Created attendance file with headers");
  }
  else
  {
    printBoth("Attendance file exists");
  }
}

void saveAttendanceToFile(String studentId)
{
  File file = SPIFFS.open(attendanceFilePath, FILE_APPEND);
  if (!file)
  {
    printBoth("Failed to open file for appending");
    return;
  }

  // Format: date,student_id,status,synced
  String record = currentDate + "," + studentId + ",present,0";
  file.println(record);
  file.close();

  printBoth("Saved attendance record to file: " + record);
}

// New function to load WiFi credentials from SPIFFS
void loadWiFiCredentials()
{
  if (SPIFFS.exists(wifiConfigFile))
  {
    File file = SPIFFS.open(wifiConfigFile, FILE_READ);
    if (file)
    {
      String ssidFromFile = file.readStringUntil('\n');
      String passwordFromFile = file.readStringUntil('\n');

      // Trim any newlines or whitespace
      ssidFromFile.trim();
      passwordFromFile.trim();

      // Only update if not empty
      if (ssidFromFile.length() > 0)
      {
        storedSSID = ssidFromFile;
      }
      if (passwordFromFile.length() > 0)
      {
        storedPassword = passwordFromFile;
      }

      file.close();
      printBoth("WiFi credentials loaded: " + storedSSID);
    }
  }
  else
  {
    printBoth("No saved WiFi credentials found");
  }
}

// New function to save WiFi credentials to SPIFFS
void saveWiFiCredentials(const String &newSSID, const String &newPassword)
{
  File file = SPIFFS.open(wifiConfigFile, FILE_WRITE);
  if (file)
  {
    file.println(newSSID);
    file.println(newPassword);
    file.close();
    printBoth("WiFi credentials saved successfully");
  }
  else
  {
    printBoth("Failed to save WiFi credentials");
  }
}

// New function to update WiFi settings
void updateWiFiSettings()
{
  printBoth("Current SSID: " + storedSSID);
  printBoth("Enter new SSID (or leave empty to keep current):");

  String newSSID = readInput();
  if (newSSID.length() > 0)
  {
    printBoth("Enter password for " + newSSID + ":");
    String newPassword = readInput();

    // Update stored variables
    storedSSID = newSSID;
    storedPassword = newPassword;

    // Save to file
    saveWiFiCredentials(storedSSID, storedPassword);
    printBoth("WiFi settings updated");
  }
  else
  {
    printBoth("SSID unchanged");
  }
}

// Modified connectToWiFi function
void connectToWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    printBoth("WiFi already connected!");
    return;
  }

  // Load saved credentials only when needed
  loadWiFiCredentials();

  // If no credentials are available, prompt user
  if (storedSSID.length() == 0)
  {
    printBoth("No WiFi credentials found. Please set them now:");
    updateWiFiSettings();
  }

  printBoth("Connecting to " + storedSSID + " ...");

  WiFi.begin(storedSSID.c_str(), storedPassword.c_str());

  int wifiCounter = 0;
  while (WiFi.status() != WL_CONNECTED && wifiCounter < 20) // Timeout after 20 seconds
  {
    delay(1000);
    printBoth(".");
    wifiCounter++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    printBoth("\nConnection established!");
    printBoth("IP address: " + WiFi.localIP().toString());
  }
  else
  {
    printBoth("\nWiFi connection failed! Do you want to update WiFi settings? (Y/N)");
    String response = readInput();
    if (response == "Y" || response == "y")
    {
      updateWiFiSettings();
      // Try connecting once more with new settings
      printBoth("Trying again with new settings...");
      WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
      wifiCounter = 0;
      while (WiFi.status() != WL_CONNECTED && wifiCounter < 20)
      {
        delay(1000);
        printBoth(".");
        wifiCounter++;
      }

      if (WiFi.status() == WL_CONNECTED)
      {
        printBoth("\nConnection established!");
        printBoth("IP address: " + WiFi.localIP().toString());
      }
      else
      {
        printBoth("\nWiFi connection still failed. Cannot sync to Google Sheets.");
      }
    }
  }
}

void disconnectWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    printBoth("Disconnecting from WiFi...");
    WiFi.disconnect();
    printBoth("WiFi disconnected");
  }
}

// Function to sync attendance data to Google Sheets; Low RAM version
void syncToGoogle()
{
  // Connect to WiFi before syncing
  connectToWiFi();

  if (WiFi.status() != WL_CONNECTED)
  {
    printBoth("WiFi not connected. Cannot sync to Google Sheets.");
    return;
  }

  File file = SPIFFS.open(attendanceFilePath, FILE_READ);
  if (!file)
  {
    printBoth("Failed to open file for reading");
    disconnectWiFi();
    return;
  }

  // Read the header line and discard
  String header = file.readStringUntil('\n');

  // Temporary file to store synced records
  File tempFile = SPIFFS.open("/temp.csv", FILE_WRITE);
  if (!tempFile)
  {
    printBoth("Failed to create temp file");
    file.close();
    disconnectWiFi();
    return;
  }

  // Write the header to temp file
  tempFile.println(header);

  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate validation

  // Increase timeout values for client
  client.setTimeout(20000); // 20 seconds timeout

  HTTPClient http;
  // Increase timeout values for HTTP client
  http.setTimeout(20000);

  String fullUrl = "https://" + String(host) + url;

  // Build JSON array of records to sync
  String jsonPayload = "{\"command\": \"batch_attendance\", \"sheet_name\": \"Attendance\", \"records\": [";

  int recordCount = 0;
  bool hasUnsyncedRecords = false;

  // First pass: count unsynchronized records and build JSON array
  while (file.available())
  {
    String line = file.readStringUntil('\n');
    if (line.length() <= 1)
      continue; // Skip empty lines

    // Parse the CSV line
    int commaPos1 = line.indexOf(',');
    int commaPos2 = line.indexOf(',', commaPos1 + 1);
    int commaPos3 = line.indexOf(',', commaPos2 + 1);

    String date = line.substring(0, commaPos1);
    String studentId = line.substring(commaPos1 + 1, commaPos2);
    String status = line.substring(commaPos2 + 1, commaPos3);
    String synced = line.substring(commaPos3 + 1);

    // Only include records that haven't been synced yet
    if (synced.toInt() == 0)
    {
      // Add comma if not the first record
      if (hasUnsyncedRecords)
      {
        jsonPayload += ",";
      }

      // Add this record to the JSON array
      jsonPayload += "{\"date\":\"" + date + "\",\"student_id\":\"" + studentId + "\",\"status\":\"" + status + "\"}";

      hasUnsyncedRecords = true;
      recordCount++;
    }
  }

  // Close the JSON array and object
  jsonPayload += "]}";

  // Reset file position to beginning (after header)
  file.seek(0);
  file.readStringUntil('\n'); // Skip header

  // If no records to sync, just report and exit
  if (!hasUnsyncedRecords)
  {
    printBoth("No unsynced records found. Nothing to upload.");
    file.close();
    tempFile.close();
    disconnectWiFi();
    return;
  }

  printBoth("Publishing " + String(recordCount) + " attendance records to Google Sheets...");
  printBoth("Payload size: " + String(jsonPayload.length()) + " bytes");

  // Send the batch request
  http.begin(client, fullUrl);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(jsonPayload);

  bool syncSuccessful = false;

  // Handle response
  if (httpResponseCode > 0)
  {
    String response = http.getString();
    printBoth("HTTP Response code: " + String(httpResponseCode));
    printBoth("Response: " + response);
    syncSuccessful = true;
  }
  // Check for specific negative error codes that might still indicate success
  else if (httpResponseCode == -11)
  {
    printBoth("Response timeout but data likely sent. HTTP Response code: " + String(httpResponseCode));
    // Optimistically assume data was sent
    syncSuccessful = true;
  }
  else
  {
    printBoth("Error publishing data. HTTP Response code: " + String(httpResponseCode));
    syncSuccessful = false;
  }

  http.end();

  // Now update the local records based on sync result
  while (file.available())
  {
    String line = file.readStringUntil('\n');
    if (line.length() <= 1)
    {
      continue; // Skip empty lines
    }

    // Parse the CSV line to check sync status
    int lastCommaPos = line.lastIndexOf(',');
    String syncStatus = line.substring(lastCommaPos + 1);

    if (syncStatus.toInt() == 0 && syncSuccessful)
    {
      // Mark as synced by replacing the last 0 with 1
      String updatedLine = line.substring(0, lastCommaPos + 1) + "1";
      tempFile.println(updatedLine);
    }
    else
    {
      // Keep line as is
      tempFile.println(line);
    }
  }

  file.close();
  tempFile.close();

  // Replace the original file with the temp file
  SPIFFS.remove(attendanceFilePath);
  SPIFFS.rename("/temp.csv", attendanceFilePath);

  if (syncSuccessful)
  {
    printBoth("Sync completed successfully. " + String(recordCount) + " records synced.");
  }
  else
  {
    printBoth("Sync failed. Will try again later.");
  }

  // Disconnect from WiFi after syncing
  disconnectWiFi();
}

uint8_t getFingerprintEnroll(uint8_t id)
{
  int p = -1;
  printBoth("Waiting for valid finger to enroll as #" + String(id));
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      printBoth("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      printBoth(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      printBoth("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      printBoth("Imaging error");
      break;
    default:
      printBoth("Unknown error");
      break;
    }

    if (p == FINGERPRINT_OK)
    {
      printBoth("Stored!");
      indicateSuccess(); // Success indicator
    }
    else
    {
      // In failure cases
      indicateFailure(); // Failure indicator
    }
  }

  p = finger.image2Tz(1);
  switch (p)
  {
  case FINGERPRINT_OK:
    printBoth("Image converted");
    break;
  case FINGERPRINT_IMAGEMESS:
    printBoth("Image too messy");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    printBoth("Communication error");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    printBoth("Could not find fingerprint features");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    printBoth("Could not find fingerprint features");
    return p;
  default:
    printBoth("Unknown error");
    return p;
  }

  printBoth("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
  {
    p = finger.getImage();
  }

  printBoth("Place same finger again");
  p = -1;
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      printBoth("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      printBoth(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      printBoth("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      printBoth("Imaging error");
      break;
    default:
      printBoth("Unknown error");
      break;
    }
  }

  p = finger.image2Tz(2);
  switch (p)
  {
  case FINGERPRINT_OK:
    printBoth("Image converted");
    break;
  case FINGERPRINT_IMAGEMESS:
    printBoth("Image too messy");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    printBoth("Communication error");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    printBoth("Could not find fingerprint features");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    printBoth("Could not find fingerprint features");
    return p;
  default:
    printBoth("Unknown error");
    return p;
  }

  p = finger.createModel();
  if (p == FINGERPRINT_OK)
  {
    printBoth("Prints matched!");
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    printBoth("Communication error");
    return p;
  }
  else if (p == FINGERPRINT_ENROLLMISMATCH)
  {
    printBoth("Fingerprints did not match");
    return p;
  }
  else
  {
    printBoth("Unknown error");
    return p;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK)
  {
    printBoth("Stored!");
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    printBoth("Communication error");
    return p;
  }
  else if (p == FINGERPRINT_BADLOCATION)
  {
    printBoth("Could not store in that location");
    return p;
  }
  else if (p == FINGERPRINT_FLASHERR)
  {
    printBoth("Error writing to flash");
    return p;
  }
  else
  {
    printBoth("Unknown error");
    return p;
  }

  return true;
}

void enrollFingerprint()
{
  printBoth("Ready to enroll a fingerprint!");
  printBoth("Please type in the ID # (from 1 to 127) you want to save this finger as...");
  uint8_t id = readnumber();
  if (id == 0)
  { // ID #0 not allowed
    return;
  }
  printBoth("Enrolling ID #" + String(id));

  while (!getFingerprintEnroll(id))
    ;
}

int getFingerprintID()
{
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)
    return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
    return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)
  {
    // LED failure indication
    indicateFailure();
    return -1;
  }

  printBoth("Found ID #" + String(finger.fingerID) + " with confidence of " + String(finger.confidence));
  return finger.fingerID;
}

// Function to add attendance
void addAttendance(int fingerprintID)
{
  String studentId;

  if (fingerprintID)
  {
    printBoth("Welcome " + String(fingerprintID));
    studentId = String(fingerprintID);
  }
  else
  {
    printBoth("Unknown fingerprint ID");
    return;
  }

  // Save attendance to local file (passing only studentId)
  saveAttendanceToFile(studentId);

  // LED success indication
  indicateSuccess();
}

void viewStoredRecords()
{
  File file = SPIFFS.open(attendanceFilePath, FILE_READ);
  if (!file)
  {
    printBoth("Failed to open attendance file");
    return;
  }

  printBoth("\n--- Stored Attendance Records ---");

  // Read and print all lines
  while (file.available())
  {
    String line = file.readStringUntil('\n');
    printBoth(line);
  }

  file.close();
  printBoth("--- End of Records ---\n");
}

// Function to clear attendance data
void clearAttendanceData()
{
  printBoth("Are you sure you want to clear all attendance records? (Y/N)");

  String confirmation = readInput();
  if (confirmation == "Y" || confirmation == "y")
  {
    // Double confirmation for safety
    printBoth("ALL ATTENDANCE RECORDS WILL BE PERMANENTLY DELETED!");
    printBoth("Type 'CONFIRM' to proceed:");

    String finalConfirmation = readInput();
    if (finalConfirmation == "CONFIRM")
    {
      // Delete the old file
      if (SPIFFS.remove(attendanceFilePath))
      {
        // Create a new file with only the header
        File file = SPIFFS.open(attendanceFilePath, FILE_WRITE);
        if (file)
        {
          // Write CSV headers
          file.println("date,student_id,status,synced");
          file.close();

          printBoth("All attendance records have been cleared successfully!");
          indicateSuccess(); // Visual confirmation
        }
        else
        {
          printBoth("Error: Failed to create a new attendance file");
          indicateFailure();
        }
      }
      else
      {
        printBoth("Error: Failed to remove the old attendance file");
        indicateFailure();
      }
    }
    else
    {
      printBoth("Operation canceled: Confirmation text didn't match");
    }
  }
  else
  {
    printBoth("Operation canceled");
  }

  delay(2000);
}

void enrollMode()
{
  printBoth("Entering Enroll Mode...");
  printBoth("Follow instructions on serial monitor");

  while (true)
  {
    enrollFingerprint();

    printBoth("\nEnrollment options:");
    printBoth("1. Enroll another fingerprint");
    printBoth("2. Return to main menu");

    String option = readInput();
    if (option == "2")
    {
      break;
    }
  }
}

void setCurrentDate()
{
  printBoth("Enter today's date in DD/MM format (e.g., 19/5):");

  // Wait for user to input a date
  String dateInput = readInput(); // Will wait for input by default

  // Check if we got a valid input (non-empty after timeout)
  if (dateInput.length() > 0)
  {
    // Optional: add a way to cancel and keep current date
    if (dateInput == "x" || dateInput == "X")
    {
      printBoth("Date change canceled. Keeping current date: " + currentDate);
      return;
    }

    currentDate = dateInput;
    printBoth("Date set to: " + currentDate);
  }
  else
  {
    printBoth("No date entered. Keeping current date: " + currentDate);
  }
}

void attendanceMode()
{
  // First set the date for attendance
  setCurrentDate();

  printBoth("Entering Attendance Mode for date: " + currentDate);
  printBoth("Place Finger... (Press 'X' to exit)");

  while (true)
  {
    // Check for exit command first (without blocking)
    if (Serial.available() || receivedCommand.length() > 0)
    {
      String cmd = "";

      if (receivedCommand.length() > 0)
      {
        cmd = String(receivedCommand.c_str());
        receivedCommand = "";
      }
      else
      {
        cmd = Serial.readStringUntil('\n');
        cmd.trim();
      }

      if (cmd == "x" || cmd == "X")
      {
        printBoth("Exiting Attendance Mode...");
        return;
      }
    }

    // Wait for a fingerprint to be detected
    int fingerprintID = getFingerprintID();

    if (fingerprintID > 0)
    {
      // Fingerprint found, add attendance
      addAttendance(fingerprintID);
      delay(2000); // Delay before next scan
      printBoth("Place Finger... (Press 'X' to exit)");
    }

    // Handle BLE connection states
    if (!deviceConnected && oldDeviceConnected)
    {
      delay(500);
      pServer->startAdvertising();
      oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected)
    {
      oldDeviceConnected = deviceConnected;
    }

    delay(100); // Small delay to avoid spamming the sensor
  }
}

void clearAllFingerprints()
{
  printBoth("Are you sure you want to clear all fingerprints? (Y/N)");

  String confirmation = readInput();
  if (confirmation == "Y" || confirmation == "y")
  {
    printBoth("Clearing all fingerprints...");

    uint8_t p = finger.emptyDatabase();
    if (p == FINGERPRINT_OK)
    {
      printBoth("All fingerprints cleared successfully!");
    }
    else
    {
      printBoth("Failed to clear fingerprints.");
    }
  }
  else
  {
    printBoth("Clear operation canceled.");
  }
  delay(2000);
}

void setupRGB()
{
  // Initialize NeoPixel
  pixels.begin();
  pixels.setBrightness(50); // Set brightness (0-255)

  // Quick test flash - red then green
  pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
  pixels.show();
  delay(300);
  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
  pixels.show();
  delay(300);
  pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Turn off
  pixels.show();

  printBoth("NeoPixel LED initialized");
}

void indicateSuccess()
{
  pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
  pixels.show();
  delay(1000);
  pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Turn off
  pixels.show();
}

void indicateFailure()
{
  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
  pixels.show();
  delay(1000);
  pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Turn off
  pixels.show();
}

void setupBLE()
{
  // Initialize BLE device
  BLEDevice::init("ESP32S3-Attendance");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristics
  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_TX,
      BLECharacteristic::PROPERTY_NOTIFY);

  pTxCharacteristic->addDescriptor(new BLE2902());

  pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX,
      BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(new CharacteristicCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();

  Serial.println("BLE device initialized. Waiting for client connections...");
}

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
  printBoth("Initializing sensor...");

  FINGERPRINT_SERIAL.begin(57600, SERIAL_8N1, Serial1RX, Serial1TX);

  finger.begin(57600);
  if (finger.verifyPassword())
  {
    printBoth("Found fingerprint sensor!");
  }
  else
  {
    printBoth("Did not find fingerprint sensor :(");
    while (1)
    {
      delay(1000);
    }
  }

  setupRGB();

  finger.getTemplateCount();
  printBoth("Stored Prints: " + String(finger.templateCount));

  if (finger.templateCount == 0)
  {
    printBoth("Sensor doesn't contain any fingerprint data. Please enroll a fingerprint.");
  }
  else
  {
    printBoth("Sensor contains " + String(finger.templateCount) + " templates");
  }
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