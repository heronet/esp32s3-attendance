#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>

// Globals
extern String currentDate;

// Function prototypes
void initSPIFFS();
void saveAttendanceToFile(String studentId);
void viewStoredRecords();
void clearAttendanceData();
void setCurrentDate();
void addAttendance(int fingerprintID);

#endif // STORAGE_H