#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"

// Globals
extern BLEServer *pServer;
extern BLECharacteristic *pTxCharacteristic;
extern BLECharacteristic *pRxCharacteristic;
extern bool deviceConnected;
extern bool oldDeviceConnected;
extern std::string receivedCommand;
extern String bleCommandBuffer;

// Class declarations
class ServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer);
    void onDisconnect(BLEServer *pServer);
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic);
};

// Function prototypes
void setupBLE();
void printBoth(String message);
String readInput();

#endif // BLE_MANAGER_H