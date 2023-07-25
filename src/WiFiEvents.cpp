#include <WifiEvents.h>
#include <log.h>
#include <WifiMain.h>

static const char* TAG_WIFI = "WiFi-Event";


void WiFiEventSTAConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO(TAG_WIFI, "Connected to access point. SSID: %s , BSSID %02X:%02X:%02X:%02X:%02X:%02X , Channel %u , Auth Mode %u",
                (const char *)(info.wifi_sta_connected.ssid),
                info.wifi_sta_connected.bssid[0],
                info.wifi_sta_connected.bssid[1],
                info.wifi_sta_connected.bssid[2],
                info.wifi_sta_connected.bssid[3],
                info.wifi_sta_connected.bssid[4],
                info.wifi_sta_connected.bssid[5],
                info.wifi_sta_connected.channel,
                info.wifi_sta_connected.authmode);
}

void WiFiEventSTADisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_ERROR(TAG_WIFI, "WiFi lost connection. SSID: %s , BSSID %02X:%02X:%02X:%02X:%02X:%02X , Reason: %u, RSSI: %d", 
                (const char *)(info.wifi_sta_disconnected.ssid),
                info.wifi_sta_disconnected.bssid[0],
                info.wifi_sta_disconnected.bssid[1],
                info.wifi_sta_disconnected.bssid[2],
                info.wifi_sta_disconnected.bssid[3],
                info.wifi_sta_disconnected.bssid[4],
                info.wifi_sta_disconnected.bssid[5],
                info.wifi_sta_disconnected.reason,
                info.wifi_sta_disconnected.rssi);
}

void WiFiEventSTAGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO(TAG_WIFI, "Obtained IPv4 address: " IPSTR " , Netmask: " IPSTR " , Gateway: " IPSTR, 
                                            IP2STR(&info.got_ip.ip_info.ip), IP2STR(&info.got_ip.ip_info.netmask), IP2STR(&info.got_ip.ip_info.gw));
}

void WifiEventSTAGotIP6(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO(TAG_WIFI, "Obtained IPv6 address: " IPV6STR " , IP Index: %d, Zone: %d", IPV62STR(info.got_ip6.ip6_info.ip), info.got_ip6.ip_index, info.got_ip6.ip6_info.ip.zone);
}

void WiFiEventAPSTAConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO(TAG_WIFI, "WiFi Client connected. MAC: %02X:%02X:%02X:%02X:%02X:%02X , aid: %u , mesh_child: %s",
                            info.wifi_ap_staconnected.mac[0],
                            info.wifi_ap_staconnected.mac[1],
                            info.wifi_ap_staconnected.mac[2],
                            info.wifi_ap_staconnected.mac[3],
                            info.wifi_ap_staconnected.mac[4],
                            info.wifi_ap_staconnected.mac[5],
                            info.wifi_ap_staconnected.aid,
                            info.wifi_ap_staconnected.is_mesh_child ? "yes" : "no");
}

void WiFiEventAPSTADisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_WARNING(TAG_WIFI, "WiFi Client disconnected. MAC: %02X:%02X:%02X:%02X:%02X:%02X , aid: %u , mesh_child: %s",
                            info.wifi_ap_stadisconnected.mac[0],
                            info.wifi_ap_stadisconnected.mac[1],
                            info.wifi_ap_stadisconnected.mac[2],
                            info.wifi_ap_stadisconnected.mac[3],
                            info.wifi_ap_stadisconnected.mac[4],
                            info.wifi_ap_stadisconnected.mac[5],
                            info.wifi_ap_stadisconnected.aid,
                            info.wifi_ap_stadisconnected.is_mesh_child ? "yes" : "no");
}

void WiFiEventAPSTAIPAssigned(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO(TAG_WIFI, "Assigned IPv4 address to client: " IPSTR, IP2STR(&info.wifi_ap_staipassigned.ip));
}

void WiFiEventAPProbeReceived(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_DEBUG(TAG_WIFI, "Received probe request. RSSI: %d , MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                            info.wifi_ap_probereqrecved.rssi,
                            info.wifi_ap_probereqrecved.mac[0],
                            info.wifi_ap_probereqrecved.mac[1],
                            info.wifi_ap_probereqrecved.mac[2],
                            info.wifi_ap_probereqrecved.mac[3],
                            info.wifi_ap_probereqrecved.mac[4],
                            info.wifi_ap_probereqrecved.mac[5]
                            );
}

void WiFiEventAPGOTIP6(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO(TAG_WIFI, "Obtained IPv6 address:: " IPV6STR " , IP Index: %d, Zone: %d", IPV62STR(info.got_ip6.ip6_info.ip), info.got_ip6.ip_index, info.got_ip6.ip6_info.ip.zone);
}

void WiFiEventProvCredRecv(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_INFO(TAG_WIFI, "Provisioned Credentials for SSID: %s", (const char *)info.prov_cred_recv.ssid);
}

void WiFiEventProvCredFailed(WiFiEvent_t event, WiFiEventInfo_t info)
{
    LOG_ERROR(TAG_WIFI, "Provisioning Failed: Reason : %s", (info.prov_fail_reason == WIFI_PROV_STA_AUTH_ERROR)?"Authentication Failed":"AP Not Found");
}

void WiFiEvent(WiFiEvent_t event)
{
    LOG_VERBOSE(TAG_WIFI, "Event: %d", event);
    switch (event) {
        case ARDUINO_EVENT_WIFI_READY: 
            LOG_INFO(TAG_WIFI, "WiFi Interface ready");
            break;

        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            LOG_DEBUG(TAG_WIFI, "Completed scan for access points");
            break;

        case ARDUINO_EVENT_WIFI_STA_START:
            LOG_INFO(TAG_WIFI, "WiFi Client started");
            break;

        case ARDUINO_EVENT_WIFI_STA_STOP:
            LOG_WARNING(TAG_WIFI, "WiFi Client stopped");
            break;

        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
            LOG_WARNING(TAG_WIFI, "WiFi Auth mode changed");
            break;

        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            LOG_WARNING(TAG_WIFI, "Lost IP address and IP address is reset to 0");
            break;

        case ARDUINO_EVENT_WIFI_AP_START:
            LOG_INFO(TAG_WIFI, "WiFi access point started");
            break;

        case ARDUINO_EVENT_WIFI_AP_STOP:
            LOG_WARNING(TAG_WIFI, "WiFi access point stopped");
            break;

        case ARDUINO_EVENT_PROV_CRED_SUCCESS:
            LOG_DEBUG(TAG_WIFI, "WiFi Provisioned Credentials successfully.");
            break;

        default: 
            break;
    }
}