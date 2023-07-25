#pragma once

#include <Arduino.h>
#include <WiFi.h>


void WiFiEventSTAConnected(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiEventSTADisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiEventSTAGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
void WifiEventSTAGotIP6(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiEventAPSTAConnected(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiEventAPSTADisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiEventAPSTAIPAssigned(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiEventAPProbeReceived(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiEventAPGOTIP6(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiEventProvCredRecv(WiFiEvent_t event, WiFiEventInfo_t info);
void WiFiEventProvCredFailed(WiFiEvent_t event, WiFiEventInfo_t info);

void WiFiEvent(WiFiEvent_t event);
