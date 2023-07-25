#pragma once

#include <Arduino.h>
#include <DallasTemperature.h>
#include "DallasSensorID.h"

#define DALLAS_RESOLUTION_BIT 11

#define MONITORING_TASK_ERROR_LOG_INTERVAL 1000
#define MONITORING_TASK_WARNING_LOG_INTERVAL 5000

#define currentTimeUL() (unsigned long) (esp_timer_get_time() / 1000ULL)

struct
{
  int id;
  DeviceAddress addr;
  float lastTemp;
} TempSensor[TEMP_SENSOR_COUNT];

extern float heatExchanger1InputTemperature;
extern float heatExchanger1OutputTemperature;
extern float heatExchanger1Temperature;

extern float heatExchanger2InputTemperature;
extern float heatExchanger2OutputTemperature;
extern float heatExchanger2Temperature;

extern float recirculatingPumpWaterInputTemperature;
extern float recirculatingPumpWaterOutputTemperature;

extern bool compressor1State; //* True if compressor is running
extern bool compressor2State; //* True if compressor is running
extern bool compressor3State; //* True if compressor is running
extern bool compressor4State; //* True if compressor is running
extern bool waterPumpMotorState; //* True if motor is running
extern bool mainOnOffState; //* True if main switch is on
extern bool waterPumpMotorProtectionState; //* True if water pump motor DM protection is actiated
extern bool waterFlowSwitchState; //* True if water flow is present
extern bool highPressureProtectionActive; //* True if there is too high pressure in system
extern bool lowPressureProtectionActive; //* True if there is too low pressure in system for lowPressureTimeout or more @param lowPressureTimeout timeout in miliseconds
extern bool lowPressureSwitchActive; //* True if there is too low pressure in system

extern bool errorReadingSensors;
extern bool criticalTemperatureFreezeProtection;

extern bool monitoringReady;

//extern float freezeProtectionTemperature;

extern TaskHandle_t monitoringTaskHandle;

extern bool criticalSensorsPresent;

void monitoringTask(void * parameter);
bool sensorPresent(int id);
bool checkCriticalSensorTemp(float temp, int id);