#include "storage.h"
#include "ble_manager.h"
#include "indicators.h"
#include "config.h"

// Globals
String currentDate = "19/5"; // Default date (today's date)

void initSPIFFS()
{
    if (!SPIFFS.begin(true))
    {
        printBoth("SPIFFS Mount Failed");
        return;
    }

    // Check if attendance file exists, if not create it with headers
    if (!SPIFFS.exists(ATTENDANCE_FILE_PATH))
    {
        File file = SPIFFS.open(ATTENDANCE_FILE_PATH, FILE_WRITE);
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

// Implementation Note:
// Move the following functions from main.cpp to storage.cpp:
void saveAttendanceToFile(String studentId)
{
    File file = SPIFFS.open(ATTENDANCE_FILE_PATH, FILE_APPEND);
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

void viewStoredRecords()
{
    File file = SPIFFS.open(ATTENDANCE_FILE_PATH, FILE_READ);
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
            if (SPIFFS.remove(ATTENDANCE_FILE_PATH))
            {
                // Create a new file with only the header
                File file = SPIFFS.open(ATTENDANCE_FILE_PATH, FILE_WRITE);
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
