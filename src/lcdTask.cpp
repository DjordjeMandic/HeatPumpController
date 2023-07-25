#include <Arduino.h>
#include "lcdTask.h"
#include "TFT_eSPI.h"
#include "log.h"
#include "version.h"
#include <esp_log.h>
#include <PinDefinitions.h>
#include <Images\power.h>
#include <Images\sync.h>
#include <Images\stop.h>
#include <Images\check.h>
#include <controlTask.h>
#include <monitoringTask.h>
#include <Images\temperature.h>
#include <Free_Fonts.h>
#include <ESP32Encoder.h>
#include <Images/motorWaterPump.h>
#include <Images/pressure.h>
#include <Images/compressor.h>
#include <FreeSansBold9pt7bDegree.h>
#include <WiFi.h>
#include <rom/rtc.h>
#include <WifiMain.h>
#include <stdio.h>
#include "rom/ets_sys.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include <config.h>

TaskHandle_t lcdTaskHandle = NULL;

bool lcdLogReady = false;
volatile int currentLCDMenu = LCD_MENU_LOG;
static const char* TAG = "LCD";

bool firstLog = false;

bool param_config_entered = false;

uint64_t errorCount = 0;

int64_t oldErrorDisplayTime;

TFT_eSPI tft = TFT_eSPI(); 
TFT_eSprite iconSprite32 = TFT_eSprite(&tft);
TFT_eSprite mainScreenTempSprite = TFT_eSprite(&tft);
TFT_eSprite mainScreenTemp2Sprite = TFT_eSprite(&tft);
TFT_eSprite aboutScreenSprite = TFT_eSprite(&tft);
TFT_eSprite paramScreenSprite = TFT_eSprite(&tft);

int blank[19];
char msg[4096];
char *p_msg = msg;
uint32_t lastlogFGColor;
uint32_t lastlogBGColor;

// Keep track of the drawing x coordinate
uint16_t xPos = 0;

char buffUptime[64];

char buffMain[64];

// The scrolling area must be a integral multiple of TEXT_HEIGHT
#define TEXT_HEIGHT 16 // Height of text to be printed and scrolled
#define BOT_FIXED_AREA 16 // Number of lines in bottom fixed area (lines counted from bottom of screen)
#define TOP_FIXED_AREA 16 // Number of lines in top fixed area (lines counted from top of screen)
#define YMAX 320 // Bottom of screen area

// The initial y coordinate of the top of the scrolling area
uint16_t yStart = TOP_FIXED_AREA;
// yArea must be a integral multiple of TEXT_HEIGHT
uint16_t yArea = YMAX-TOP_FIXED_AREA-BOT_FIXED_AREA;
// The initial y coordinate of the top of the bottom text line
uint16_t yDraw = YMAX - BOT_FIXED_AREA - TEXT_HEIGHT;

unsigned long lastForceUpdate = 0;

float heatExchanger1InputTemperatureLocal;
float heatExchanger1OutputTemperatureLocal;
float heatExchanger1TemperatureLocal;
float heatExchanger2InputTemperatureLocal;
float heatExchanger2OutputTemperatureLocal;
float heatExchanger2TemperatureLocal;
float recirculatingPumpWaterInputTemperatureLocal;
float recirculatingPumpWaterOutputTemperatureLocal;


static portMUX_TYPE temperatureRead_spinlock = portMUX_INITIALIZER_UNLOCKED;


extern uint64_t encoderSW_press_cnt;
uint64_t encoderSW_press_cnt_old = 0;
uint8_t currentParamSelected = 0;
uint8_t currentParamSelectedOld = 0;
extern bool encoderSW_pressed;
bool editCurrentSelectedParameter = false;

extern int64_t lastEncoderPos;
int64_t oldEncPosition = 0;


extern uint32_t low_pressure_timeout_seconds;
extern uint32_t water_pump_stop_delay_seconds;
extern uint32_t water_pump_keep_running_time_seconds;
extern uint32_t compressor_start_delay_seconds;
extern uint32_t flow_switch_timeout_seconds;
extern float freeze_protection_temperature_limit;
extern float start_temperature_requirement;
extern float first_pair_shutdown_temp;
extern float shutdown_temp_delta;

float secondPairOffTemp;

float temperatureReadFixed()
{
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR, 3, SENS_FORCE_XPD_SAR_S);
    SET_PERI_REG_BITS(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_CLK_DIV, 10, SENS_TSENS_CLK_DIV_S);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_DUMP_OUT);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP_FORCE);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP);
    ets_delay_us(100);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_DUMP_OUT);
    ets_delay_us(5);
    float temp_f = (float)GET_PERI_REG_BITS2(SENS_SAR_SLAVE_ADDR3_REG, SENS_TSENS_OUT, SENS_TSENS_OUT_S);
    float temp_c = (temp_f - 32) / 1.8;
    return temp_c;
}

void lcdTask(void * parameter)
{
    lastForceUpdate = currentTimeUL();
    errorCount = 0;
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    LOG_INFO(TAG, "LCD initialized");
    iconSprite32.setColorDepth(1);
    iconSprite32.createSprite(32,32);
    mainScreenTempSprite.setColorDepth(1);
    mainScreenTempSprite.createSprite(139, 18);
    mainScreenTemp2Sprite.createSprite(240-(5*32),32);
    while(1)
    {
        lcdLogReady = false;
        if(currentLCDMenu == LCD_MENU_MAIN)
        {
            LOG_INFO(TAG, "Switching to main menu");
            switchToMainMenu();
        }
        else if(currentLCDMenu == LCD_MENU_PARAM)
        {
            LOG_INFO(TAG, "Switching to param menu");
            switchToParamMenu();
        }
        else if(currentLCDMenu == LCD_MENU_LOG)
        {
            LOG_INFO(TAG, "Switching to log console");
            switchToLogMenu();
            continue;
        }
        else if(currentLCDMenu == LCD_MENU_ABOUT)
        {
            LOG_INFO(TAG, "Switching to about menu");
            switchToAboutMenu();
        }
        else
        {
            LOG_ERROR(TAG, "Unknown display menu option");
            tft.fillScreen(TFT_BLACK);
        }
    }

    vTaskDelete(NULL); //should never get here
}

void switchToParamMenu()
{
    disableScrollArea();
    param_config_entered = false;
    tft.fillScreen(LCD_PARAM_MENU_BG_COLOR);
    tft.fillRect(0,0,240,16, LCD_PARAM_MENU_TOP_BG_COLOR);
    tft.fillRect(0,320-16,240,16, LCD_PARAM_MENU_BOT_BG_COLOR);
    tft.setTextColor(TFT_WHITE, LCD_PARAM_MENU_TOP_BG_COLOR);
    paramScreenSprite.setColorDepth(1);
    paramScreenSprite.createSprite(240, 18);
    tft.drawCentreString("Parameters",120,0,2);
    encoderSW_press_cnt_old = encoderSW_press_cnt;
    oldEncPosition = lastEncoderPos;
    tft.fillRect(0,16,240,320-32, TFT_BLACK);
    secondPairOffTemp = first_pair_shutdown_temp + shutdown_temp_delta;
    while(currentLCDMenu == LCD_MENU_PARAM)
    {
        if(digitalRead(ENCODER_SW_PIN) != LOW)
        {
            if(param_config_entered && currentParamSelected != 0 && encoderSW_pressed)
            {
                delay(100);
                encoderSW_press_cnt_old = encoderSW_press_cnt;
                encoderSW_pressed = false;
                editCurrentSelectedParameter = !editCurrentSelectedParameter;
            }
            else if(!param_config_entered && encoderSW_pressed)
            {
                delay(100);
                encoderSW_press_cnt_old = encoderSW_press_cnt;
                encoderSW_pressed = false;
                currentParamSelected = 0;
                param_config_entered = true;
                tft.fillRect(0,320-50,240,30, LCD_PARAM_MENU_BG_COLOR);
            }
            else if(currentParamSelected != 0 && encoderSW_pressed)
            { 
                encoderSW_pressed = false;
            }
            else if(param_config_entered && (currentParamSelected == 0) && encoderSW_pressed)
            {
                param_config_entered = false;
                currentParamSelected = 0;
                LOG_DEBUG(TAG, "Saving config");
                uint32_t oldFGcolor = tft.textcolor;
                uint32_t oldBGColor = tft.textbgcolor;
                if(!saveConfig())
                {
                    LOG_ERROR(TAG, "Config save failed");
                    tft.setTextColor(TFT_RED, LCD_PARAM_MENU_BG_COLOR);
                    tft.fillRect(0,320-50,240,30, LCD_PARAM_MENU_BG_COLOR);
                    tft.drawCentreString("Config not saved", 120, 320-50, 4);
                }
                else
                {
                    LOG_INFO(TAG, "Config saved");
                    tft.setTextColor(TFT_GREEN, LCD_PARAM_MENU_BG_COLOR);
                    tft.fillRect(0,320-50,240, 30, LCD_PARAM_MENU_BG_COLOR);
                    tft.drawCentreString("Config saved", 120, 320-50, 4);
                }
                tft.setTextColor(oldFGcolor, oldBGColor);
                delay(100);
                encoderSW_pressed = false;
                encoderSW_press_cnt_old = encoderSW_press_cnt;
                currentParamSelected = 0;
            }
        }
        if(!param_config_entered) currentParamSelected = 0;
        if(currentParamSelected > 9) currentParamSelected = 0;
        //LOG_INFO(TAG, "%u, %u", param_config_entered, currentParamSelected);
        
        paramScreenSprite.setTextFont(2);
        paramScreenSprite.setBitmapColor(TFT_WHITE, param_config_entered ? (editCurrentSelectedParameter ? TFT_RED : TFT_PURPLE) : TFT_BLACK);
        
        if(!editCurrentSelectedParameter) 
            oldEncPosition = lastEncoderPos;
        
        secondPairOffTemp = first_pair_shutdown_temp + shutdown_temp_delta;
        switch(currentParamSelected)
        {
            case 1:
                if(editCurrentSelectedParameter && (lastEncoderPos != oldEncPosition)) 
                {
                    float tmp = freeze_protection_temperature_limit;
                    tmp += (lastEncoderPos - oldEncPosition) * 0.1f;
                    if(tmp < 0.0f) tmp = 0.0f;
                    if(tmp > 10.0f) tmp = 10.0f; 
                    if(first_pair_shutdown_temp <= tmp) first_pair_shutdown_temp = tmp + 0.1f;
                    if(start_temperature_requirement+0.1f <= tmp) start_temperature_requirement = tmp + 0.2f;

                    if(first_pair_shutdown_temp + shutdown_temp_delta <= tmp) shutdown_temp_delta = tmp - first_pair_shutdown_temp + 0.1f;

                    freeze_protection_temperature_limit = tmp;
                    oldEncPosition = lastEncoderPos;
                }
                paramScreenSprite.fillScreen(TFT_BLACK);
                paramScreenSprite.setCursor(0, 0);
                paramScreenSprite.printf("Freeze protection: %.1f`C\n", freeze_protection_temperature_limit);
                paramScreenSprite.pushSprite(0,30);
                break;

            case 2:
                if(editCurrentSelectedParameter && (lastEncoderPos != oldEncPosition)) 
                {
                    float tmp = start_temperature_requirement;
                    tmp += (lastEncoderPos - oldEncPosition) * 0.1f;
                    if(tmp <= first_pair_shutdown_temp) tmp = first_pair_shutdown_temp + 0.1f;
                    if(tmp <= freeze_protection_temperature_limit) tmp = freeze_protection_temperature_limit + 0.2f;
                    if(tmp > 75.0f) tmp = 75.0f;
                    start_temperature_requirement = tmp;
                    oldEncPosition = lastEncoderPos;
                }
                paramScreenSprite.fillScreen(TFT_BLACK);
                paramScreenSprite.setCursor(0, 0);
                paramScreenSprite.printf("Start temperature: %.1f`C\n", start_temperature_requirement);
                paramScreenSprite.pushSprite(0,30+18);
                break;

            case 3:
                if(editCurrentSelectedParameter && (lastEncoderPos != oldEncPosition)) 
                {
                    float tmp = first_pair_shutdown_temp;
                    tmp += (lastEncoderPos - oldEncPosition) * 0.1f;
                    if(tmp <= freeze_protection_temperature_limit) tmp = freeze_protection_temperature_limit + 0.1f;
                    if(tmp >= start_temperature_requirement) tmp = start_temperature_requirement-0.1f;

                    if(tmp + shutdown_temp_delta <= freeze_protection_temperature_limit) shutdown_temp_delta = freeze_protection_temperature_limit - tmp + 0.1f;

                    first_pair_shutdown_temp = tmp;
                    oldEncPosition = lastEncoderPos;
                }
                paramScreenSprite.fillScreen(TFT_BLACK);
                paramScreenSprite.setCursor(0, 0);
                paramScreenSprite.printf("First pair off temp: %.1f`C\n", first_pair_shutdown_temp);
                paramScreenSprite.pushSprite(0,30+2*18);
                break;

            case 4:
                secondPairOffTemp = 0;
                if(editCurrentSelectedParameter && (lastEncoderPos != oldEncPosition)) 
                {
                    float tmp = shutdown_temp_delta;//negativno
                    tmp += (lastEncoderPos - oldEncPosition) * 0.1f;
                    if(tmp > CONFIG_TEMP_DELTA_MAX) tmp = CONFIG_TEMP_DELTA_MAX;
                    if(first_pair_shutdown_temp + tmp <= freeze_protection_temperature_limit) tmp = freeze_protection_temperature_limit - first_pair_shutdown_temp + 0.1f;

                    shutdown_temp_delta = tmp;
                    oldEncPosition = lastEncoderPos;
                }
                paramScreenSprite.fillScreen(TFT_BLACK);
                paramScreenSprite.setCursor(0, 0);
                paramScreenSprite.printf("Second pair delta: %.1f`C\n", shutdown_temp_delta);
                paramScreenSprite.pushSprite(0,30+3*18);

                
                secondPairOffTemp = first_pair_shutdown_temp + shutdown_temp_delta;
                paramScreenSprite.fillScreen(TFT_BLACK);
                paramScreenSprite.setCursor(0, 0);
                paramScreenSprite.printf("Second pair off temp: %.1f`C\n", secondPairOffTemp);
                paramScreenSprite.pushSprite(0,30+4*18);
                break;

            case 5:
                if(editCurrentSelectedParameter && (lastEncoderPos != oldEncPosition)) 
                {
                    uint32_t tmp = low_pressure_timeout_seconds;
                    int64_t x = tmp + (lastEncoderPos - oldEncPosition);
                    if(x < 0) tmp = 0;
                    else tmp = x;
                    //if(tmp > 60) tmp = 60;
                    low_pressure_timeout_seconds = tmp;
                    oldEncPosition = lastEncoderPos;
                }
                paramScreenSprite.fillScreen(TFT_BLACK);
                paramScreenSprite.setCursor(0, 0);
                paramScreenSprite.printf("Low pressure timeout: %us\n", low_pressure_timeout_seconds);
                paramScreenSprite.pushSprite(0,30+5*18);
                break;

            case 6:
                if(editCurrentSelectedParameter && (lastEncoderPos != oldEncPosition)) 
                {
                    uint32_t tmp = flow_switch_timeout_seconds;
                    int64_t x = tmp + (lastEncoderPos - oldEncPosition);
                    if(x < 0) tmp = 0;
                    else tmp = x;
                    //if(tmp > 60) tmp = 60;
                    flow_switch_timeout_seconds = tmp;
                    oldEncPosition = lastEncoderPos;
                }
                paramScreenSprite.fillScreen(TFT_BLACK);
                paramScreenSprite.setCursor(0, 0);
                paramScreenSprite.printf("Flow timeout: %us\n", flow_switch_timeout_seconds);
                paramScreenSprite.pushSprite(0,30+6*18);
                break;

            case 7:
                if(editCurrentSelectedParameter && (lastEncoderPos != oldEncPosition)) 
                {
                    uint32_t tmp = compressor_start_delay_seconds;
                    int64_t x = tmp + (lastEncoderPos - oldEncPosition);
                    if(x < 0) tmp = 0;
                    else tmp = x;
                    //if(tmp > 60) tmp = 60;
                    compressor_start_delay_seconds = tmp;
                    oldEncPosition = lastEncoderPos;
                }
                paramScreenSprite.fillScreen(TFT_BLACK);
                paramScreenSprite.setCursor(0, 0);
                paramScreenSprite.printf("Compressor start delay: %us\n", compressor_start_delay_seconds);
                paramScreenSprite.pushSprite(0,30+7*18);
                break;

            case 8:
                if(editCurrentSelectedParameter && (lastEncoderPos != oldEncPosition)) 
                {
                    uint32_t tmp = water_pump_keep_running_time_seconds;
                    int64_t x = tmp + (lastEncoderPos - oldEncPosition);
                    if(x < 0) tmp = 0;
                    else tmp = x;
                    //if(tmp > 60) tmp = 60;
                    water_pump_keep_running_time_seconds = tmp;
                    oldEncPosition = lastEncoderPos;
                }
                paramScreenSprite.fillScreen(TFT_BLACK);
                paramScreenSprite.setCursor(0, 0);
                paramScreenSprite.printf("Water pump keep running: %us\n", water_pump_keep_running_time_seconds);
                paramScreenSprite.pushSprite(0,30+8*18);
                break;

            case 9:
                if(editCurrentSelectedParameter && (lastEncoderPos != oldEncPosition)) 
                {
                    uint32_t tmp = water_pump_stop_delay_seconds;
                    int64_t x = tmp + (lastEncoderPos - oldEncPosition);
                    if(x < 0) tmp = 0;
                    else tmp = x;
                    //if(tmp > 60) tmp = 60;
                    water_pump_stop_delay_seconds = tmp;
                    oldEncPosition = lastEncoderPos;
                }
                paramScreenSprite.fillScreen(TFT_BLACK);
                paramScreenSprite.setCursor(0, 0);
                paramScreenSprite.printf("Water pump stop delay: %us\n", water_pump_stop_delay_seconds);
                paramScreenSprite.pushSprite(0,30+9*18);
                break;

            default:
                break;
        }

        paramScreenSprite.setBitmapColor(TFT_WHITE, TFT_BLACK);
        paramScreenSprite.fillScreen(TFT_BLACK);
        paramScreenSprite.setTextColor(TFT_WHITE, TFT_BLACK);

        paramScreenSprite.setCursor(0, 0);
        paramScreenSprite.setTextFont(2);
        paramScreenSprite.printf("Freeze protection: %.1f`C\n", freeze_protection_temperature_limit);
        if(currentParamSelected != 1) paramScreenSprite.pushSprite(0,30);

        paramScreenSprite.fillScreen(TFT_BLACK);
        paramScreenSprite.setCursor(0, 0);
        paramScreenSprite.printf("Start temperature: %.1f`C\n", start_temperature_requirement);
        if(currentParamSelected != 2) paramScreenSprite.pushSprite(0,30+18);

        paramScreenSprite.fillScreen(TFT_BLACK);
        paramScreenSprite.setCursor(0, 0);
        paramScreenSprite.printf("First pair off temp: %.1f`C\n", first_pair_shutdown_temp);
        if(currentParamSelected != 3) paramScreenSprite.pushSprite(0,30+2*18);

        paramScreenSprite.fillScreen(TFT_BLACK);
        paramScreenSprite.setCursor(0, 0);
        paramScreenSprite.printf("Second pair delta: %.1f`C\n", shutdown_temp_delta);
        if(currentParamSelected != 4) paramScreenSprite.pushSprite(0,30+3*18);

        paramScreenSprite.fillScreen(TFT_BLACK);
        paramScreenSprite.setCursor(0, 0);
        paramScreenSprite.printf("Second pair off temp: %.1f`C\n", secondPairOffTemp);
        if(currentParamSelected != 4) paramScreenSprite.pushSprite(0,30+4*18);

        paramScreenSprite.fillScreen(TFT_BLACK);
        paramScreenSprite.setCursor(0, 0);
        paramScreenSprite.printf("Low pressure timeout: %us\n", low_pressure_timeout_seconds);
        if(currentParamSelected != 5) paramScreenSprite.pushSprite(0,30+5*18);

        paramScreenSprite.fillScreen(TFT_BLACK);
        paramScreenSprite.setCursor(0, 0);
        paramScreenSprite.printf("Flow timeout: %us\n", flow_switch_timeout_seconds);
        if(currentParamSelected != 6) paramScreenSprite.pushSprite(0,30+6*18);

        paramScreenSprite.fillScreen(TFT_BLACK);
        paramScreenSprite.setCursor(0, 0);
        paramScreenSprite.printf("Compressor start delay: %us\n", compressor_start_delay_seconds);
        if(currentParamSelected != 7) paramScreenSprite.pushSprite(0,30+7*18);

        paramScreenSprite.fillScreen(TFT_BLACK);
        paramScreenSprite.setCursor(0, 0);
        paramScreenSprite.printf("Water pump keep running: %us\n", water_pump_keep_running_time_seconds);
        if(currentParamSelected != 8) paramScreenSprite.pushSprite(0,30+8*18);

        paramScreenSprite.fillScreen(TFT_BLACK);
        paramScreenSprite.setCursor(0, 0);
        paramScreenSprite.printf("Water pump stop delay: %us\n", water_pump_stop_delay_seconds);
        if(currentParamSelected != 9) paramScreenSprite.pushSprite(0,30+9*18);
        
        /*while(param_config_entered && (currentParamSelectedOld == currentParamSelected))
        {
            
        }*/
        updateUptimeCentreString(120, 320-16, 2, param_config_entered ? TFT_RED : TFT_WHITE, LCD_PARAM_MENU_BOT_BG_COLOR);
        giveToMonitoring(1);
        currentParamSelectedOld = currentParamSelected;
    }
}

void switchToAboutMenu()
{
    disableScrollArea();
    tft.fillScreen(LCD_ABOUT_MENU_BG_COLOR);
    tft.fillRect(0,0,240,16, LCD_ABOUT_MENU_TOP_BG_COLOR);
    tft.fillRect(0,320-16,240,16, LCD_ABOUT_MENU_BOT_BG_COLOR);
    tft.setTextColor(TFT_WHITE, LCD_ABOUT_MENU_TOP_BG_COLOR);

    tft.drawCentreString("About System",120,0,2);

    tft.setCursor(0, 20);
    tft.setTextColor(TFT_RED, LCD_ABOUT_MENU_BG_COLOR);
    tft.setTextFont(2);
    tft.printf("%s rev %u, %u x %u MHz\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores() ,ESP.getCpuFreqMHz());
    tft.printf("Flash: %u MB (%u Mbit), %u MHz\n", ESP.getFlashChipSize()/(1024*1024), ESP.getFlashChipSize()/(1024*128), ESP.getFlashChipSpeed()/1000000);
    tft.print("RST[CPU0]: ");
    printResetReason(tft, rtc_get_reset_reason(0));
    tft.print("RST[CPU1]: ");
    printResetReason(tft, rtc_get_reset_reason(1));
    
    tft.setTextColor(TFT_BLUE, LCD_ABOUT_MENU_BG_COLOR);

    float tmp = ((ESP.getSketchSize()*1.0f)/(ESP.getFreeSketchSpace()*1.0f))*100.0f;
    if(tmp >= 100.0f)
    {
        tft.printf("Firmware size: %u bytes 100%%\n", ESP.getSketchSize());
    }
    else
    {
        tft.printf("Firmware size: %u bytes %.2f%%\n", ESP.getSketchSize(), tmp);
    }
    tft.printf("Firmware partition: %u bytes\n", ESP.getFreeSketchSpace());
    tft.printf("Firmware: %s - Timestamp:\n%s\n", VERSION_SHORT, BUILD_DATETIME);
    tft.printf("ESP-IDF: %s , ARDUINO: v%d.%d.%d\n", esp_get_idf_version(), 
                                    ESP_ARDUINO_VERSION_MAJOR, 
                                    ESP_ARDUINO_VERSION_MINOR,
                                    ESP_ARDUINO_VERSION_PATCH);
    //tft.printf("Firmware MD5: %s\n", ESP.getSketchMD5().c_str());
    
    //int16_t cursorX = tft.getCursorX();
    int16_t cursorY = tft.getCursorY();

    lastForceUpdate = currentTimeUL();
    aboutScreenSprite.createSprite(240, 320-cursorY-16);
    aboutScreenSprite.setColorDepth(8);
    aboutScreenSprite.setTextFont(2);
    uint8_t apsta = 0;
    while(currentLCDMenu == LCD_MENU_ABOUT)
    {
        if(currentTimeUL() - lastForceUpdate >= FORCE_UPDATE_INTERVAL_MS)
        {
            int16_t cursorXtmp = tft.getCursorX();
            int16_t cursorYtmp = tft.getCursorY();
            tft.setTextColor(TFT_WHITE, LCD_ABOUT_MENU_TOP_BG_COLOR);
            tft.setCursor(0,0);
            tft.setTextFont(2);
            taskENTER_CRITICAL(&temperatureRead_spinlock);
            tft.printf("%3.2f`C", temperatureReadFixed());
            taskEXIT_CRITICAL(&temperatureRead_spinlock);
            tft.setCursor(cursorXtmp, cursorYtmp);

            aboutScreenSprite.fillScreen(LCD_ABOUT_MENU_BG_COLOR);
            aboutScreenSprite.setCursor(0, 0);
            aboutScreenSprite.setTextColor(TFT_GREEN, LCD_ABOUT_MENU_BG_COLOR);
            aboutScreenSprite.printf("[RAM] total heap size: %u bytes\n", ESP.getHeapSize());
            aboutScreenSprite.printf("[RAM] available heap: %u bytes\n", ESP.getFreeHeap());
            aboutScreenSprite.printf("[RAM] lowest heap yet: %u bytes\n", ESP.getMinFreeHeap());
            aboutScreenSprite.printf("[RAM] max allocatable: %u bytes\n", ESP.getMaxAllocHeap());
            int16_t cursorYSprite = aboutScreenSprite.getCursorY();

            aboutScreenSprite.setTextColor(TFT_ORANGE, LCD_ABOUT_MENU_BG_COLOR);

            switch(WiFi.getMode())
            {
                case WIFI_MODE_STA:
                {
                    aboutScreenSprite.print("WiFi STA ");
                    if(WiFi.isConnected())
                    {
                        aboutScreenSprite.printf(", channel %ld , rssi: %ddBm\n", WiFi.channel(), WiFi.RSSI());
                        aboutScreenSprite.printf("SSID: %s\n", WiFi.SSID().c_str());
                        aboutScreenSprite.printf("IP: %s - %s\n", WiFi.localIP().toString().c_str(), WiFi.getHostname());
                        if(!wifiManPortalActive)
                        {
                            aboutScreenSprite.printf("Port: %u , MAC: %s", SERVER_PORT, WiFi.macAddress().c_str()); 
                        }
                        else
                        {
                            aboutScreenSprite.printf("Port: %u&%u , MAC: %s", SERVER_PORT, wifi_manPort, WiFi.macAddress().c_str()); 
                        }
                    }
                    else
                    {
                        aboutScreenSprite.print(", not connected");
                    }
                }
                    break;
                case WIFI_MODE_AP:
                {
                    aboutScreenSprite.printf("WiFi AP , channel %ld , clients: %u\n", WiFi.channel(), WiFi.softAPgetStationNum()); 
                    aboutScreenSprite.printf("SSID: %s\n", WiFi.softAPSSID().c_str());
                    aboutScreenSprite.printf("IP: %s - %s\n", WiFi.softAPIP().toString().c_str(), WiFi.softAPgetHostname());
                    if(!wifiManPortalActive)
                    {
                        aboutScreenSprite.printf("Port: %u , MAC: %s", SERVER_PORT, WiFi.softAPmacAddress().c_str()); 
                    }
                    else
                    {
                        aboutScreenSprite.printf("Port: %u&%u , MAC: %s", SERVER_PORT, wifi_manPort, WiFi.softAPmacAddress().c_str()); 
                    }
                }
                    break;
                case WIFI_MODE_APSTA:
                {
                    aboutScreenSprite.fillRect(0,cursorYSprite,240,7, TFT_BROWN);
                    aboutScreenSprite.setTextColor(TFT_WHITE, TFT_BROWN);
                    aboutScreenSprite.setCursor(0,cursorYSprite);
                    aboutScreenSprite.setTextFont(GLCD);
                    aboutScreenSprite.println("WiFi Mode: AP & STATION");
                    aboutScreenSprite.setTextFont(2);
                    aboutScreenSprite.setTextColor(TFT_ORANGE, LCD_ABOUT_MENU_BG_COLOR);
                    
                    if(apsta < 15)
                    {
                        aboutScreenSprite.printf("WiFi AP , channel %ld , clients: %u\n", WiFi.channel(), WiFi.softAPgetStationNum()); 
                        aboutScreenSprite.printf("SSID: %s\n", WiFi.softAPSSID().c_str());
                        aboutScreenSprite.printf("IP: %s - %s\n", WiFi.softAPIP().toString().c_str(), WiFi.softAPgetHostname());
                        if(!wifiManPortalActive)
                        {
                            aboutScreenSprite.printf("Port: %u , MAC: %s", SERVER_PORT, WiFi.softAPmacAddress().c_str()); 
                        }
                        else
                        {
                            aboutScreenSprite.printf("Port: %u&%u , MAC: %s", SERVER_PORT, wifi_manPort, WiFi.softAPmacAddress().c_str()); 
                        }
                        apsta++;
                    }
                    else
                    {
                        apsta++;
                        if(apsta != 0)
                        {
                            if(apsta > 30) apsta = 0;
                            aboutScreenSprite.print("WiFi STA ");
                            if(WiFi.isConnected())
                            {
                                aboutScreenSprite.printf(", channel %ld , rssi: %ddBm\n", WiFi.channel(), WiFi.RSSI());
                                aboutScreenSprite.printf("SSID: %s\n", WiFi.SSID().c_str());
                                aboutScreenSprite.printf("IP: %s - %s\n", WiFi.localIP().toString().c_str(), WiFi.getHostname());
                                if(!wifiManPortalActive)
                                {
                                    aboutScreenSprite.printf("Port: %u , MAC: %s", SERVER_PORT, WiFi.macAddress().c_str()); 
                                }
                                else
                                {
                                    aboutScreenSprite.printf("Port: %u&%u , MAC: %s", SERVER_PORT, wifi_manPort, WiFi.macAddress().c_str()); 
                                }
                            }
                            else
                            {
                                aboutScreenSprite.print(", not connected");
                            }
                        }
                    }
                }
                    break;
                default:
                    aboutScreenSprite.println("WiFi not started!");
            }
            aboutScreenSprite.pushSprite(0, cursorY);
            lastForceUpdate = currentTimeUL();
        }
        updateUptimeCentreString(120, 320-16, 2, TFT_WHITE, LCD_ABOUT_MENU_BOT_BG_COLOR);
        giveToMonitoring(1);
    }
    aboutScreenSprite.deleteSprite();
}

void printResetReason(Print& p, int reason)
{
    switch (reason)
    {
        case 1 : p.println ("POWERON_RESET");break;          /**<1,  Vbat power on reset*/
        case 3 : p.println ("SW_RESET");break;               /**<3,  Software reset digital core*/
        case 4 : p.println ("OWDT_RESET");break;             /**<4,  Legacy watch dog reset digital core*/
        case 5 : p.println ("DEEPSLEEP_RESET");break;        /**<5,  Deep Sleep reset digital core*/
        case 6 : p.println ("SDIO_RESET");break;             /**<6,  Reset by SLC module, reset digital core*/
        case 7 : p.println ("TG0WDT_SYS_RESET");break;       /**<7,  Timer Group0 Watch dog reset digital core*/
        case 8 : p.println ("TG1WDT_SYS_RESET");break;       /**<8,  Timer Group1 Watch dog reset digital core*/
        case 9 : p.println ("RTCWDT_SYS_RESET");break;       /**<9,  RTC Watch dog Reset digital core*/
        case 10 : p.println ("INTRUSION_RESET");break;       /**<10, Instrusion tested to reset CPU*/
        case 11 : p.println ("TGWDT_CPU_RESET");break;       /**<11, Time Group reset CPU*/
        case 12 : p.println ("SW_CPU_RESET");break;          /**<12, Software reset CPU*/
        case 13 : p.println ("RTCWDT_CPU_RESET");break;      /**<13, RTC Watch dog Reset CPU*/
        case 14 : p.println ("EXT_CPU_RESET");break;         /**<14, for APP CPU, reseted by PRO CPU*/
        case 15 : p.println ("RTCWDT_BROWN_OUT_RESET");break;/**<15, Reset when the vdd voltage is not stable*/
        case 16 : p.println ("RTCWDT_RTC_RESET");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
        default : p.println ("NO_MEAN");
    }
}

void switchToMainMenu()
{
    disableScrollArea();
    tft.fillScreen(LCD_MAIN_MENU_BG_COLOR);
    tft.fillRect(0, 0, 240, 32, LCD_MAIN_MENU_TOP_BG_COLOR);
    tft.fillRect(0, 32, 240, 1, LCD_MAIN_MENU_SEPARATOR_LINE_COLOR);
    

    tft.fillSmoothRoundRect(1, 35, 238, 123, 10, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR, LCD_MAIN_MENU_BG_COLOR);
    tft.drawSmoothRoundRect(1, 35, 10, 8, 238, 123, LCD_MAIN_MENU_UPPER_LINE_COLOR, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("Group 1 & Exchanger 1", 120, 42, GFXFF);

    tft.fillSmoothRoundRect(1, 161, 238, 123, 10, LCD_MAIN_MENU_BOTTOM_GROUP_BG_COLOR, LCD_MAIN_MENU_BG_COLOR);
    tft.drawSmoothRoundRect(1, 161, 10, 8, 238, 123, LCD_MAIN_MENU_BOTTOM_LINE_COLOR, LCD_MAIN_MENU_BOTTOM_GROUP_BG_COLOR);
    tft.setFreeFont(FSSB9);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("Group 2 & Exchanger 2", 120, 168, GFXFF);

    tft.fillRect(0, 320-32, 240, 32, LCD_MAIN_MENU_BOT_BG_COLOR);
    tft.fillRect(0, 320-32-1, 240, 1, LCD_MAIN_MENU_SEPARATOR_LINE_COLOR);

    tft.setTextColor(TFT_WHITE, LCD_MAIN_MENU_TOP_BG_COLOR);
    tft.setFreeFont(FSSB12);                 // Select the font
    tft.drawCentreString("STATUS", 120, 6, GFXFF);// Print the string name of the font
    oldErrorDisplayTime = esp_timer_get_time();

    while(currentLCDMenu == LCD_MENU_MAIN)
    {

        if((currentTimeUL() - lastForceUpdate >= FORCE_UPDATE_INTERVAL_MS))
        {
            heatExchanger1InputTemperatureLocal = -200.0f;
            heatExchanger1OutputTemperatureLocal = -200.0f;
            heatExchanger1TemperatureLocal = -200.0f;
            heatExchanger2InputTemperatureLocal = -200.0f;
            heatExchanger2OutputTemperatureLocal = -200.0f;
            heatExchanger2TemperatureLocal = -200.0f;
            recirculatingPumpWaterInputTemperatureLocal = -200.0f;
            recirculatingPumpWaterOutputTemperatureLocal = -200.0f;
            lastForceUpdate = currentTimeUL();
        }

        if(!mainOnOffState)
            tft.setBitmapColor(TFT_RED, LCD_MAIN_MENU_TOP_BG_COLOR);
        else if(mainOnOffState && controlTaskRunning)
            tft.setBitmapColor(TFT_GREEN, LCD_MAIN_MENU_TOP_BG_COLOR);
        else if(mainOnOffState && !controlTaskRunning)
            tft.setBitmapColor(TFT_ORANGE, LCD_MAIN_MENU_TOP_BG_COLOR);
        else
            tft.setBitmapColor(TFT_WHITE, LCD_MAIN_MENU_TOP_BG_COLOR);
        iconSprite32.pushImage(0,0,32,32, (uint16_t*)powerSymbolMono, 1);
        iconSprite32.pushSprite(240-32, 0);

        if(safetyErrorOccured)
        {
            tft.setBitmapColor(waitingForUserToResetSafetyError ? TFT_ORANGE : TFT_RED, LCD_MAIN_MENU_TOP_BG_COLOR);
            iconSprite32.pushImage(0,0,32,32,(uint16_t*)syncAlertSymbolMono,1);
        }
        else
        {
            if(controlTaskRunning)
            {
                if(enteredMainLoop)
                {
                    if(waterPumpRunningTillEnd)
                    {
                        tft.setBitmapColor(TFT_SKYBLUE, LCD_MAIN_MENU_TOP_BG_COLOR);
                        iconSprite32.pushImage(0,0,32,32,(uint16_t*)syncSymbolMono,1);
                    }
                    else
                    {
                        tft.setBitmapColor(TFT_GREEN, LCD_MAIN_MENU_TOP_BG_COLOR);
                        iconSprite32.pushImage(0,0,32,32,(uint16_t*)syncSymbolMono,1);
                    }
                }
                else
                {
                    tft.setBitmapColor(TFT_YELLOW, LCD_MAIN_MENU_TOP_BG_COLOR);
                    iconSprite32.pushImage(0,0,32,32,(uint16_t*)syncSymbolMono,1);
                }
            }
            else
            {
                tft.setBitmapColor(TFT_RED, LCD_MAIN_MENU_TOP_BG_COLOR);
                iconSprite32.pushImage(0,0,32,32,(uint16_t*)syncOffSymbolMono,1);
            }
        }
        iconSprite32.pushSprite(0, 0);
        if(errorCount > 0)
        {
            tft.setBitmapColor(TFT_RED, LCD_MAIN_MENU_TOP_BG_COLOR);
            iconSprite32.pushImage(0,0,32,32,(uint16_t*)stopSymbolMono,1);
            if((unsigned long) ((esp_timer_get_time() - oldErrorDisplayTime) / 1000ULL) > 5000UL) 
            {
                errorCount = 0;
            }
        }
        else
        {
            tft.setBitmapColor(TFT_GREEN, LCD_MAIN_MENU_TOP_BG_COLOR);
            iconSprite32.pushImage(0,0,32,32,(uint16_t*)checkSymbolMono,1);
        }
        iconSprite32.pushSprite(32, 0);

        if(recirculatingPumpWaterInputTemperature <= start_temperature_requirement)
        {
            tft.setBitmapColor((recirculatingPumpWaterInputTemperature == DEVICE_DISCONNECTED_C) ? TFT_RED : TFT_ORANGE, LCD_MAIN_MENU_TOP_BG_COLOR);
            iconSprite32.pushImage(0,0,32,32,(uint16_t*)temperatureAlertSymbolMono,1);
        }
        else
        {
            tft.setBitmapColor(TFT_GREEN, LCD_MAIN_MENU_TOP_BG_COLOR);
            iconSprite32.pushImage(0,0,32,32,(uint16_t*)temperatureCheckSymbolMono,1);
        }
        
        iconSprite32.pushSprite(240-(2*32), 0);

        tft.setBitmapColor(waterPumpMotorProtectionState ? TFT_RED : TFT_GREEN, LCD_MAIN_MENU_BOT_BG_COLOR);
        iconSprite32.pushImage(0,0,32,32,(uint16_t*)motorSymbolMono,1);
        iconSprite32.pushSprite(0, 320-32);

        tft.setBitmapColor((waterPumpMotorState && !waterPumpMotorProtectionState) ? TFT_GREEN : TFT_RED, LCD_MAIN_MENU_BOT_BG_COLOR);
        iconSprite32.pushImage(0,0,32,32,(uint16_t*)liquidPumpSymbolMono,1);
        iconSprite32.pushSprite(32, 320-32);

        tft.setBitmapColor(waterFlowSwitchState ? 
                                    (waterPumpMotorState ? TFT_BLUE : TFT_ORANGE) : TFT_WHITE, LCD_MAIN_MENU_BOT_BG_COLOR);
        iconSprite32.pushImage(0,0,32,32,(uint16_t*)waterFlowSymbolMono,1);
        iconSprite32.pushSprite(32*2, 320-32);

        tft.setBitmapColor(highPressureProtectionActive ? TFT_RED : TFT_GREEN, LCD_MAIN_MENU_BOT_BG_COLOR);
        iconSprite32.pushImage(0,0,32,32,(uint16_t*)lowPressureSymbolMono,1);
        iconSprite32.pushSprite(240-(32*2), 320-32);

        tft.setBitmapColor(lowPressureSwitchActive ? TFT_RED : TFT_GREEN, LCD_MAIN_MENU_BOT_BG_COLOR);
        iconSprite32.pushImage(0,0,32,32,(uint16_t*)highPressureSymbolMono,1);
        iconSprite32.pushSprite(240-32, 320-32);


        iconSprite32.setBitmapColor(compressor1State ? TFT_GREEN : TFT_RED, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
        iconSprite32.pushImage(0,0,32,32,(uint16_t*)compressorSymbolMono,1);
        iconSprite32.setFreeFont(FSSBD9);
        iconSprite32.drawCentreString("1", 12, 8, GFXFF);
        iconSprite32.pushSprite(10, 70);
        
        iconSprite32.setBitmapColor(compressor3State ? TFT_GREEN : TFT_RED, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
        iconSprite32.pushImage(0,0,32,32,(uint16_t*)compressorSymbolMono,1);
        iconSprite32.setFreeFont(FSSBD9);
        iconSprite32.drawCentreString("3", 12, 8, GFXFF);
        iconSprite32.pushSprite(10, 80+32);

        tft.setFreeFont(FSSBD9);
        tft.setTextColor(TFT_SKYBLUE, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);

        if((heatExchanger1TemperatureLocal != heatExchanger1Temperature) || criticalTemperatureFreezeProtection)
        {
            heatExchanger1TemperatureLocal = heatExchanger1Temperature;
            if(heatExchanger1TemperatureLocal == DEVICE_DISCONNECTED_C)
            {
                sprintf(buffMain, "Temp: error");
            }
            else
            {
                sprintf(buffMain, "Temp: %6.2f`C", heatExchanger1TemperatureLocal);
            }
            mainScreenTempSprite.fillScreen(TFT_BLACK);
            mainScreenTempSprite.setFreeFont(FSSBD9);
            mainScreenTempSprite.setTextColor(TFT_WHITE, TFT_BLACK);
            mainScreenTempSprite.drawCentreString(buffMain, 70, 0, GFXFF);
            mainScreenTempSprite.setBitmapColor((heatExchanger1TemperatureLocal == DEVICE_DISCONNECTED_C || criticalTemperatureFreezeProtection) ?
                                                        TFT_RED : TFT_SKYBLUE, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
            mainScreenTempSprite.pushSprite(98, 70);
        }

        if(heatExchanger1InputTemperatureLocal != heatExchanger1InputTemperature)
        {
            heatExchanger1InputTemperatureLocal = heatExchanger1InputTemperature;
            if(heatExchanger1InputTemperatureLocal == DEVICE_DISCONNECTED_C)
            {
                sprintf(buffMain, "In: error");
            }
            else
            {
                sprintf(buffMain, "In: %6.2f`C", heatExchanger1InputTemperatureLocal);
            }
            mainScreenTempSprite.fillScreen(TFT_BLACK);
            mainScreenTempSprite.setFreeFont(FSSBD9);
            mainScreenTempSprite.setTextColor(TFT_WHITE, TFT_BLACK);
            mainScreenTempSprite.drawCentreString(buffMain, 70, 0, GFXFF);
            mainScreenTempSprite.setBitmapColor((heatExchanger1InputTemperatureLocal == DEVICE_DISCONNECTED_C) ?
                                                        TFT_RED : TFT_SKYBLUE, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
            mainScreenTempSprite.pushSprite(98, 100);
        }

        if(heatExchanger1OutputTemperatureLocal != heatExchanger1OutputTemperature)
        {
            heatExchanger1OutputTemperatureLocal = heatExchanger1OutputTemperature;
            if(heatExchanger1OutputTemperatureLocal == DEVICE_DISCONNECTED_C)
            {
                sprintf(buffMain, "Out: error");
            }
            else
            {
                sprintf(buffMain, "Out: %6.2f`C", heatExchanger1OutputTemperatureLocal);
            }
            mainScreenTempSprite.fillScreen(TFT_BLACK);
            mainScreenTempSprite.setFreeFont(FSSBD9);
            mainScreenTempSprite.setTextColor(TFT_WHITE, TFT_BLACK);
            mainScreenTempSprite.drawCentreString(buffMain, 70, 0, GFXFF);
            mainScreenTempSprite.setBitmapColor((heatExchanger1OutputTemperatureLocal == DEVICE_DISCONNECTED_C) ?
                                                        TFT_RED : TFT_SKYBLUE, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
            mainScreenTempSprite.pushSprite(98, 130);
        }
        
        tft.setTextColor(TFT_SKYBLUE, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
        tft.drawString("Comp. 1", 10+32+5, 70, 2);
        tft.setTextColor(compressor1State ? TFT_GREEN : TFT_RED, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
        tft.drawString(compressor1State ? "Running " : "Stopped", 10+32+5, 70+16, 2);

        tft.setTextColor(TFT_SKYBLUE, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
        tft.drawString("Comp. 3", 10+32+5, 80+32, 2);
        tft.setTextColor(compressor3State ? TFT_GREEN : TFT_RED, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
        tft.drawString(compressor3State ? "Running " : "Stopped", 10+32+5, 70+16+32+10, 2);

        iconSprite32.setBitmapColor(compressor2State ? TFT_GREEN : TFT_RED, LCD_MAIN_MENU_BOTTOM_GROUP_BG_COLOR);
        iconSprite32.pushImage(0,0,32,32,(uint16_t*)compressorSymbolMono,1);
        iconSprite32.setFreeFont(FSSBD9);
        iconSprite32.drawCentreString("2", 12, 8, GFXFF);
        iconSprite32.pushSprite(10, 70+126);

        iconSprite32.setBitmapColor(compressor4State ? TFT_GREEN : TFT_RED, LCD_MAIN_MENU_BOTTOM_GROUP_BG_COLOR);
        iconSprite32.pushImage(0,0,32,32,(uint16_t*)compressorSymbolMono,1);
        iconSprite32.setFreeFont(FSSBD9);
        iconSprite32.drawCentreString("4", 12, 8, GFXFF);
        iconSprite32.pushSprite(10, 80+32+126);

        tft.setFreeFont(FSSBD9);
        tft.setTextColor(TFT_SKYBLUE, LCD_MAIN_MENU_BOTTOM_GROUP_BG_COLOR);

        if((heatExchanger2TemperatureLocal != heatExchanger2Temperature) || criticalTemperatureFreezeProtection)
        {
            heatExchanger2TemperatureLocal = heatExchanger2Temperature;
            if(heatExchanger2TemperatureLocal == DEVICE_DISCONNECTED_C)
            {
                sprintf(buffMain, "Temp: error");
            }
            else
            {
                sprintf(buffMain, "Temp: %6.2f`C", heatExchanger2TemperatureLocal);
            }
            mainScreenTempSprite.fillScreen(TFT_BLACK);
            mainScreenTempSprite.setFreeFont(FSSBD9);
            mainScreenTempSprite.setTextColor(TFT_WHITE, TFT_BLACK);
            mainScreenTempSprite.drawCentreString(buffMain, 70, 0, GFXFF);
            mainScreenTempSprite.setBitmapColor((heatExchanger2TemperatureLocal == DEVICE_DISCONNECTED_C || criticalTemperatureFreezeProtection) ?
                                                        TFT_RED : TFT_SKYBLUE, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
            mainScreenTempSprite.pushSprite(98, 70+126);
        }

        if(heatExchanger2InputTemperatureLocal != heatExchanger2InputTemperature)
        {
            heatExchanger2InputTemperatureLocal = heatExchanger2InputTemperature;
            if(heatExchanger2InputTemperatureLocal == DEVICE_DISCONNECTED_C)
            {
                sprintf(buffMain, "In: error");
            }
            else
            {
                sprintf(buffMain, "In: %6.2f`C", heatExchanger2InputTemperatureLocal);
            }
            mainScreenTempSprite.fillScreen(TFT_BLACK);
            mainScreenTempSprite.setFreeFont(FSSBD9);
            mainScreenTempSprite.setTextColor(TFT_WHITE, TFT_BLACK);
            mainScreenTempSprite.drawCentreString(buffMain, 70, 0, GFXFF);
            mainScreenTempSprite.setBitmapColor((heatExchanger2InputTemperatureLocal == DEVICE_DISCONNECTED_C) ?
                                                        TFT_RED : TFT_SKYBLUE, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
            mainScreenTempSprite.pushSprite(98, 100+126);
        }

        if(heatExchanger2OutputTemperatureLocal != heatExchanger2OutputTemperature)
        {
            heatExchanger2OutputTemperatureLocal = heatExchanger2OutputTemperature;
            if(heatExchanger2OutputTemperatureLocal == DEVICE_DISCONNECTED_C)
            {
                sprintf(buffMain, "Out: error");
            }
            else
            {
                sprintf(buffMain, "Out: %6.2f`C", heatExchanger2OutputTemperatureLocal);
            }
            mainScreenTempSprite.fillScreen(TFT_BLACK);
            mainScreenTempSprite.setFreeFont(FSSBD9);
            mainScreenTempSprite.setTextColor(TFT_WHITE, TFT_BLACK);
            mainScreenTempSprite.drawCentreString(buffMain, 70, 0, GFXFF);
            mainScreenTempSprite.setBitmapColor((heatExchanger2OutputTemperatureLocal == DEVICE_DISCONNECTED_C) ?
                                                        TFT_RED : TFT_SKYBLUE, LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR);
            mainScreenTempSprite.pushSprite(98, 130+126);
        }
        tft.setTextColor(TFT_SKYBLUE, LCD_MAIN_MENU_BOTTOM_GROUP_BG_COLOR);
        tft.drawString("Comp. 2", 10+32+5, 70+126, 2);
        tft.setTextColor(compressor2State ? TFT_GREEN : TFT_RED, LCD_MAIN_MENU_BOTTOM_GROUP_BG_COLOR);
        tft.drawString(compressor2State ? "Running " : "Stopped", 10+32+5, 70+16+126, 2);

        tft.setTextColor(TFT_SKYBLUE, LCD_MAIN_MENU_BOTTOM_GROUP_BG_COLOR);
        tft.drawString("Comp. 4", 10+32+5, 80+32+126, 2);
        tft.setTextColor(compressor4State ? TFT_GREEN : TFT_RED, LCD_MAIN_MENU_BOTTOM_GROUP_BG_COLOR);
        tft.drawString(compressor4State ? "Running " : "Stopped", 10+32+5, 70+16+32+10+126, 2);
        
        if((recirculatingPumpWaterOutputTemperatureLocal != recirculatingPumpWaterOutputTemperature) || (recirculatingPumpWaterInputTemperatureLocal != recirculatingPumpWaterInputTemperature) || criticalTemperatureFreezeProtection)
        {
            recirculatingPumpWaterOutputTemperatureLocal = recirculatingPumpWaterOutputTemperature;
            if(recirculatingPumpWaterOutputTemperatureLocal == DEVICE_DISCONNECTED_C)
            {
                sprintf(buffMain, "error");
            }
            else
            {
                sprintf(buffMain, "%6.2f`C", recirculatingPumpWaterOutputTemperatureLocal);
            }

            uint32_t fgColor;
            if(recirculatingPumpWaterOutputTemperatureLocal == DEVICE_DISCONNECTED_C || criticalTemperatureFreezeProtection)
            {
                fgColor = TFT_RED;
            }
            else if((!compressor1State && !compressor3State) && (compressor2State && compressor4State) && mainOnOffState && controlTaskRunning && enteredMainLoop)
            {
                fgColor = TFT_ORANGE;
            }
            else if((compressor1State && compressor2State && compressor3State && compressor4State) && mainOnOffState && controlTaskRunning && enteredMainLoop)
            {
                fgColor = TFT_SILVER;
            }
            else
            {
                fgColor = TFT_WHITE;
            }
            mainScreenTemp2Sprite.setFreeFont(FSSBD9);
            mainScreenTemp2Sprite.setTextColor(fgColor, LCD_MAIN_MENU_BOT_BG_COLOR);
            mainScreenTemp2Sprite.fillScreen(LCD_MAIN_MENU_BOT_BG_COLOR);
            mainScreenTemp2Sprite.drawCentreString(buffMain, (240-(5*32))/2, 0, GFXFF);

            
            recirculatingPumpWaterInputTemperatureLocal = recirculatingPumpWaterInputTemperature;
            if(recirculatingPumpWaterInputTemperatureLocal == DEVICE_DISCONNECTED_C)
            {
                sprintf(buffMain, "error");
            }
            else
            {
                sprintf(buffMain, "%6.2f`C", recirculatingPumpWaterInputTemperatureLocal);
            }

            if(recirculatingPumpWaterInputTemperatureLocal == DEVICE_DISCONNECTED_C || criticalTemperatureFreezeProtection)
            {
                fgColor = TFT_RED;
            }
            else if(!controlTaskRunning && (recirculatingPumpWaterInputTemperature <= start_temperature_requirement))
            {
                fgColor = TFT_ORANGE;
            }
            else if(controlTaskRunning)
            {
                fgColor = TFT_SKYBLUE;
            }
            else
            {
                fgColor = TFT_WHITE;
            }

            mainScreenTemp2Sprite.setTextColor(fgColor, LCD_MAIN_MENU_BOT_BG_COLOR);
            mainScreenTemp2Sprite.drawCentreString(buffMain, (240-(5*32))/2, 16, GFXFF);
            mainScreenTemp2Sprite.pushSprite(3*32, 320-32);
        }

        //Starting
        //Running
        //Stopped

        if(monitoringReady)
            xTaskNotifyGive(monitoringTaskHandle);
    }
}

void switchToLogMenu()
{
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    String log_lcd_info = "Log console - " + String(baud) + " baud";
    //lcd log screen
    tft.fillRect(0,320-16,240,320, TFT_DARKGREY);

    uint32_t debugBgColor;
    switch(LCD_DEBUG_LEVEL)
    {
        case ESP_LOG_NONE:
            debugBgColor = TFT_PURPLE;
            break;
        case ESP_LOG_ERROR:
            debugBgColor = TFT_RED;
            break;
        case ESP_LOG_WARN:
            debugBgColor = TFT_ORANGE;
            break;
        case ESP_LOG_INFO:
            debugBgColor = TFT_DARKGREEN;
            break;
        case ESP_LOG_DEBUG:
            debugBgColor = TFT_BLUE;
            break;
        case ESP_LOG_VERBOSE:
            debugBgColor = TFT_DARKGREY;
            break;
        default:
            debugBgColor = TFT_CYAN;
    }
    tft.fillRect(0,0,240,16, debugBgColor);
    tft.setTextColor(TFT_WHITE, debugBgColor);
    tft.drawCentreString(log_lcd_info,120,0,2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    setupScrollArea(TOP_FIXED_AREA, BOT_FIXED_AREA);
    for (byte i = 0; i<18; i++) blank[i]=0;

    tft.setTextColor(lastlogFGColor, lastlogBGColor);
    p_msg = msg;
    lcd_draw('\r');
    while(*p_msg != '\0')
    {
        lcd_draw(*p_msg);
        p_msg++;
    }
    if(esp_log_default_level == ESP_LOG_NONE || LCD_DEBUG_LEVEL == ESP_LOG_NONE)
    {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("Logging is disabled!",120,320/2,4);
    }
    else
    {
        lcdLogReady = true;
    }

    while(currentLCDMenu == LCD_MENU_LOG || keepLogScreen)
    {
        taskENTER_CRITICAL(&lcdloguptimeupdate_spinlock);
        updateUptimeCentreString(120, 320-16, 2, TFT_WHITE, TFT_DARKGREY);
        taskEXIT_CRITICAL(&lcdloguptimeupdate_spinlock);
        giveToMonitoring(1);
    }
}

bool giveToMonitoring(uint32_t delayMS)
{
    if(monitoringReady)
    {
        xTaskNotifyGive(monitoringTaskHandle);
        return true;
    }
    delay(delayMS);
    return false;
}

void updateUptimeCentreString(int32_t x, int32_t y, uint8_t font, uint32_t fgColor, uint32_t bgColor)
{
    sprintf(buffUptime, VERSION_SHORT " Uptime: %6u ms", (unsigned long) (esp_timer_get_time() / 1000ULL));
    uint32_t oldFGcolor = tft.textcolor;
    uint32_t oldBGColor = tft.textbgcolor;
    tft.setTextColor(fgColor, bgColor);
    tft.drawCentreString(buffUptime,120,320-16,2);
    tft.setTextColor(oldFGcolor, oldBGColor);
}

size_t LOG_TFT(esp_log_level_t loglevel, const char *tag, const char *fmt, ...)
{
    if(loglevel > CORE_DEBUG_LEVEL) return 0;
    if(loglevel > LCD_DEBUG_LEVEL) return 0;
    if(esp_log_level_get(tag) > esp_log_default_level) return 0;
    if(!lcdLogReady) return 0;
    if((currentLCDMenu != LCD_MENU_LOG) && !keepLogScreen) return 0;
    
    taskENTER_CRITICAL(&lcdlog_spinlock);

    char loglevelchr;

    uint32_t fgColor;
    uint32_t bgColor;

    switch(loglevel)
    {
        case ESP_LOG_NONE:
            return 0;
            break;
        case ESP_LOG_ERROR:
            loglevelchr = 'E';
            fgColor = TFT_RED;
            bgColor = TFT_BLACK;
            break;
        case ESP_LOG_WARN:
            loglevelchr = 'W';
            fgColor = TFT_YELLOW;
            bgColor = TFT_BLACK;
            break;
        case ESP_LOG_INFO:
            loglevelchr = 'I';
            fgColor = TFT_GREEN;
            bgColor = TFT_BLACK;
            break;
        case ESP_LOG_DEBUG:
            loglevelchr = 'D';
            fgColor = TFT_SKYBLUE;
            bgColor = TFT_BLACK;
            break;
        case ESP_LOG_VERBOSE:
            loglevelchr = 'V';
            fgColor = TFT_LIGHTGREY;
            bgColor = TFT_BLACK;
            break;
        default:
            fgColor = TFT_BLACK;
            bgColor = TFT_WHITE;
            return 0;
    }

    msg[4096-1] = '\0';

    int numm = sprintf(msg, "[%6u][%c]: [%s]\r  ->  ", (unsigned long) (esp_timer_get_time() / 1000ULL), loglevelchr, tag);
    
    va_list    args;
    va_start(args, fmt);

    int ret = vsnprintf(msg+numm, sizeof(msg)-numm, fmt, args); // do check return value

    va_end(args);
    msg[numm+ret] = '\0';

    uint32_t oldTextColor = tft.textcolor;
    uint32_t oldTextBGColor = tft.textbgcolor;
    
    tft.setTextColor(fgColor, bgColor);
    p_msg = msg;
    lcd_draw('\r');
    while(*p_msg != '\0')
    {
        lcd_draw(*p_msg);
        p_msg++;
    }
    lastlogBGColor = tft.textbgcolor;
    lastlogFGColor = tft.textcolor;
    tft.setTextColor(oldTextColor, oldTextBGColor);
    updateUptimeCentreString(120, 320-16, 2, TFT_WHITE, TFT_DARKGREY);
    taskEXIT_CRITICAL(&lcdlog_spinlock);
    return strlen(msg)-1;
}

void lcd_draw(char chr)
{
    if (chr == '\r' || chr == '\n' || xPos>231) {
      xPos = 0;
      yDraw = scroll_line(); // It can take 13ms to scroll and blank 16 pixel lines
    }
    if (chr  > 31 && chr  < 128) {
      xPos += tft.drawChar(chr ,xPos,yDraw,2);
      blank[(18+(yStart-TOP_FIXED_AREA)/TEXT_HEIGHT)%19]=xPos; // Keep a record of line lengths
    }
}

int scroll_line() 
{
    int yTemp = yStart; // Store the old yStart, this is where we draw the next line
    // Use the record of line lengths to optimise the rectangle size we need to erase the top line
    tft.fillRect(0,yStart,/*blank[(yStart-TOP_FIXED_AREA)/TEXT_HEIGHT]*/240,TEXT_HEIGHT, TFT_BLACK);

    // Change the top of the scroll area
    yStart+=TEXT_HEIGHT;
    // The value must wrap around as the screen memory is a circular buffer
    if (yStart >= YMAX - BOT_FIXED_AREA) yStart = TOP_FIXED_AREA + (yStart - YMAX + BOT_FIXED_AREA);
    // Now we can scroll the display
    scrollAddress(yStart);
    return  yTemp;
}

void disableScrollArea()
{
    setupScrollArea(0, 0);
    scrollAddress(0);
}

void setupScrollArea(uint16_t tfa, uint16_t bfa) 
{
    tft.writecommand(ST7789_VSCRDEF); // Vertical scroll definition
    tft.writedata(tfa >> 8);           // Top Fixed Area line count
    tft.writedata(tfa);
    tft.writedata((YMAX-tfa-bfa)>>8);  // Vertical Scrolling Area line count
    tft.writedata(YMAX-tfa-bfa);
    tft.writedata(bfa >> 8);           // Bottom Fixed Area line count
    tft.writedata(bfa);
}

void scrollAddress(uint16_t vsp) 
{
    tft.writecommand(ST7789_VSCRSADD); // Vertical scrolling pointer
    tft.writedata(vsp>>8);
    tft.writedata(vsp);
}