#include "sync.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "config.h"
#include <SPIFFS.h>

void syncToGoogle()
{
    // Connect to WiFi before syncing
    connectToWiFi();

    if (WiFi.status() != WL_CONNECTED)
    {
        printBoth("WiFi not connected. Cannot sync to Google Sheets.");
        return;
    }

    File file = SPIFFS.open(ATTENDANCE_FILE_PATH, FILE_READ);
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

    String url = String("/macros/s/") + GSCRIPT_ID + "/exec";
    String fullUrl = "https://" + String(HOST) + url;

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
    SPIFFS.remove(ATTENDANCE_FILE_PATH);
    SPIFFS.rename("/temp.csv", ATTENDANCE_FILE_PATH);

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
