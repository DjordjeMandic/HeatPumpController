#pragma once

#define FORMAT_LITTLEFS_IF_FAILED true

#define SERIALIZE_JSON_PRETTY true

#define CONFIG_JSON_DOC_SIZE 2048
#define CONFIG_JSON_DOC_SIZE_MIN 600

const String config_filename = "/config.json";

#define GET_VARIABLE_NAME(Variable) (#Variable)

#define CONFIG_DEFAULT_BAUD 115200
#define CONFIG_DEFAULT_AP_SSID "HeatPumpController"
#define CONFIG_DEFAULT_AP_PASS "12345678"
#define CONFIG_DEFAULT_USRNAME "admin"
#define CONFIG_DEFAULT_OTAPASSWORD "admin"
//#define CONFIG_DEFAULT_SERVERPORT 81
#define CONFIG_DEFAULT_WIFI_MANPORT 80
//#define CONFIG_DEFAULT_WIFI_CONNECTION_TIMEOUT 15000
#define CONFIG_DEFAULT_WIFI_MAN_CONFIG_PORTAL_TIMEOUT 0
#define CONFIG_DEFAULT_WIFI_RESET_ACTIVE_STATE LOW
#define CONFIG_DEFAULT_WEB_PORTAL_ACTIVE_STATE LOW
#define CONFIG_DEFAULT_MAIN_SWITCH_ACTIVE_STATE LOW
#define CONFIG_DEFAULT_LOW_PRESSURE_SWITCH_ACTIVE_STATE HIGH
#define CONFIG_DEFAULT_HIGH_PRESSURE_SWITCH_ACTIVE_STATE HIGH
#define CONFIG_DEFAULT_WATER_PUMP_DM_PROTECTION_ACTIVE_STATE LOW
#define CONFIG_DEFAULT_FLOW_SWITCH_ACTIVE_STATE LOW
#define CONFIG_DEFAULT_FREEZE_PROTECTION_TEMPERATURE_LIMIT 5.0f
#define CONFIG_DEFAULT_START_TEMPERATURE_REQUIREMENT 11.0f
#define CONFIG_DEFAULT_FIRST_PAIR_SHUTDOWN_TEMP 9.0f
#define CONFIG_DEFAULT_SHUTDOWN_TEMP_DELTA -1.0f
#define CONFIG_DEFAULT_FLOW_SWITCH_TIMEOUT_SECONDS 10
#define CONFIG_DEFAULT_LOW_PRESSURE_TIMEOUT_SECONDS 30
#define CONFIG_DEFAULT_COMPRESSOR_START_DELAY_SECONDS 10
#define CONFIG_DEFAULT_WATER_PUMP_STOP_DELAY_SECONDS 5
#define CONFIG_DEFAULT_WATER_PUMP_KEEP_RUNNING_TIME_SECONDS 30

#define CONFIG_PORT_MIN 1
#define CONFIG_PORT_MAX 65000

#define CONFIG_DELAY_MIN 0
#define CONFIG_DELAY_MAX 60

#define CONFIG_TEMPERAUTRE_MIN 0.0f
#define CONFIG_TEMP_DELTA_MAX 0


extern unsigned long baud;
extern char ap_ssid[64];
extern char ap_pass[64];
extern char usrname[64];
extern char otapassword[64];

extern uint16_t serverPort;
extern uint16_t wifi_manPort;
extern uint32_t wifi_connection_timeout;
extern uint32_t wifi_man_config_portal_timeout_seconds;

extern uint32_t low_pressure_timeout_seconds;
extern uint32_t water_pump_stop_delay_seconds;
extern uint32_t water_pump_keep_running_time_seconds;
extern uint32_t compressor_start_delay_seconds;
extern uint32_t flow_switch_timeout_seconds;

extern float freeze_protection_temperature_limit;
extern float start_temperature_requirement;
extern float first_pair_shutdown_temp;
extern float shutdown_temp_delta;

extern bool wifi_reset_active_state;
extern bool web_portal_active_state;
extern bool main_switch_active_state;
extern bool low_pressure_active_state;
extern bool high_pressure_active_state;
extern bool water_pump_dm_protection_active_state;
extern bool flow_switch_active_state;


bool initConfig();
bool readConfig();
bool saveConfig();
bool restoreDefaultConfig();
String readFile(String filename);
size_t writeFile(String filename, String content);
bool dumpConfig();