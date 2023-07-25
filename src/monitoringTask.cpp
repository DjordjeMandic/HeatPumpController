#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "monitoringTask.h"
#include "DallasSensorID.h"
#include "PinDefinitions.h"
#include "controlTask.h"
#include "log.h"
#include <esp_task_wdt.h>
#include <ESP32Encoder.h>
#include <config.h>


TaskHandle_t monitoringTaskHandle = NULL;


bool compressor1State = false; //* True if compressor is running
bool compressor2State = false; //* True if compressor is running
bool compressor3State = false; //* True if compressor is running
bool compressor4State = false; //* True if compressor is running
bool waterPumpMotorState = false; //* True if motor is running
bool mainOnOffState = false; //* True if main switch is on
bool waterPumpMotorProtectionState = false; //* True if water pump motor DM protection is actiated
bool waterFlowSwitchState = false; //* True if water flow is present
bool highPressureProtectionActive = false; //* True if there is too high pressure in system
bool lowPressureProtectionActive = false; //* True if there is too low pressure in system for lowPressureTimeout or more @param lowPressureTimeout timeout in miliseconds
bool lowPressureSwitchActive = false; //* True if there is too low pressure in system




bool errorReadingSensors = false;
bool criticalTemperatureFreezeProtection = false;

bool monitoringReady = false;

//float freezeProtectionTemperature = 5.0f;

float heatExchanger1InputTemperature = DEVICE_DISCONNECTED_C;
float heatExchanger1OutputTemperature = DEVICE_DISCONNECTED_C;
float heatExchanger1Temperature = DEVICE_DISCONNECTED_C;

float heatExchanger2InputTemperature = DEVICE_DISCONNECTED_C;
float heatExchanger2OutputTemperature = DEVICE_DISCONNECTED_C;
float heatExchanger2Temperature = DEVICE_DISCONNECTED_C;

float recirculatingPumpWaterInputTemperature = DEVICE_DISCONNECTED_C;
float recirculatingPumpWaterOutputTemperature = DEVICE_DISCONNECTED_C;

unsigned long lastTempRequest = 0;
unsigned long lastWarnLogTime = 0;
unsigned long lastLowPressureResetTime = 0;
unsigned long lastLowPressureWarningTime = 0;
int delayInMillis = 0;
unsigned long lastErrorTime = 0;

//unsigned long lowPressureTimeout = 30000; //* Timeout to check for low pressure before triggering protection

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

static const char* TAG = "Monitor";

char sensorsFailed[(TEMP_SENSOR_COUNT)*(6+4)];
char *sensorsFailedPtr;

bool criticalSensorsPresent = true;

bool firstSensorRead = false;

static portMUX_TYPE currentScreenUpdate_spinlock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE digitalRead_spinlock = portMUX_INITIALIZER_UNLOCKED;

int64_t oldEncPos = 0;
int oldLCDMenu = BOOT_LCD_MENU;

extern bool param_config_entered;
extern uint8_t currentParamSelected;
extern bool editCurrentSelectedParameter;

extern volatile bool beep;

volatile int64_t lastEncoderPos;
volatile int64_t newEncoderPos;
static IRAM_ATTR void enc_cb(void* arg) {
    ESP32Encoder* enc = (ESP32Encoder*) arg;
    int64_t newPosition = enc->getCount() / 2;
    newEncoderPos = newPosition;
    taskENTER_CRITICAL_ISR(&currentScreenUpdate_spinlock);
    if(newPosition > lastEncoderPos)
    {
        if(param_config_entered && currentLCDMenu == LCD_MENU_PARAM)
        {
            if(!editCurrentSelectedParameter)
            {
                if(currentParamSelected == 9)
                {
                    currentParamSelected = 0;
                }
                else
                {
                    currentParamSelected++;
                }
            }
        }
        else 
        {
            param_config_entered = false;
            editCurrentSelectedParameter = false;
            if(currentLCDMenu == LCD_MENU_MAX-1)
            {
                currentLCDMenu = LCD_MENU_MIN+1;
            }
            else
            {
                currentLCDMenu++;
            }
        }
        //LOG_INFO(TAG, "Position: %i, Direction:    CW->", newPosition);
    }
    if(newPosition < lastEncoderPos)
    {
        if(param_config_entered && currentLCDMenu == LCD_MENU_PARAM)
        {
            if(!editCurrentSelectedParameter)
            {
                if(currentParamSelected == 0)
                {
                    currentParamSelected = 9;
                }
                else
                {
                    currentParamSelected--;
                }
            }
        }
        else 
        {
            param_config_entered = false;
            editCurrentSelectedParameter = false;
            if(currentLCDMenu == LCD_MENU_MIN+1)
            {
                currentLCDMenu = LCD_MENU_MAX-1;
                
            }
            else
            {
                currentLCDMenu--;
            }
        }
        //LOG_INFO(TAG, "Position: %i, Direction: <-CCW", newPosition);
    }
    taskEXIT_CRITICAL_ISR(&currentScreenUpdate_spinlock);
    lastEncoderPos = newPosition;
    beep = true;
    xTaskResumeFromISR(lcdTaskHandle);
}

ESP32Encoder encoder(true, enc_cb);


void monitoringTask(void * parameter)
{
    monitoringReady = false;
    LOG_INFO(TAG, "Starting...");
    //setup
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

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    pinMode(HIGH_PRESSURE_SWITCH_PIN, INPUT_PULLUP);
    pinMode(LOW_PRESSURE_SWITCH_PIN, INPUT_PULLUP);
    pinMode(WATER_PUMP_DM_PROTECTION_PIN, INPUT_PULLUP);

    LOG_INFO(TAG, "Initializing encoder counter");    
    ESP32Encoder::useInternalWeakPullResistors=UP;
    encoder.attachHalfQuad(ENCODER_DT_PIN, ENCODER_CLK_PIN);
    encoder.setCount(0);
    encoder.setFilter(1023);

    sensors.begin();
    sensors.setResolution(DALLAS_RESOLUTION_BIT);
    delayInMillis = 750 / (1 << (12 - DALLAS_RESOLUTION_BIT));
    LOG_INFO(TAG, "Sensor resolution %i", DALLAS_RESOLUTION_BIT);
    LOG_INFO(TAG, "Sensor conversion time %i ms", delayInMillis);
    
    for (uint8_t index = 0; index < TEMP_SENSOR_COUNT; index++)
    {
        TempSensor[index].id = ERROR_SENSOR_ID;
    }

    for (uint8_t index = 0; index < TEMP_SENSOR_COUNT; index++)
    {
        DeviceAddress addrs;
        sensors.getAddress(addrs, index);
        
        //sensors.setUserData(addrs, HEAT_EXCHANGER2_IN_SENSOR_ID);
        //continue;
        int id = sensors.getUserData(addrs); // if 0 failed to read
        if(id == 0)
        {
            LOG_ERROR(TAG, "Sensor #%u id is 0!", id);
        }
        bool duplicateFound = false;
        if(index > 0)
        for(uint8_t i = 0; i < index; i++)
        {
            if(TempSensor[i].id == id)
            {
                LOG_ERROR(TAG, "Sensor #%i has same id as sensor #%i with id %i, ignoring it", index, i, id);
                duplicateFound = true;
                break;
            }
        }
        if(duplicateFound) continue;
        sensors.getAddress(TempSensor[index].addr, index);
        TempSensor[index].id = id;
        LOG_INFO(TAG, "Got sensor #%i with id %i", index, TempSensor[index].id);
    }

    for (uint8_t index = 0; index < TEMP_SENSOR_COUNT; index++)
    {
        if(TempSensor[index].id <= ERROR_SENSOR_ID || TempSensor[index].id >= ERROR_SENSOR_ID_MAX)
        {
            LOG_ERROR(TAG, "Sensor #%i failed to respond", index);
        }
        else
        {
            LOG_INFO(TAG, "Sensor #%i with id %i OK", index, TempSensor[index].id);
        }
    }
    
    if(!sensorPresent(HEAT_EXCHANGER1_SENSOR_ID))
    {
        LOG_ERROR(TAG, "Heat Exchanger 1 sensor failed to respond. ID %i", HEAT_EXCHANGER1_SENSOR_ID);
        criticalSensorsPresent &= false;
    }
    if(!sensorPresent(HEAT_EXCHANGER2_SENSOR_ID))
    {
        LOG_ERROR(TAG, "Heat Exchanger 2 sensor failed to respond. ID %i", HEAT_EXCHANGER2_SENSOR_ID);
        criticalSensorsPresent &= false;
    }
    if(!sensorPresent(RECIRCULATING_PUMP_IN_SENSOR_ID))
    {
        LOG_ERROR(TAG, "Recirculating pump input sensor failed to respond. ID %i", RECIRCULATING_PUMP_IN_SENSOR_ID);
        criticalSensorsPresent &= false;
    }
    if(!sensorPresent(RECIRCULATING_PUMP_OUT_SENSOR_ID))
    {
        LOG_ERROR(TAG, "Recirculating pump output sensor failed to respond. ID %i", RECIRCULATING_PUMP_OUT_SENSOR_ID);
        criticalSensorsPresent &= false;
    }

    while(!criticalSensorsPresent)
    {
        LOG_ERROR(TAG, "Failed to initalize sensors!");
        delay(3000);
    }
    LOG_INFO(TAG, "Requesting temperatures - blocking");
    sensors.setWaitForConversion(true);
    sensors.requestTemperatures();
    lastTempRequest = (unsigned long) (esp_timer_get_time() / 1000ULL);
    sensors.setWaitForConversion(false);
    if(esp_task_wdt_add(NULL) != ESP_OK)
    {
        while(1)
        {
            LOG_ERROR(TAG, "Failed to attach control task to TWDT");
            delay(5000);
        }
    }
    LOG_INFO(TAG, "Task attached to TWDT.");

    lastLowPressureResetTime = lastLowPressureWarningTime = currentTimeUL();
    // uraditi proveru koji senzor je odkazao i strpaj sve to iznad u jedan metod i vrsi proveru i u samom loopu ispod i uraditi prenos info na control task
    while(1)
    {
        taskENTER_CRITICAL(&digitalRead_spinlock);
        compressor1State = digitalRead(COMPRESSOR1_PIN) == HIGH;
        compressor2State = digitalRead(COMPRESSOR2_PIN) == HIGH;
        compressor3State = digitalRead(COMPRESSOR3_PIN) == HIGH;
        compressor4State = digitalRead(COMPRESSOR4_PIN) == HIGH;
        waterPumpMotorState = digitalRead(WATER_PUMP_PIN) == HIGH;
        mainOnOffState = digitalRead(MAIN_ON_OFF_SWITCH_PIN) == main_switch_active_state;
        waterPumpMotorProtectionState = digitalRead(WATER_PUMP_DM_PROTECTION_PIN) == water_pump_dm_protection_active_state;
        waterFlowSwitchState = digitalRead(FLOW_SWITCH_PIN) == flow_switch_active_state;
        highPressureProtectionActive = digitalRead(HIGH_PRESSURE_SWITCH_PIN) == high_pressure_active_state;
        lowPressureSwitchActive = digitalRead(LOW_PRESSURE_SWITCH_PIN) == low_pressure_active_state;
        taskEXIT_CRITICAL(&digitalRead_spinlock);

        if(lowPressureSwitchActive)
        {
            lowPressureProtectionActive = (currentTimeUL() - lastLowPressureResetTime) >= (low_pressure_timeout_seconds*1000);
            if(currentTimeUL() - lastLowPressureWarningTime >= 1000)
            {
                LOG_WARNING(TAG, "Low pressure switch is active, timeout in %lu seconds", (low_pressure_timeout_seconds - ((currentTimeUL() - lastLowPressureResetTime) / 1000UL)))
                lastLowPressureWarningTime = currentTimeUL();
            }
        }
        else
        {
            lowPressureProtectionActive = false;
            lastLowPressureResetTime = currentTimeUL();
        }

        if (currentTimeUL() - lastTempRequest >= delayInMillis)
        {   
            errorReadingSensors = false;
            criticalTemperatureFreezeProtection = false;
            LOG_VERBOSE(TAG, "Reading sensors");
            sensorsFailedPtr = sensorsFailed;
            for(uint8_t index = 0; index < TEMP_SENSOR_COUNT; index++)
            {
                float tmp = sensors.getTempC(TempSensor[index].addr);
                TempSensor[index].lastTemp = tmp;
                //if one sensor failed to read, set error flag
                if(tmp == DEVICE_DISCONNECTED_C) 
                {
                    sensorsFailedPtr += sprintf(sensorsFailedPtr, "%d, ", TempSensor[index].id);
                    if((sensorsFailedPtr - sensorsFailed) >= sizeof(sensorsFailed)) 
                    {
                        sensorsFailedPtr = sensorsFailed;
                        sensorsFailed[sizeof(sensorsFailed)/sizeof(char)] = '\0';
                    }
                    errorReadingSensors |= true;
                }
                LOG_VERBOSE(TAG, "Sensor id %i temperature %f C", TempSensor[index].id, tmp);
                
                if(!checkCriticalSensorTemp(tmp, TempSensor[index].id))
                {
                    errorReadingSensors |= true;
                    //continue;
                }
                switch(TempSensor[index].id)
                {
                    case HEAT_EXCHANGER1_SENSOR_ID:
                        heatExchanger1Temperature = tmp;
                        criticalTemperatureFreezeProtection |= heatExchanger1Temperature < freeze_protection_temperature_limit;
                        break;
                    case HEAT_EXCHANGER2_SENSOR_ID:
                        heatExchanger2Temperature = tmp;
                        criticalTemperatureFreezeProtection |= heatExchanger2Temperature < freeze_protection_temperature_limit;
                        break;
                    case HEAT_EXCHANGER1_IN_SENSOR_ID:
                        heatExchanger1InputTemperature = tmp;
                        break;
                    case HEAT_EXCHANGER1_OUT_SENSOR_ID:
                        heatExchanger1OutputTemperature = tmp;
                        break;
                    case HEAT_EXCHANGER2_IN_SENSOR_ID:
                        heatExchanger2InputTemperature = tmp;
                        break;
                    case HEAT_EXCHANGER2_OUT_SENSOR_ID:
                        heatExchanger2OutputTemperature = tmp;
                        break;
                    case RECIRCULATING_PUMP_IN_SENSOR_ID:
                        recirculatingPumpWaterInputTemperature = tmp;
                        criticalTemperatureFreezeProtection |= recirculatingPumpWaterInputTemperature < freeze_protection_temperature_limit;
                        break;
                    case RECIRCULATING_PUMP_OUT_SENSOR_ID:
                        recirculatingPumpWaterOutputTemperature = tmp;
                        criticalTemperatureFreezeProtection |= recirculatingPumpWaterOutputTemperature < freeze_protection_temperature_limit;
                        break;
                    case ERROR_SENSOR_ID:
                        LOG_DEBUG(TAG, "Skipping reading of sensor #%i with id %i (failed to initialize)", index, TempSensor[index].id);
                        break;
                    default:
                        LOG_ERROR(TAG, "Unknown sensor #%i id %i", index, TempSensor[index].id);
                        errorReadingSensors |= true;
                }
            }
            if(errorReadingSensors) 
            {
                sensorsFailedPtr -= 2;
                *sensorsFailedPtr = '\0';
                LOG_ERROR(TAG, "Failed reading sensors with ids: %s", sensorsFailed);
            }
            LOG_VERBOSE(TAG, "Requesting temperatures - async");
            lastTempRequest = sensors.requestTemperatures().timestamp;
            LOG_VERBOSE(TAG, "Last temperature request timestamp %6u", lastTempRequest);
            firstSensorRead = true;
        }

        // if there is flow but water pump motor is not running warn user
        if(waterFlowSwitchState && (!waterPumpMotorState || waterPumpMotorProtectionState))
        {
            if((unsigned long) (esp_timer_get_time() / 1000ULL) - lastWarnLogTime >= MONITORING_TASK_WARNING_LOG_INTERVAL)
            {
                LOG_WARNING(TAG, "Flow swtich active but water pump is probbably not running");
                lastWarnLogTime = (unsigned long) (esp_timer_get_time() / 1000ULL);
            }
        }
        else
        {
            lastWarnLogTime = 0;
        }

        bool killControl = false;

        if(criticalTemperatureFreezeProtection) 
        {
            killControl |= true;
            if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                LOG_ERROR(TAG, "Freezing protection active.");
        }

        if(waterPumpMotorProtectionState) 
        {
            killControl |= true;
            if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                LOG_ERROR(TAG, "Water pump DM protection active.");
        }

        if(pumpRunningMonitorFlow && waterPumpMotorState && !waterFlowSwitchState)
        {
            killControl |= true;
            if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                LOG_ERROR(TAG, "Water pump flow protection active.");
        }

        if(controlTaskRunning & !waterPumpRunningTillEnd & !mainOnOffState & !enteredMainLoop)
        {
            killControl |= true;
            if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                LOG_ERROR(TAG, "Control task running with main switch off. Didnt enter main loop yet");
        } 

        if(highPressureProtectionActive)
        {
            killControl |= true;
            if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                LOG_ERROR(TAG, "High pressure protection active.")
        }

        if(lowPressureProtectionActive)
        {
            killControl |= true;
            if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                LOG_ERROR(TAG, "Low pressure protection active.")
        }

        if(killControl)
        {
            if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
            LOG_DEBUG(TAG, "Turning everything off and notifying control task.");
            digitalWrite(COMPRESSOR1_PIN, LOW);
            digitalWrite(COMPRESSOR2_PIN, LOW);
            digitalWrite(COMPRESSOR3_PIN, LOW);
            digitalWrite(COMPRESSOR4_PIN, LOW);
            digitalWrite(WATER_PUMP_PIN, LOW);
            //notify main task to end
            if(controlTaskHandle != NULL)
            {
                if(controlTaskRunning || pumpRunningMonitorFlow)
                {
                    if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                        LOG_DEBUG(TAG, "Control task is running.");
                    xTaskNotifyGive(controlTaskHandle);
                    
                    if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                        LOG_ERROR(TAG, "Control task notified.");
                }
                else
                {
                    if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                        LOG_ERROR(TAG, "Control task not running.");
                }
            }
            else
            {
                if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                    LOG_ERROR(TAG, "No handle to control task");
            }
            shutdownAll();
            if(currentTimeUL() - lastErrorTime >= MONITORING_TASK_ERROR_LOG_INTERVAL)
                lastErrorTime = currentTimeUL();
        }

        if(esp_task_wdt_reset() != ESP_OK)
        {
            LOG_ERROR(TAG, "Failed to reset wdt in monitoring loop");
        }
        
        if(currentLCDMenu != oldLCDMenu)
        {
            LOG_DEBUG(TAG, "Current LCD menu changed from %d to %d.", oldLCDMenu, currentLCDMenu);
            oldLCDMenu = currentLCDMenu;
        }

        if(oldEncPos != lastEncoderPos)
        {
            LOG_DEBUG(TAG, "Encoder position changed from %lld to %lld. Direction: %s", oldEncPos, lastEncoderPos, (oldEncPos > lastEncoderPos) ? "<-CCW" : "   CW->");
            oldEncPos = lastEncoderPos;
        }
        
        if(!monitoringReady && firstSensorRead)
        {
            LOG_INFO(TAG, "Task ready.")
        }
        
        monitoringReady |= firstSensorRead;
        //wait for lower priority task to finish
        uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, 100);
    }

    //should never get here
    vTaskDelete(NULL);
}


bool sensorPresent(int id)
{
    bool present = false;
    for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
    {
        if(TempSensor[i].id == id) present |= true;
    }
    return present;
}

bool checkCriticalSensorTemp(float temp, int id)
{
    if(temp == DEVICE_DISCONNECTED_C)
    {
        switch(id)
        {
            case HEAT_EXCHANGER1_SENSOR_ID:
                criticalTemperatureFreezeProtection |= true;
                criticalSensorsPresent &= false;
                LOG_DEBUG(TAG, "Failed to read temperature of heat exchanger 1 sensor with id %i", id);
                return false;
                break;
            case HEAT_EXCHANGER2_SENSOR_ID:
                criticalTemperatureFreezeProtection |= true;
                criticalSensorsPresent &= false;
                LOG_DEBUG(TAG, "Failed to read temperature of heat exchanger 2 sensor with id %i", id);
                return false;
                break;
            case RECIRCULATING_PUMP_IN_SENSOR_ID:
                criticalTemperatureFreezeProtection |= true;
                criticalSensorsPresent &= false;
                LOG_DEBUG(TAG, "Failed to read temperature of recirculating pump input sensor with id %i", id);
                return false;
                break;
            case RECIRCULATING_PUMP_OUT_SENSOR_ID:
                criticalTemperatureFreezeProtection |= true;
                criticalSensorsPresent &= false;
                LOG_DEBUG(TAG, "Failed to read temperature of recirculating pump output sensor with id %i", id);
                return false;
                break;
            default:
                LOG_DEBUG(TAG, "Failed to read temperature for sensor id %i", id);
                errorReadingSensors |= true;
        }
    }
    return true;
}