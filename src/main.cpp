#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "monitoringTask.h"
#include "controlTask.h"
#include "PinDefinitions.h"
#include "Free_Fonts.h"
#include <version.h>
#include <Images/power.h>
#include "log.h"
#include "lcdTask.h"
#include <ESP32Encoder.h>
#include <WifiMain.h>
#include <config.h>

static const char* TAG = "Main";

uint64_t encoderSW_press_cnt = 0;
bool encoderSW_pressed = false;

volatile bool beep = false;

void IRAM_ATTR encSWISR()
{
    if(currentLCDMenu == LCD_MENU_PARAM)
    {
        encoderSW_press_cnt++;
        encoderSW_pressed = true;
    }
    beep = true;
}

void setup(void)
{
    esp_log_level_set("*", (esp_log_level_t)CORE_DEBUG_LEVEL);
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
    pinMode(MAIN_ON_OFF_SWITCH_PIN, INPUT_PULLUP);
    pinMode(FLOW_SWITCH_PIN, INPUT_PULLUP);
    pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
    attachInterrupt(ENCODER_SW_PIN, encSWISR, FALLING);
    
    Serial.begin(baud);
    Serial.flush();
    LOG_INFO(TAG, "Booting...");

    tone(BUZZER_PIN, BUZZER_FREQUENCY_HZ, BUZZER_BUZZ_DELAY_MS);

    LOG_INFO(TAG, "Starting LCD task...");
    xTaskCreatePinnedToCore(
                    lcdTask,   /* Task function. */
                    "LCD",     /* name of task. */
                    4096,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &lcdTaskHandle,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */ 
    delay(1000);

    LOG_INFO(TAG, "Version: " VERSION);

    initConfig();

    
    Serial.flush();
    Serial.begin(baud);

    LOG_INFO(TAG, "Starting monitoring task...");
    xTaskCreatePinnedToCore(
                    monitoringTask,   /* Task function. */
                    "Monitor",     /* name of task. */
                    4096,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    2,           /* priority of the task */
                    &monitoringTaskHandle,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */ 


    LOG_INFO(TAG, "Starting contorl task...");
    xTaskCreatePinnedToCore(
                    controlTask,   /* Task function. */
                    "Control",     /* name of task. */
                    4096,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    2,           /* priority of the task */
                    &controlTaskHandle,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */ 
        
    startWifiAndServer();
}

// TODO probati sa prekidacima na inputu i led na output

void loop(void)
{
    wifiLoop();
    if(beep)
    {
        tone(BUZZER_PIN, BUZZER_FREQUENCY_HZ, BUZZER_BUZZ_DELAY_MS);
        beep = false;
    }
    //delay(1);
}