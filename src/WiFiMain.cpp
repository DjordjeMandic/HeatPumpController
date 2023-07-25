#include <Arduino.h>
#include <log.h>
#include <WiFiManager.h>
#include <WifiMain.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <WifiEvents.h>
#include <PinDefinitions.h>
#include <config.h>
#include <FS.h>
#include <LittleFS.h>
#include <SimpleFTPServer.h>

static const char* TAG = "WiFi-Main";

//const char* ssidAp = "Heat Pump Controller";
//const char* passwordAp = "12345678";

unsigned long lastWifiTimeOut = 0;

bool wifiManPortalActive = true;

WiFiManager wm;
AsyncWebServer webServer(SERVER_PORT);

FtpServer ftpSrv;

extern bool configReady;

bool configUploadStarted = false;
bool configUploadTransferDone = false;
String str = config_filename;

void startWifiAndServer()
{
    pinMode(WEB_PORTAL_PIN, INPUT_PULLUP);
    pinMode(RESET_WIFI_SETTINGS_PIN, INPUT_PULLUP);
    LOG_INFO(TAG, "Starting WiFi");
    WiFi.disconnect(true);
    str.remove(0,1);
    WiFi.onEvent(WiFiEvent);
    WiFi.onEvent(WiFiEventSTAConnected,  WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(WiFiEventSTADisconnected,  WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent(WiFiEventSTAGotIP,  WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(WifiEventSTAGotIP6,  WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP6);
    WiFi.onEvent(WiFiEventAPSTAConnected,  WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent(WiFiEventAPSTADisconnected,  WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
    WiFi.onEvent(WiFiEventAPSTAIPAssigned,  WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
    WiFi.onEvent(WiFiEventAPProbeReceived,  WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED);
    WiFi.onEvent(WiFiEventAPGOTIP6,  WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_GOT_IP6);
    WiFi.onEvent(WiFiEventProvCredRecv,  WiFiEvent_t::ARDUINO_EVENT_PROV_CRED_RECV);
    WiFi.onEvent(WiFiEventProvCredFailed,  WiFiEvent_t::ARDUINO_EVENT_PROV_CRED_FAIL);

    WiFi.mode(WIFI_STA);

    wm.setConfigPortalBlocking(false);
    wm.setConfigPortalTimeout(wifi_man_config_portal_timeout_seconds);
    wm.setDarkMode(true);
    wm.setHttpPort(wifi_manPort);
    wm.setDebugOutput(true, "[      ][ ][WiFiManager.cpp:   ] wm(): [WifiManager] ");
    wm.setShowInfoErase(false);      // do not show erase button on info page
    wm.setAPClientCheck(true); // avoid timeout if client connected to softap
    
    LOG_INFO(TAG, "Starting WiFi Manager, port: %u, timeout: %u", wifi_manPort, wifi_man_config_portal_timeout_seconds);
    if(wm.autoConnect(ap_ssid, ap_pass)){
        LOG_INFO(TAG, "Successfully connected to network");
        if(digitalRead(WEB_PORTAL_PIN) == web_portal_active_state) wm.startWebPortal();
    }
    else {
        LOG_ERROR(TAG, "Couldnt automatically connect to network, timed out. Starting config portal.");
    }

    if(WiFi.status() == WL_CONNECTED)
    {
        LOG_INFO(TAG, "Successfully connected to %s , IP: %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    }
    else
    {
        LOG_ERROR(TAG, "Failed to connect");
    }

    AsyncElegantOTA.begin(&webServer, usrname, otapassword);    // Start AsyncElegantOTA
    webServer.begin();    

    LOG_INFO(TAG, "HTTP web server started at port %u", SERVER_PORT);

    LOG_INFO(TAG, "Starting FTP server");
    if(configReady)
    {
        ftpSrv.setCallback(_callback);
        ftpSrv.setTransferCallback(_transferCallback);
        ftpSrv.begin(usrname,otapassword);    //username, password for ftp.   (default 21, 50009 for PASV)
        LOG_INFO(TAG, "FTP server started");
    }
    else
    {
        LOG_ERROR(TAG, "Cant start FTP server, filesystem not ready");
    }
}

void wifiLoop()
{
    wm.process();
    ftpSrv.handleFTP();
    if(digitalRead(RESET_WIFI_SETTINGS_PIN) == wifi_reset_active_state)
    {
        LOG_WARNING(TAG, "Reseting wifi settings");
        wm.resetSettings();
        wm.startConfigPortal(ap_ssid, ap_pass);
        while(digitalRead(RESET_WIFI_SETTINGS_PIN) == wifi_reset_active_state);
        delay(250);
    }

    if(!(wifiManPortalActive = (wm.getWebPortalActive() || wm.getConfigPortalActive()))) 
        if(digitalRead(WEB_PORTAL_PIN) == web_portal_active_state) 
            wm.startWebPortal();

    if(configUploadTransferDone)
    {
        LOG_INFO(TAG, "Config updated via FTP, reading...");
        ftpSrv.handleFTP();
        delay(100);
        if(!readConfig())
        {
#if RESET_ON_FAILED_CONFIG_READ_AFTER_UPLOAD
            LOG_ERROR(TAG, "New config is corrupted, restarting");
            delay(100);
            ESP.restart();
#endif
            LOG_ERROR(TAG, "New config is corrupted, keeping current settings in RAM. Config will be restored to defaults on next boot unless current settings are manually saved");
        }
        configUploadTransferDone = false;
    }
}

void _callback(FtpOperation ftpOperation, unsigned int freeSpace, unsigned int totalSpace)
{
    switch (ftpOperation) 
    {
        case FTP_CONNECT:
            LOG_INFO(TAG, "FTP: Connected!");
            break;
        case FTP_DISCONNECT:
            LOG_WARNING(TAG, "FTP: Disconnected!");
            break;
        case FTP_FREE_SPACE_CHANGE:
            LOG_DEBUG(TAG, "FTP: Free space change, free %u of %u!", freeSpace, totalSpace);
            break;
        default:
            break;
    }
}

void _transferCallback(FtpTransferOperation ftpOperation, const char* name, unsigned int transferredSize)
{

    switch (ftpOperation) 
    {
        case FTP_UPLOAD_START:
            LOG_INFO(TAG, "FTP: Upload start File %s", name);
            if(str.equals(name))
            {
                configUploadStarted = true;
                configUploadTransferDone = false;
            }
            break;
        case FTP_UPLOAD:
            ESP_LOGD(TAG, "FTP: Upload of file %s byte %u", name, transferredSize);
            break;
        case FTP_DOWNLOAD_START:
            LOG_INFO(TAG, "FTP: Download start File %s", name);
            break;
        case FTP_DOWNLOAD:
            ESP_LOGD(TAG, "FTP: Download of file %s byte %u", name, transferredSize);
            break; 
        case FTP_TRANSFER_STOP:
            LOG_INFO(TAG, "FTP: Finish transfer! File %s", name);
            if(configUploadStarted && str.equals(name))
            {
                configUploadStarted = false;
                configUploadTransferDone = true;
            }
            break;
        case FTP_TRANSFER_ERROR:
            LOG_ERROR(TAG, "FTP: Transfer error! File %s", name);
            if(str.equals(name))
            {
                configUploadStarted = false;
                configUploadTransferDone = false;
            }
            break;
        default:
            break;
    }

    /* FTP_UPLOAD_START = 0,
    * FTP_UPLOAD = 1,
    *
    * FTP_DOWNLOAD_START = 2,
    * FTP_DOWNLOAD = 3,
    *
    * FTP_TRANSFER_STOP = 4,
    * FTP_DOWNLOAD_STOP = 4,
    * FTP_UPLOAD_STOP = 4,
    *
    * FTP_TRANSFER_ERROR = 5,
    * FTP_DOWNLOAD_ERROR = 5,
    * FTP_UPLOAD_ERROR = 5
    */
}