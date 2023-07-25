#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WifiEvents.h>
#include <FS.h>
#include <LittleFS.h>
#include <SimpleFTPServer.h>

#define currentTimeUL() (unsigned long) (esp_timer_get_time() / 1000ULL)


//#define WIFI_MAN_CONFIG_PORTAL_TIMEOUT 0

#define SERVER_PORT 81

//#define WIFI_MAN_PORT 80

extern AsyncWebServer webServer;
extern bool wifiManPortalActive;
void startWifiAndServer();
void wifiLoop();

void _callback(FtpOperation ftpOperation, unsigned int freeSpace, unsigned int totalSpace);
void _transferCallback(FtpTransferOperation ftpOperation, const char* name, unsigned int transferredSize);