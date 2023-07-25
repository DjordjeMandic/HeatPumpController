#include <Arduino.h>
#include "FS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>  
#include <log.h>
#include <config.h>

static const char* TAG = "Config";

bool configReady = false;

unsigned long baud = CONFIG_DEFAULT_BAUD;
char ap_ssid[64] = CONFIG_DEFAULT_AP_SSID;
char ap_pass[64] = CONFIG_DEFAULT_AP_PASS;
char usrname[64] = CONFIG_DEFAULT_USRNAME;
char otapassword[64] = CONFIG_DEFAULT_OTAPASSWORD;

//uint16_t serverPort = CONFIG_DEFAULT_SERVERPORT;
uint16_t wifi_manPort = CONFIG_DEFAULT_WIFI_MANPORT;
//uint32_t wifi_connection_timeout = CONFIG_DEFAULT_WIFI_CONNECTION_TIMEOUT;
uint32_t wifi_man_config_portal_timeout_seconds = CONFIG_DEFAULT_WIFI_MAN_CONFIG_PORTAL_TIMEOUT;

uint32_t low_pressure_timeout_seconds = CONFIG_DEFAULT_LOW_PRESSURE_TIMEOUT_SECONDS;
uint32_t water_pump_stop_delay_seconds = CONFIG_DEFAULT_WATER_PUMP_STOP_DELAY_SECONDS;
uint32_t water_pump_keep_running_time_seconds = CONFIG_DEFAULT_WATER_PUMP_KEEP_RUNNING_TIME_SECONDS;
uint32_t compressor_start_delay_seconds = CONFIG_DEFAULT_COMPRESSOR_START_DELAY_SECONDS;
uint32_t flow_switch_timeout_seconds = CONFIG_DEFAULT_FLOW_SWITCH_TIMEOUT_SECONDS;

float freeze_protection_temperature_limit = CONFIG_DEFAULT_FREEZE_PROTECTION_TEMPERATURE_LIMIT;
float start_temperature_requirement = CONFIG_DEFAULT_START_TEMPERATURE_REQUIREMENT;
float first_pair_shutdown_temp = CONFIG_DEFAULT_FIRST_PAIR_SHUTDOWN_TEMP;
float shutdown_temp_delta = CONFIG_DEFAULT_SHUTDOWN_TEMP_DELTA;

bool wifi_reset_active_state = CONFIG_DEFAULT_WIFI_RESET_ACTIVE_STATE;
bool web_portal_active_state = CONFIG_DEFAULT_WEB_PORTAL_ACTIVE_STATE;
bool main_switch_active_state = CONFIG_DEFAULT_MAIN_SWITCH_ACTIVE_STATE;
bool low_pressure_active_state = CONFIG_DEFAULT_LOW_PRESSURE_SWITCH_ACTIVE_STATE;
bool high_pressure_active_state = CONFIG_DEFAULT_HIGH_PRESSURE_SWITCH_ACTIVE_STATE;
bool water_pump_dm_protection_active_state = CONFIG_DEFAULT_WATER_PUMP_DM_PROTECTION_ACTIVE_STATE;
bool flow_switch_active_state = CONFIG_DEFAULT_FLOW_SWITCH_ACTIVE_STATE;


static portMUX_TYPE config_spinlock = portMUX_INITIALIZER_UNLOCKED;

bool initConfig()
{
    if(configReady) return true;

    LOG_INFO(TAG, "Initializing...");

    configReady = false;
    
    LOG_DEBUG(TAG, "Initializing LittleFS");
    if(!LittleFS.begin(false))
    {
        LOG_ERROR(TAG, "LittleFS mount failed, starting format.");
        if(!LittleFS.begin(true))
        {
            LOG_ERROR(TAG, "LittleFS mount and format failed! Aborting.");
            return false;
        }
        else
        {
            LOG_WARNING(TAG, "Formatting");
        }
    }

    if(!LittleFS.begin(false))
    {
        LOG_ERROR(TAG, "LittleFS mount failed. Aborting.");
        return false;
    }

    LOG_INFO(TAG, "LittleFS mounted.");

    File file = LittleFS.open(config_filename, "r");

    if(!file || file.isDirectory())
    {
        file.close();
        LOG_ERROR(TAG, "Failed to open \"%s\", restoring default config.", config_filename.c_str());
        restoreDefaultConfig();
    }
    else
    {
        file.close();
        LOG_INFO(TAG, "Found config file.");
        //while(file.available()) Serial.print((char)file.read());
    }

    if(!readConfig())
    {
        LOG_ERROR(TAG, "Error reading config, restoring defaults.");
        if(restoreDefaultConfig())
        {
            LOG_ERROR(TAG, "Fatal error, failed restoring defaults. Aborting.");
            configReady = false;
            return configReady;
        }
    }

    configReady = true;

#if DUMP_JSON_CONFIG_TO_SERIAL_ON_INIT
    dumpConfig();
#endif

    return configReady;
}

bool readConfig()
{
    LOG_INFO(TAG, "Reading configuration");
    
    String content = readFile(config_filename);
#if DUMP_JSON_CONFIG_TO_SERIAL_ON_READ
    ESP_LOGI(TAG, "Config file contents:\n%s", content.c_str());
#endif
    unsigned int configFileSize = content.length();
    
    if(configFileSize < CONFIG_JSON_DOC_SIZE_MIN)
    {
        LOG_ERROR(TAG, "Config file \"%s\" is too small, %u bytes", config_filename.c_str(), configFileSize);
        return false;
    }
    else if(configFileSize > CONFIG_JSON_DOC_SIZE)
    {
        LOG_ERROR(TAG, "Config file \"%s\" is too big, %u bytes", config_filename.c_str(), configFileSize);
        return false;
    }
    
    DynamicJsonDocument doc(CONFIG_JSON_DOC_SIZE);

    LOG_INFO(TAG, "Got config file \"%s\", %u bytes", config_filename.c_str(), configFileSize);

    auto err = deserializeJson(doc, content);
    if(err)
    {
        LOG_ERROR(TAG, "Error deserializing config file \"%s\"", config_filename.c_str());
        return false;
    }
    
    if(!(doc.containsKey(GET_VARIABLE_NAME(baud)) &&
        doc.containsKey(GET_VARIABLE_NAME(ap_ssid)) &&
        doc.containsKey(GET_VARIABLE_NAME(ap_pass)) &&
        doc.containsKey(GET_VARIABLE_NAME(usrname)) &&
        doc.containsKey(GET_VARIABLE_NAME(otapassword)) &&
        //doc.containsKey(GET_VARIABLE_NAME(serverPort)) &&
        doc.containsKey(GET_VARIABLE_NAME(wifi_manPort)) &&
        //doc.containsKey(GET_VARIABLE_NAME(wifi_connection_timeout)) &&
        doc.containsKey(GET_VARIABLE_NAME(wifi_man_config_portal_timeout_seconds)) &&
        doc.containsKey(GET_VARIABLE_NAME(low_pressure_timeout_seconds)) &&
        doc.containsKey(GET_VARIABLE_NAME(water_pump_stop_delay_seconds)) &&
        doc.containsKey(GET_VARIABLE_NAME(water_pump_keep_running_time_seconds)) &&
        doc.containsKey(GET_VARIABLE_NAME(compressor_start_delay_seconds)) &&
        doc.containsKey(GET_VARIABLE_NAME(flow_switch_timeout_seconds)) &&
        doc.containsKey(GET_VARIABLE_NAME(freeze_protection_temperature_limit)) &&
        doc.containsKey(GET_VARIABLE_NAME(start_temperature_requirement)) &&
        doc.containsKey(GET_VARIABLE_NAME(first_pair_shutdown_temp)) &&
        doc.containsKey(GET_VARIABLE_NAME(shutdown_temp_delta)) &&
        doc.containsKey(GET_VARIABLE_NAME(wifi_reset_active_state)) &&
        doc.containsKey(GET_VARIABLE_NAME(web_portal_active_state)) &&
        doc.containsKey(GET_VARIABLE_NAME(main_switch_active_state)) &&
        doc.containsKey(GET_VARIABLE_NAME(low_pressure_active_state)) &&
        doc.containsKey(GET_VARIABLE_NAME(high_pressure_active_state)) &&
        doc.containsKey(GET_VARIABLE_NAME(water_pump_dm_protection_active_state)) &&
        doc.containsKey(GET_VARIABLE_NAME(flow_switch_active_state))))
    {
        LOG_ERROR(TAG, "Missing keys in config file \"%s\"", config_filename.c_str());
        return false;
    }

    baud = doc[GET_VARIABLE_NAME(baud)];

    doc[GET_VARIABLE_NAME(ap_ssid)].as<String>().toCharArray(ap_ssid, 64);
    doc[GET_VARIABLE_NAME(ap_pass)].as<String>().toCharArray(ap_pass, 64);
    doc[GET_VARIABLE_NAME(usrname)].as<String>().toCharArray(usrname, 64);
    doc[GET_VARIABLE_NAME(otapassword)].as<String>().toCharArray(otapassword, 64);

    bool saveConfigNeeded = false;

    //serverPort = doc[GET_VARIABLE_NAME(serverPort)];
    uint16_t manPort = doc[GET_VARIABLE_NAME(wifi_manPort)].as<uint16_t>();
    if(manPort >= CONFIG_PORT_MIN && manPort <= CONFIG_PORT_MAX)
    {
        wifi_manPort = manPort;
    }
    else
    {
        saveConfigNeeded |= true;
        LOG_ERROR(TAG, "%s out of range. Val: %u, range = [%u - %u], restoring default %u", GET_VARIABLE_NAME(wifi_manPort), manPort, CONFIG_PORT_MIN, CONFIG_PORT_MAX, CONFIG_DEFAULT_WIFI_MANPORT);
        wifi_manPort = CONFIG_DEFAULT_WIFI_MANPORT;
    }
    
    //wifi_connection_timeout = doc[GET_VARIABLE_NAME(wifi_connection_timeout)].as<uint32_t>();

    wifi_man_config_portal_timeout_seconds = doc[GET_VARIABLE_NAME(wifi_man_config_portal_timeout_seconds)].as<uint32_t>();

    low_pressure_timeout_seconds = doc[GET_VARIABLE_NAME(low_pressure_timeout_seconds)].as<uint32_t>();

    water_pump_stop_delay_seconds = doc[GET_VARIABLE_NAME(water_pump_stop_delay_seconds)].as<uint32_t>();
    water_pump_keep_running_time_seconds = doc[GET_VARIABLE_NAME(water_pump_keep_running_time_seconds)].as<uint32_t>();
    compressor_start_delay_seconds = doc[GET_VARIABLE_NAME(compressor_start_delay_seconds)].as<uint32_t>();

    flow_switch_timeout_seconds = doc[GET_VARIABLE_NAME(flow_switch_timeout_seconds)].as<uint32_t>();
    
    float temp =  doc[GET_VARIABLE_NAME(freeze_protection_temperature_limit)].as<float>();
    if(temp < CONFIG_TEMPERAUTRE_MIN)
    {
        saveConfigNeeded |= true;
        LOG_ERROR(TAG, "%s out of range. Val: %u, range = [%u - inf], restoring default %u", GET_VARIABLE_NAME(freeze_protection_temperature_limit), manPort, CONFIG_TEMPERAUTRE_MIN, CONFIG_DEFAULT_FREEZE_PROTECTION_TEMPERATURE_LIMIT);
        freeze_protection_temperature_limit = CONFIG_DEFAULT_FREEZE_PROTECTION_TEMPERATURE_LIMIT;
    }
    else
    {
        freeze_protection_temperature_limit = temp;
    }

    temp = doc[GET_VARIABLE_NAME(start_temperature_requirement)].as<float>();
    if(temp < CONFIG_TEMPERAUTRE_MIN)
    {
        saveConfigNeeded |= true;
        LOG_ERROR(TAG, "%s out of range. Val: %u, range = [%u - inf], restoring default %u", GET_VARIABLE_NAME(wifstart_temperature_requirementi_manPort), manPort, CONFIG_TEMPERAUTRE_MIN, CONFIG_DEFAULT_START_TEMPERATURE_REQUIREMENT);
        start_temperature_requirement = CONFIG_DEFAULT_FREEZE_PROTECTION_TEMPERATURE_LIMIT;
    }
    else
    {
        start_temperature_requirement = temp;
    }

    temp = doc[GET_VARIABLE_NAME(first_pair_shutdown_temp)].as<float>();
    if(temp < CONFIG_TEMPERAUTRE_MIN)
    {
        saveConfigNeeded |= true;
        LOG_ERROR(TAG, "%s out of range. Val: %u, range = [%u - inf], restoring default %u", GET_VARIABLE_NAME(first_pair_shutdown_temp), manPort, CONFIG_TEMPERAUTRE_MIN, CONFIG_DEFAULT_FIRST_PAIR_SHUTDOWN_TEMP);
        first_pair_shutdown_temp = CONFIG_DEFAULT_FIRST_PAIR_SHUTDOWN_TEMP;
    }
    else
    {
        first_pair_shutdown_temp = temp;
    }

    temp = doc[GET_VARIABLE_NAME(shutdown_temp_delta)].as<float>();
    if(temp > CONFIG_TEMP_DELTA_MAX)
    {
        saveConfigNeeded |= true;
        LOG_ERROR(TAG, "%s out of range. Val: %u, range = [-inf - %u], restoring default %u", GET_VARIABLE_NAME(shutdown_temp_delta), manPort, CONFIG_TEMP_DELTA_MAX, CONFIG_DEFAULT_SHUTDOWN_TEMP_DELTA);
        shutdown_temp_delta = CONFIG_DEFAULT_SHUTDOWN_TEMP_DELTA;
    }
    else
    {
        shutdown_temp_delta = temp;
    }

    wifi_reset_active_state = doc[GET_VARIABLE_NAME(wifi_reset_active_state)].as<bool>();
    web_portal_active_state = doc[GET_VARIABLE_NAME(web_portal_active_state)].as<bool>();
    main_switch_active_state = doc[GET_VARIABLE_NAME(main_switch_active_state)].as<bool>();
    low_pressure_active_state = doc[GET_VARIABLE_NAME(low_pressure_active_state)].as<bool>();
    high_pressure_active_state = doc[GET_VARIABLE_NAME(high_pressure_active_state)].as<bool>();
    water_pump_dm_protection_active_state = doc[GET_VARIABLE_NAME(water_pump_dm_protection_active_state)].as<bool>();
    flow_switch_active_state = doc[GET_VARIABLE_NAME(flow_switch_active_state)].as<bool>();

    if(saveConfigNeeded)
    {
        LOG_WARNING(TAG, "Saving config due to some keys being out of range.");
        saveConfig();
    }

    LOG_INFO(TAG, "Done reading configuration");
    return true;
}

bool saveConfig()
{
    LOG_INFO(TAG, "Saving confing");
    DynamicJsonDocument doc(CONFIG_JSON_DOC_SIZE);

    doc[GET_VARIABLE_NAME(baud)] = baud;

    doc[GET_VARIABLE_NAME(ap_ssid)] = String(ap_ssid);

    doc[GET_VARIABLE_NAME(ap_pass)] = String(ap_pass);

    doc[GET_VARIABLE_NAME(usrname)] = String(usrname);

    doc[GET_VARIABLE_NAME(otapassword)] = String(otapassword);

    //doc[GET_VARIABLE_NAME(serverPort)] = serverPort = CONFIG_DEFAULT_SERVERPORT;
    doc[GET_VARIABLE_NAME(wifi_manPort)] = wifi_manPort;
    //doc[GET_VARIABLE_NAME(wifi_connection_timeout)] = wifi_connection_timeout = CONFIG_DEFAULT_WIFI_CONNECTION_TIMEOUT;
    doc[GET_VARIABLE_NAME(wifi_man_config_portal_timeout_seconds)] = wifi_man_config_portal_timeout_seconds;
    doc[GET_VARIABLE_NAME(low_pressure_timeout_seconds)] = low_pressure_timeout_seconds;
    doc[GET_VARIABLE_NAME(water_pump_keep_running_time_seconds)] = water_pump_keep_running_time_seconds;
    doc[GET_VARIABLE_NAME(water_pump_stop_delay_seconds)] = water_pump_stop_delay_seconds;
    doc[GET_VARIABLE_NAME(compressor_start_delay_seconds)] = compressor_start_delay_seconds;
    doc[GET_VARIABLE_NAME(flow_switch_timeout_seconds)] = flow_switch_timeout_seconds;
    doc[GET_VARIABLE_NAME(freeze_protection_temperature_limit)] = freeze_protection_temperature_limit;
    doc[GET_VARIABLE_NAME(start_temperature_requirement)] = start_temperature_requirement;
    doc[GET_VARIABLE_NAME(first_pair_shutdown_temp)] = first_pair_shutdown_temp;
    doc[GET_VARIABLE_NAME(shutdown_temp_delta)] = shutdown_temp_delta;
    doc[GET_VARIABLE_NAME(wifi_reset_active_state)] = wifi_reset_active_state;
    doc[GET_VARIABLE_NAME(web_portal_active_state)] = web_portal_active_state;
    doc[GET_VARIABLE_NAME(main_switch_active_state)] = main_switch_active_state;
    doc[GET_VARIABLE_NAME(low_pressure_active_state)] = low_pressure_active_state;
    doc[GET_VARIABLE_NAME(high_pressure_active_state)] = high_pressure_active_state;
    doc[GET_VARIABLE_NAME(water_pump_dm_protection_active_state)] = water_pump_dm_protection_active_state;
    doc[GET_VARIABLE_NAME(flow_switch_active_state)] = flow_switch_active_state;

    String tmp = "";

    LOG_DEBUG(TAG, "Serializing json, prettify: %s", SERIALIZE_JSON_PRETTY ? "true" : "false");
    if(SERIALIZE_JSON_PRETTY)
    {
        serializeJsonPretty(doc, tmp);
    }
    else
    {
        serializeJson(doc, tmp);
    }

    if(writeFile(config_filename, tmp))
    {
        LOG_INFO(TAG, "Config saved to \"%s\"", config_filename.c_str());
#if DUMP_JSON_CONFIG_TO_SERIAL_ON_SAVE
        dumpConfig();
#endif
        return true;
    }
    else
    {
        LOG_ERROR(TAG, "Failed to save defaults to \"%s\"", config_filename.c_str());
        return false;
    }
}

bool restoreDefaultConfig()
{
    LOG_WARNING(TAG, "Restoring default confing");
    DynamicJsonDocument doc(CONFIG_JSON_DOC_SIZE);

    doc[GET_VARIABLE_NAME(baud)] = baud = CONFIG_DEFAULT_BAUD;

    doc[GET_VARIABLE_NAME(ap_ssid)] = CONFIG_DEFAULT_AP_SSID;
    String(CONFIG_DEFAULT_AP_SSID).toCharArray(ap_ssid, 64);

    doc[GET_VARIABLE_NAME(ap_pass)] = CONFIG_DEFAULT_AP_PASS;
    String(CONFIG_DEFAULT_AP_PASS).toCharArray(ap_pass, 64);

    doc[GET_VARIABLE_NAME(usrname)] = CONFIG_DEFAULT_USRNAME;
    String(CONFIG_DEFAULT_USRNAME).toCharArray(usrname, 64);

    doc[GET_VARIABLE_NAME(otapassword)] = CONFIG_DEFAULT_OTAPASSWORD;
    String(CONFIG_DEFAULT_OTAPASSWORD).toCharArray(otapassword, 64);

    //doc[GET_VARIABLE_NAME(serverPort)] = serverPort = CONFIG_DEFAULT_SERVERPORT;
    doc[GET_VARIABLE_NAME(wifi_manPort)] = wifi_manPort = CONFIG_DEFAULT_WIFI_MANPORT;
    //doc[GET_VARIABLE_NAME(wifi_connection_timeout)] = wifi_connection_timeout = CONFIG_DEFAULT_WIFI_CONNECTION_TIMEOUT;
    doc[GET_VARIABLE_NAME(wifi_man_config_portal_timeout_seconds)] = wifi_man_config_portal_timeout_seconds = CONFIG_DEFAULT_WIFI_MAN_CONFIG_PORTAL_TIMEOUT;
    doc[GET_VARIABLE_NAME(low_pressure_timeout_seconds)] = low_pressure_timeout_seconds = CONFIG_DEFAULT_LOW_PRESSURE_TIMEOUT_SECONDS;
    doc[GET_VARIABLE_NAME(water_pump_keep_running_time_seconds)] = water_pump_keep_running_time_seconds = CONFIG_DEFAULT_WATER_PUMP_KEEP_RUNNING_TIME_SECONDS;
    doc[GET_VARIABLE_NAME(water_pump_stop_delay_seconds)] = water_pump_stop_delay_seconds = CONFIG_DEFAULT_WATER_PUMP_STOP_DELAY_SECONDS;
    doc[GET_VARIABLE_NAME(compressor_start_delay_seconds)] = compressor_start_delay_seconds = CONFIG_DEFAULT_COMPRESSOR_START_DELAY_SECONDS;
    doc[GET_VARIABLE_NAME(flow_switch_timeout_seconds)] = flow_switch_timeout_seconds = CONFIG_DEFAULT_FLOW_SWITCH_TIMEOUT_SECONDS;
    doc[GET_VARIABLE_NAME(freeze_protection_temperature_limit)] = freeze_protection_temperature_limit = CONFIG_DEFAULT_FREEZE_PROTECTION_TEMPERATURE_LIMIT;
    doc[GET_VARIABLE_NAME(start_temperature_requirement)] = start_temperature_requirement = CONFIG_DEFAULT_START_TEMPERATURE_REQUIREMENT;
    doc[GET_VARIABLE_NAME(first_pair_shutdown_temp)] = first_pair_shutdown_temp = CONFIG_DEFAULT_FIRST_PAIR_SHUTDOWN_TEMP;
    doc[GET_VARIABLE_NAME(shutdown_temp_delta)] = shutdown_temp_delta = CONFIG_DEFAULT_SHUTDOWN_TEMP_DELTA;
    doc[GET_VARIABLE_NAME(wifi_reset_active_state)] = wifi_reset_active_state = CONFIG_DEFAULT_WIFI_RESET_ACTIVE_STATE;
    doc[GET_VARIABLE_NAME(web_portal_active_state)] = web_portal_active_state = CONFIG_DEFAULT_WEB_PORTAL_ACTIVE_STATE;
    doc[GET_VARIABLE_NAME(main_switch_active_state)] = main_switch_active_state = CONFIG_DEFAULT_MAIN_SWITCH_ACTIVE_STATE;
    doc[GET_VARIABLE_NAME(low_pressure_active_state)] = low_pressure_active_state = CONFIG_DEFAULT_LOW_PRESSURE_SWITCH_ACTIVE_STATE;
    doc[GET_VARIABLE_NAME(high_pressure_active_state)] = high_pressure_active_state = CONFIG_DEFAULT_HIGH_PRESSURE_SWITCH_ACTIVE_STATE;
    doc[GET_VARIABLE_NAME(water_pump_dm_protection_active_state)] = water_pump_dm_protection_active_state = CONFIG_DEFAULT_WATER_PUMP_DM_PROTECTION_ACTIVE_STATE;
    doc[GET_VARIABLE_NAME(flow_switch_active_state)] = flow_switch_active_state = CONFIG_DEFAULT_FLOW_SWITCH_ACTIVE_STATE;

    String tmp = "";

    LOG_DEBUG(TAG, "Serializing json, prettify: %s", SERIALIZE_JSON_PRETTY ? "true" : "false");
    if(SERIALIZE_JSON_PRETTY)
    {
        serializeJsonPretty(doc, tmp);
    }
    else
    {
        serializeJson(doc, tmp);
    }

    if(writeFile(config_filename, tmp))
    {
        LOG_INFO(TAG, "Default config restored and saved to \"%s\"", config_filename.c_str());
        return true;
    }
    else
    {
        LOG_ERROR(TAG, "Failed to save defaults to \"%s\"", config_filename.c_str());
        return false;
    }
}

size_t writeFile(String filename, String content)
{
    LOG_DEBUG(TAG, "Writing file \"%s\"", filename.c_str());
    File file =  LittleFS.open(filename, "w");

    if(!file || file.isDirectory())
    {
        LOG_ERROR(TAG, "Failed to open \"%s\"", filename.c_str());
        file.close();
        return 0;
    }

    size_t written = file.print(content);
    file.close();

    if(written)
    {
        LOG_DEBUG(TAG, "File write to \"%s\" completed, %zu bytes", filename.c_str(), written);
    }
    else
    {
        LOG_ERROR(TAG, "Failed writing to \"%s\"", filename.c_str());
    }

    return written;
}

String readFile(String filename)
{
    LOG_DEBUG(TAG, "Reading file \"%s\"", filename.c_str());
    File file =  LittleFS.open(filename, "r");

    if(!file || file.isDirectory())
    {
        LOG_ERROR(TAG, "Failed to open \"%s\"", filename.c_str());
        file.close();
        return "";
    }

    String fileText = "";

    while(file.available())
    {
        char intRead = file.read();
        fileText += intRead;
    }

    file.close();

    LOG_DEBUG(TAG, "Read from file \"%s\" completed, %u bytes", filename.c_str(), fileText.length());

    return fileText;
}

bool dumpConfig()
{
    if(initConfig())
    {
        LOG_INFO(TAG, "Dumping config...");
        File file = LittleFS.open(config_filename, "r");

        if(!file || file.isDirectory())
        {
            file.close();
            LOG_ERROR(TAG, "Failed to open \"%s\"", config_filename.c_str());
            return false;
        }
        else
        {
            LOG_INFO(TAG, "Found config file:");

            size_t size = file.size();
            uint8_t *ptr = (uint8_t*)malloc(size+1);

            ptr[size] = '\0';

            if(ptr == NULL)
            {
                LOG_ERROR(TAG, "Failed to allocate %zu bytes for config dumping", size);
                return false;
            }
            
            file.read(ptr, size);

            if(file.available()) LOG_WARNING(TAG, "Config dump might be incomplete");
            file.close();

            if(size == 0)
            {
                LOG_WARNING(TAG, "Config file is empty.");
            }
            else
            {
                ESP_LOGI(TAG, "\"%s\" file content:\n%s", config_filename.c_str(), ptr);
            }
            free(ptr);
            return true;
        }
    }
    else
    {
        LOG_ERROR(TAG, "Cannot dump config content, config not initialzied");
        return false;
    }
}