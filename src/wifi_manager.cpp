#include "wifi_manager.h"
#include "ble_manager.h"
#include "config.h"
#include <SPIFFS.h>

// Globals
String storedSSID = "";
String storedPassword = "";

void loadWiFiCredentials()
{
    if (SPIFFS.exists(WIFI_CONFIG_FILE))
    {
        File file = SPIFFS.open(WIFI_CONFIG_FILE, FILE_READ);
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
    File file = SPIFFS.open(WIFI_CONFIG_FILE, FILE_WRITE);
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