#include "ble_manager.h"
#include "config.h"
#include "indicators.h"

// Globals
BLEServer *pServer = nullptr;
BLECharacteristic *pTxCharacteristic = nullptr;
BLECharacteristic *pRxCharacteristic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;
std::string receivedCommand = "";
String bleCommandBuffer = "";

void ServerCallbacks::onConnect(BLEServer *pServer)
{
    deviceConnected = true;
    Serial.println("BLE Client connected");
}

void ServerCallbacks::onDisconnect(BLEServer *pServer)
{
    deviceConnected = false;
    Serial.println("BLE Client disconnected");

    // Start advertising again so client can reconnect
    pServer->getAdvertising()->start();
}

void CharacteristicCallbacks::onWrite(BLECharacteristic *pCharacteristic)
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

void setupBLE()
{
    // Initialize BLE device
    BLEDevice::init("ESP32-S3 Attendance");

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
