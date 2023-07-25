#pragma once

#include <Arduino.h>

extern TaskHandle_t controlTaskHandle;

extern bool safetyErrorOccured;
extern bool controlTaskRunning;
extern bool pumpRunningMonitorFlow;
extern bool waterPumpRunningTillEnd;
extern bool enteredMainLoop;
extern bool waitingForUserToResetSafetyError;

extern bool keepLogScreen;

//extern uint32_t compressorStartDelaySeconds;
//extern uint32_t waterPumpStopDelaySeconds;
//extern uint32_t waterPumpKeepRunningDelaySeconds;

//extern float startTemperatureRequirement;
//extern float firstPairThresholdTemperature;
//extern float secondPairThresholdTemperature;

void unsubscribeTWDT(bool log = true);
bool checkForNotification(TickType_t msToWait);
void controlTask(void * parameter);
void shutdownAll();