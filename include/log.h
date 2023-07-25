#pragma once

#include <Arduino.h>
#include <lcdTask.h>

static portMUX_TYPE lcdlog_spinlock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE lcdloguptimeupdate_spinlock = portMUX_INITIALIZER_UNLOCKED;

extern uint64_t errorCount;

extern size_t LOG_TFT(esp_log_level_t loglevel, const char *tag, const char *fmt, ...);

#define LOG_ERROR(tag, format, ...)  { ESP_LOGE(tag, format, ##__VA_ARGS__); errorCount++; oldErrorDisplayTime = esp_timer_get_time(); LOG_TFT(ESP_LOG_ERROR, tag, format, ##__VA_ARGS__); }
#define LOG_WARNING(tag, format, ...)  { ESP_LOGW(tag, format, ##__VA_ARGS__); LOG_TFT(ESP_LOG_WARN, tag, format, ##__VA_ARGS__); }
#define LOG_INFO(tag, format, ...)  { ESP_LOGI(tag, format, ##__VA_ARGS__); LOG_TFT(ESP_LOG_INFO, tag, format, ##__VA_ARGS__); }
#define LOG_DEBUG(tag, format, ...) { ESP_LOGD(tag, format, ##__VA_ARGS__); LOG_TFT(ESP_LOG_DEBUG, tag, format, ##__VA_ARGS__); }
#define LOG_VERBOSE(tag, format, ...) { ESP_LOGV(tag, format, ##__VA_ARGS__); LOG_TFT(ESP_LOG_VERBOSE, tag, format, ##__VA_ARGS__); }

//#define BAUD 115200
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

//#define LOG_LCD_INFO "Log console - " STR(BAUD) " baud"