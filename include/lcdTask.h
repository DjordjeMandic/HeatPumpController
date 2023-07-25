#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>



extern TaskHandle_t lcdTaskHandle;

extern bool lcdLogReady;
extern bool param_config_entered;

#define LCD_MAIN_MENU_TOP_BG_COLOR 0x3186 // #303030
#define LCD_MAIN_MENU_BOT_BG_COLOR 0x3186
#define LCD_MAIN_MENU_BG_COLOR TFT_BLACK
#define LCD_MAIN_MENU_UPPER_LINE_COLOR TFT_PURPLE
#define LCD_MAIN_MENU_BOTTOM_LINE_COLOR TFT_PURPLE
#define LCD_MAIN_MENU_UPPER_GROUP_BG_COLOR 0x1082
#define LCD_MAIN_MENU_BOTTOM_GROUP_BG_COLOR 0x1082
#define LCD_MAIN_MENU_SEPARATOR_LINE_COLOR TFT_WHITE

#define LCD_ABOUT_MENU_BG_COLOR TFT_BLACK
#define LCD_ABOUT_MENU_TOP_BG_COLOR TFT_RED
#define LCD_ABOUT_MENU_BOT_BG_COLOR TFT_DARKGREY

#define LCD_PARAM_MENU_BG_COLOR TFT_BLACK
#define LCD_PARAM_MENU_TOP_BG_COLOR TFT_PURPLE
#define LCD_PARAM_MENU_BOT_BG_COLOR TFT_DARKGREY


#define SPRITE_ICON_SIZE 32

#define FORCE_UPDATE_INTERVAL_MS 333


extern int64_t oldErrorDisplayTime;

enum LCD_CURRENT_MENU
{
    LCD_MENU_MIN = 0,
    LCD_MENU_MAIN,
    LCD_MENU_PARAM,
    LCD_MENU_ABOUT,
    LCD_MENU_LOG,
    LCD_MENU_MAX
};
extern volatile int currentLCDMenu;

#define BOOT_LCD_MENU LCD_MENU_LOG

void lcdTask(void * parameter);
void lcd_draw(char chr);
void scrollAddress(uint16_t vsp);
void setupScrollArea(uint16_t tfa, uint16_t bfa);
void switchToLogMenu();
void switchToMainMenu();
void switchToAboutMenu();
void disableScrollArea();
void switchToParamMenu();
int scroll_line();
void printResetReason(Print& p, int reason);
void updateUptimeCentreString(int32_t x, int32_t y, uint8_t font, uint32_t fgColor, uint32_t bgColor);
bool giveToMonitoring(uint32_t delay = 1); //* xTaskNotifyGive monitoring task, if monitoringReady delays, returns monitoringReady