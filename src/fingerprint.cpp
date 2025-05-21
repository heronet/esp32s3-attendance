#include "fingerprint.h"
#include "indicators.h"
#include "storage.h"
#include "ble_manager.h"
#include "config.h"

// Initialize the globals
HardwareSerial SerialX(1); // define a Serial for UART1
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&SerialX);

void initFingerprint()
{
    printBoth("Initializing sensor...");

    SerialX.begin(57600, SERIAL_8N1, SERIAL1RX, SERIAL1TX);

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
