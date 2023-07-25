#include <Arduino.h>
#include "PinDefinitions.h"
#include "monitoringTask.h"
#include "log.h"
#include "lcdTask.h"
#include <esp_task_wdt.h>
#include <controlTask.h>
#include <config.h>

TaskHandle_t controlTaskHandle = NULL;

bool safetyErrorOccured = false;
bool controlTaskRunning = false;
bool waterPumpRunningTillEnd = false;

bool enteredMainLoop = false;
bool waitingForUserToResetSafetyError = false;

/*
True if pump is running and flow should be monitored for safety.
*/
bool pumpRunningMonitorFlow = false;

bool keepLogScreen = true;

//uint32_t compressorStartDelaySeconds = 10;
//uint32_t waterPumpStopDelaySeconds = 5;
//uint32_t waterPumpKeepRunningDelaySeconds = 30;
//uint32_t flowSwitchTimeoutSeconds = 10;

//float startTemperatureRequirement = 11.0f;
//float firstPairThresholdTemperature = 9.0f;
//float secondPairThresholdTemperature = 8.0f;

uint32_t ulNotificationValue;

static const char* TAG = "Control";

static portMUX_TYPE shutdownAll_spinlock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE controlTask_spinlock = portMUX_INITIALIZER_UNLOCKED;

// max 5000ms
bool checkForNotification(TickType_t msToWait)
{
    esp_task_wdt_reset();
    if(msToWait >= 5000)
    {
        assert(msToWait);
    }
    ulNotificationValue = ulTaskNotifyTake(pdTRUE, msToWait);
    if( ulNotificationValue == 1)
    {
        LOG_ERROR(TAG, "Error occured outside control task, aborting...");
        safetyErrorOccured = true;
        controlTaskRunning = false;
        pumpRunningMonitorFlow = false;
        enteredMainLoop = false;
        return true;
    }
    return false;
}

bool checkForNotificationSeconds(TickType_t secondsToWait)
{
    esp_task_wdt_reset();
    unsigned long lastTime = (unsigned long) (esp_timer_get_time() / 1000ULL);
    uint32_t res = 0;
    while((unsigned long) (esp_timer_get_time() / 1000ULL) - lastTime <= secondsToWait*1000)
    {
        res = ulTaskNotifyTake(pdTRUE, 1000);
        esp_task_wdt_reset();
        if(res > 0) break;
    }
    if( ulNotificationValue == 1)
    {
        LOG_ERROR(TAG, "Error occured outside control task, aborting...");
        safetyErrorOccured = true;
        controlTaskRunning = false;
        pumpRunningMonitorFlow = false;
        enteredMainLoop = false;
        return true;
    }
    return false;
}


void shutdownAll()
{
    LOG_VERBOSE(TAG, "Shutting down everything");
    taskENTER_CRITICAL(&shutdownAll_spinlock);
    pinMode(COMPRESSOR1_PIN, OUTPUT);
    pinMode(COMPRESSOR2_PIN, OUTPUT);
    pinMode(COMPRESSOR3_PIN, OUTPUT);
    pinMode(COMPRESSOR4_PIN, OUTPUT);
    pinMode(WATER_PUMP_PIN, OUTPUT);
    digitalWrite(COMPRESSOR1_PIN, LOW);
    digitalWrite(COMPRESSOR2_PIN, LOW);
    digitalWrite(COMPRESSOR3_PIN, LOW);
    digitalWrite(COMPRESSOR4_PIN, LOW);
    digitalWrite(WATER_PUMP_PIN, LOW);
    pumpRunningMonitorFlow = false;
    waterPumpRunningTillEnd = false;
    controlTaskRunning = false;
    enteredMainLoop = false;
    taskEXIT_CRITICAL(&shutdownAll_spinlock);
}

void controlTask(void * parameter)
{   
    LOG_INFO(TAG, "Initializing...");
    pinMode(COMPRESSOR1_PIN, OUTPUT);
    pinMode(COMPRESSOR2_PIN, OUTPUT);
    pinMode(COMPRESSOR3_PIN, OUTPUT);
    pinMode(COMPRESSOR4_PIN, OUTPUT);
    pinMode(WATER_PUMP_PIN, OUTPUT);
    //task started
    shutdownAll();
    while(!monitoringReady)
    {
        delay(1000);
        if(!criticalSensorsPresent)
        {
            LOG_ERROR(TAG, "Monitoring task failed to initialize. Aborting.");
            shutdownAll();
            vTaskSuspend(NULL);
        }
        else
        {
            if(!monitoringReady) LOG_WARNING(TAG, "Waiting for monitoring task to get ready.");
        }
    }
    LOG_INFO(TAG, "Initialization done.");
    keepLogScreen = false;
#if SWITCH_TO_MAIN_MENU_AFTER_BOOT
    taskENTER_CRITICAL(&controlTask_spinlock);
    currentLCDMenu = LCD_MENU_MAIN;
    taskEXIT_CRITICAL(&controlTask_spinlock);
#endif
    while(1)
    {
        unsubscribeTWDT(false);
        if(!monitoringReady)
        {
            LOG_ERROR(TAG, "Monitoring task not ready!");
            shutdownAll();
            delay(500);
            continue;
        }

        //check if main switch is on and water in temp over 11c for start condition
        if(!mainOnOffState ||
                             recirculatingPumpWaterInputTemperature <= start_temperature_requirement || 
                             waterPumpMotorProtectionState || criticalTemperatureFreezeProtection)
        {
            shutdownAll();
            pumpRunningMonitorFlow = false;
            controlTaskRunning = false;
            enteredMainLoop = false;
            LOG_VERBOSE(TAG, "Main switch off or temperature requirement not met");
            if(waterPumpMotorProtectionState || criticalTemperatureFreezeProtection || safetyErrorOccured)
            {
                LOG_ERROR(TAG, "Safety error occured or protection is active");
            }
            delay(1000);
            continue;
        }

        if(safetyErrorOccured)
        {
            shutdownAll();
            for(uint8_t time = 10; time > 0; time--)
            {
                LOG_ERROR(TAG, "Safety error occured, blocked for next %i seconds", time);
                shutdownAll();
                delay(1000);
            }
            while(mainOnOffState)
            {
                waitingForUserToResetSafetyError = true;
                LOG_WARNING(TAG, "Waiting for user to reset safety error by turning off main switch.");
                shutdownAll();
                delay(1000);
            }
            LOG_INFO(TAG, "Safety error reset successful");
            safetyErrorOccured = false;
            controlTaskRunning = false;
            enteredMainLoop = false;
            waitingForUserToResetSafetyError = false;
            continue;
        }
        
        LOG_INFO(TAG, "Starting...")

        if(esp_task_wdt_add(NULL) != ESP_OK)
        {
            LOG_ERROR(TAG, "Failed to attach control task to TWDT");
            delay(1000);
            continue;
        }
        LOG_INFO(TAG, "Task attached to TWDT.");
        controlTaskRunning = true;
        safetyErrorOccured = false;
        pumpRunningMonitorFlow = false;
        enteredMainLoop = false;
        // start ground water pump and check flow switch state every second, timeout 10seconds.
        digitalWrite(WATER_PUMP_PIN, HIGH);
        uint32_t timeout = flow_switch_timeout_seconds + 1;
        if(flow_switch_timeout_seconds == 61) 
        {
            timeout = 61;
            LOG_WARNING(TAG, "Flow switch timeout cant be longer than 60 seconds");
        }
        LOG_INFO(TAG, "Water pump started, waiting for flow switch, %i seconds timeout", timeout-1);
        while(!waterFlowSwitchState && timeout != 0)
        {
            if(--timeout == 0) 
            {
                shutdownAll();
                safetyErrorOccured = true;
                break;
            }
            else
            {
                LOG_WARNING(TAG, "Time out in %i seconds", timeout);
            }
            if(checkForNotification(1000)) break;
        }
        
        if(safetyErrorOccured) 
        {   
            LOG_ERROR(TAG, "Safety error occured! Aborting..");
            shutdownAll();
            continue;
        }

        LOG_INFO(TAG, "Flow switch is active. Waiting 1 more second for signal to stabilize.");
        if(checkForNotification(1000))
        {
            LOG_ERROR(TAG, "Error occured, aborting...");
            shutdownAll();
            continue;
        }

        if(!waterFlowSwitchState)
        {
            LOG_ERROR(TAG, "Flow switch not active! Aborting..");
            //error occured, pump on but no flow
            safetyErrorOccured = true;
            shutdownAll();
            continue;
        }

        pumpRunningMonitorFlow = true;

        LOG_INFO(TAG, "Starting compressor 1");
        digitalWrite(COMPRESSOR1_PIN, HIGH);
        for(uint32_t time = compressor_start_delay_seconds; time > 0; time--)
        {
            LOG_WARNING(TAG, "Starting compressor 3 in %i seconds", time);
            if(checkForNotification(1000)) 
            { 
                LOG_ERROR(TAG, "Error occured after starting compressor 1, aborting...");
                shutdownAll();
                break;
            }
        }
        if(safetyErrorOccured) continue;
        

        LOG_INFO(TAG, "Starting compressor 3");
        digitalWrite(COMPRESSOR3_PIN, HIGH);
        for(uint32_t time = compressor_start_delay_seconds; time > 0; time--)
        {
            LOG_WARNING(TAG, "Starting compressor 2 in %i seconds", time);
            if(checkForNotification(1000)) 
            { 
                LOG_ERROR(TAG, "Error occured after starting compressor 3, aborting...");
                shutdownAll();
                break;
            }
        }
        if(safetyErrorOccured) continue;
        
        LOG_INFO(TAG, "Starting compressor 2");
        digitalWrite(COMPRESSOR2_PIN, HIGH);
        for(uint32_t time = compressor_start_delay_seconds; time > 0; time--)
        {
            LOG_WARNING(TAG, "Starting compressor 4 in %i seconds", time);
            if(checkForNotification(1000)) 
            { 
                LOG_ERROR(TAG, "Error occured after starting compressor 2, aborting...");
                shutdownAll();
                break;
            }
        }
        if(safetyErrorOccured) continue;

        LOG_INFO(TAG, "Starting compressor 4");
        digitalWrite(COMPRESSOR4_PIN, HIGH);
        for(uint32_t time = compressor_start_delay_seconds; time > 0; time--)
        {
            LOG_WARNING(TAG, "Waiting %i seconds before entering main control loop", time);
            if(checkForNotification(1000)) 
            { 
                LOG_ERROR(TAG, "Error occured after starting compressor 4, aborting...");
                shutdownAll();
                break;
            }
        }
        if(safetyErrorOccured) continue;

        LOG_INFO(TAG, "Entering main control loop");
        while(!safetyErrorOccured)
        {
            enteredMainLoop = true;
            LOG_VERBOSE(TAG, "Running. Water temperature %.2f, Threshold 1 %.2f, Threshold 2 %.2f", recirculatingPumpWaterOutputTemperature, first_pair_shutdown_temp, first_pair_shutdown_temp + shutdown_temp_delta);
            if(!waterFlowSwitchState)
            {
                LOG_ERROR(TAG, "Flow switch not active, aborting...");
                safetyErrorOccured = true;
                break;
            }

            if(!mainOnOffState)
            {
                LOG_WARNING(TAG, "Shutting down before target temperature was reached");
                break;
            }

            if(recirculatingPumpWaterOutputTemperature <= first_pair_shutdown_temp)
            {
                LOG_INFO(TAG, "Shutting down compressor 1 and 3");
                digitalWrite(COMPRESSOR1_PIN, LOW);
                digitalWrite(COMPRESSOR3_PIN, LOW);
            }

            if(recirculatingPumpWaterOutputTemperature <= first_pair_shutdown_temp + shutdown_temp_delta)
            {
                LOG_INFO(TAG, "Shutting down compressor 2 and 4");
                digitalWrite(COMPRESSOR2_PIN, LOW);
                digitalWrite(COMPRESSOR4_PIN, LOW);
                break;
            }

            checkForNotification(200);

            if(esp_task_wdt_reset() != ESP_OK)
            {
                LOG_ERROR(TAG, "Failed to reset wdt in control loop");
            }
        }

        digitalWrite(COMPRESSOR1_PIN, LOW);
        digitalWrite(COMPRESSOR3_PIN, LOW);
        digitalWrite(COMPRESSOR2_PIN, LOW);
        digitalWrite(COMPRESSOR4_PIN, LOW);

        if(!safetyErrorOccured)
        {
            waterPumpRunningTillEnd = true;
            LOG_INFO(TAG, "Shutting down water pump in %i seconds", water_pump_keep_running_time_seconds);
            if(checkForNotificationSeconds(water_pump_keep_running_time_seconds))
            {
                LOG_ERROR(TAG, "Error occured, shutting down..");
                shutdownAll();
            }
        }
        else
        {
            LOG_ERROR(TAG, "Safety error occured, shutting down water pump..");
        }
        digitalWrite(WATER_PUMP_PIN, LOW); //turn off ground water pump after 30secs

        shutdownAll();      

        unsubscribeTWDT();
        pumpRunningMonitorFlow = false;
        controlTaskRunning = false;
        waterPumpRunningTillEnd = false;
        LOG_INFO(TAG, "Water pump and compressors are off.");
        if(!safetyErrorOccured) delay(water_pump_stop_delay_seconds*1000);
        LOG_INFO(TAG, "Waiting for start request");
        enteredMainLoop = false;
        delay(200);
    }
    vTaskDelete(NULL);
}

void unsubscribeTWDT(bool log)
{
    esp_task_wdt_reset();
    if((esp_task_wdt_delete(NULL) != ESP_OK) && log)
    {
        LOG_ERROR(TAG, "Failed to unsubscribe from TWDT");
    }
}