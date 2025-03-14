#ifndef _LI_WIFI_ESP_
#define _LI_WIFI_ESP_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "esp_netif.h"
#include "esp_wifi_types.h"

/** @brief Defines access point's name. Default value: esp32. Run 'make menuconfig' to setup your own value or replace here by a string */
#define DEFAULT_AP_SSID 					CONFIG_DEFAULT_AP_SSID

/** @brief Defines access point's password.
 *	@warning In the case of an open access point, the password must be a null string "" or "\0" if you want to be verbose but waste one byte.
 *	In addition, the AP_AUTHMODE must be WIFI_AUTH_OPEN
 */
#define DEFAULT_AP_PASSWORD 				CONFIG_DEFAULT_AP_PASSWORD

/** @brief Defines access point's channel.
 *  Channel selection is only effective when not connected to another AP.
 *  Good practice for minimal channel interference to use
 *  For 20 MHz: 1, 6 or 11 in USA and 1, 5, 9 or 13 in most parts of the world
 *  For 40 MHz: 3 in USA and 3 or 11 in most parts of the world
 */
#define DEFAULT_AP_CHANNEL 					CONFIG_DEFAULT_AP_CHANNEL

/** @brief Defines visibility of the access point. 0: visible AP. 1: hidden */
#define DEFAULT_AP_SSID_HIDDEN 		        0

/** @brief Defines access point's bandwidth.
 *  Value: WIFI_BW_HT20 for 20 MHz  or  WIFI_BW_HT40 for 40 MHz
 *  20 MHz minimize channel interference but is not suitable for
 *  applications with high data speeds
 */
#define DEFAULT_AP_BANDWIDTH 					WIFI_BW_HT20

/** @brief Defines if esp32 shall run both AP + STA when connected to another AP.
 *  Value: 0 will have the own AP always on (APSTA mode)
 *  Value: 1 will turn off own AP when connected to another AP (STA only mode when connected)
 *  Turning off own AP when connected to another AP minimize channel interference and increase throughput
 */
#define DEFAULT_STA_ONLY 					1

/** @brief Defines if wifi power save shall be enabled.
 *  Value: WIFI_PS_NONE for full power (wifi modem always on)
 *  Value: WIFI_PS_MODEM for power save (wifi modem sleep periodically)
 *  Note: Power save is only effective when in STA only mode
 */
#define DEFAULT_STA_POWER_SAVE 				WIFI_PS_NONE

/**
 * @brief Defines the maximum size of a SSID name. 32 is IEEE standard.
 * @warning limit is also hard coded in wifi_config_t. Never extend this value.
 */
#define MAX_SSID_SIZE						32

/**
 * @brief Defines the maximum size of a WPA2 passkey. 64 is IEEE standard.
 * @warning limit is also hard coded in wifi_config_t. Never extend this value.
 */
#define MAX_PASSWORD_SIZE					64

/** @brief Defines access point's maximum number of clients. Default: 4 */
#define DEFAULT_AP_MAX_CONNECTIONS		CONFIG_DEFAULT_AP_MAX_CONNECTIONS

/** @brief Defines access point's beacon interval. 100ms is the recommended default. */
#define DEFAULT_AP_BEACON_INTERVAL 			CONFIG_DEFAULT_AP_BEACON_INTERVAL

/**
 * @brief defines the minimum length of an access point password running on WPA2
 */
#define WPA2_MINIMUM_PASSWORD_LENGTH		8

/** @brief Defines the access point's default IP address. Default: "10.10.0.1 */
#define DEFAULT_AP_IP						CONFIG_DEFAULT_AP_IP

/** @brief Defines the access point's gateway. This should be the same as your IP. Default: "10.10.0.1" */
#define DEFAULT_AP_GATEWAY					CONFIG_DEFAULT_AP_GATEWAY

/** @brief Defines the access point's netmask. Default: "255.255.255.0" */
#define DEFAULT_AP_NETMASK					CONFIG_DEFAULT_AP_NETMASK

#define WIFI_MANAGER_NVS_NAMESPACE "espwifimgr"
#define WIFI_AUTHMODE_BLOB_SIZE 30
#define WIFI_SOFTAP_NAME_BLOB_SIZE 32
#define AUTHMODE_WPA2_ENTERPRISE "wpa_enterprise"
#define AUTHMODE_WPA2_PERSONAL "standard"

struct wifi_settings_t{
	uint8_t ap_ssid[MAX_SSID_SIZE];
	uint8_t ap_pwd[MAX_PASSWORD_SIZE];
	uint8_t ap_channel;
	uint8_t ap_ssid_hidden;
	wifi_bandwidth_t ap_bandwidth;
	bool sta_only;
	wifi_ps_type_t sta_power_save;
	bool sta_static_ip;
	esp_netif_ip_info_t sta_static_ip_config;
};
extern struct wifi_settings_t wifi_settings;

typedef enum
{
    NETWORK_DHCO = 0,
    NETWORK_MANUAL
}Netif_DHCP_et;

typedef enum {
    NETWORK_WPA2_PERSONAL = 0,
    NETWORK_WPA2_ENTERPRISE,
    NETWORK_OFFLINE
}Netif_Connection_type_et;

typedef struct 
{
    Netif_DHCP_et dhcp;
    char ipv4[16];
    char mask[16];
    char gateway[16];
    char dns[16];
    char dns_secondary[16];
    char dns_fallback[16];
}Netif_DHCP_Config_st;

typedef void (*page_function)(void);
typedef struct 
{
    Netif_DHCP_Config_st dhcp_config;
    Netif_Connection_type_et auth_type;
    char auth_type_str[30];
    char ssid[32];
    char pswd[64];
    char user[64];
    uint8_t channel;
    page_function pages;
    char softap_name[32];
}Netif_Wifi_st;

typedef enum
{
    NETIF_INTERFACE_STARTED = 0,
    NETIF_INTERFACE_CONNECTED = 1,
    NETIF_INTERFACE_DISCONNECTED = 3,
    NETIF_INTERFACE_GOT_ACCESSED = 127
}Netif_event_et;

typedef struct 
{
    uint8_t cmd;
    uint8_t *payload;
    uint16_t payload_len;
}Netif_Message_st;

typedef struct 
{
    char name[32];
    int rssi;
    Netif_Connection_type_et auth;
}Netif_scan_result_st;

/**
 * @brief initialize wifi stack
 * 
 * @param config struct with wifi general config
 * @param callback callback function to receive wifi events
 */
void NETIF_Init(Netif_Wifi_st *config_device, void (*callback)(Netif_event_et, Netif_Message_st*));

/**
 * @brief deinitialize wifi stack
 * 
 * @return true when success
 * @return false when fail
 */
bool NETIF_Deinit(void);

/**
 * @brief start scan request
 * 
 * @param result return scanned devices
 * @return int number of scanned devices
 */
int NETIF_ScanNetwork(Netif_scan_result_st *result);

/**
 * @brief Write on flash new config
 * 
 * @param newConfig new config to be written
 */
esp_err_t NETIF_SetConfig(Netif_Wifi_st newConfig);

/**
 * @brief Force a disconnect to try reconnect with the new configs.
 * 
 * @param config 
 * @return esp_err_t 
 */
esp_err_t NETIF_TryConnect(Netif_Wifi_st config);

/**
 * @brief Stop the connection retry, commonly used when a scan procedure needs to be done.
 * 
 */
void NETIF_StopTryConnect(void);

/**
 * @brief Get the netif object
 * 
 * @return esp_netif_t* 
 */
esp_netif_t *NETIF_GetEspNetifSTA();

/**
 * @brief Set a fallback credentials connection to try to connect with them if the current saved config does not connect
 * 
 * @param ssid ssid of the network  
 * @param password password of the newtork 
 */
void NETIF_SetFallBackNetwork(char *ssid, char* password);

/**
 * @brief Get connection status
 * 
 * @return true if connected
 * @return false if not connected
 */
bool NETIF_GetConnectionStatus();

char* NETIF_GetSSID();
int NETIF_GetRSSI();

/**
 * @brief Get the current wifi configuration
 * 
 * @param currentConfig 
 * @return esp_err_t 
 */
esp_err_t NETIF_GetConfig(Netif_Wifi_st *currentConfig);

#ifdef __cplusplus
}
#endif

#endif