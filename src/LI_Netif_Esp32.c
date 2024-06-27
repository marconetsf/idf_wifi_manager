/* LI_Netif_Esp32
*
*   Autor: Marcone Tenório da Silva Filho
*   Data:  14/02/2022
*/

/* *********************************** *
 *                                     *
 *              include                *
 *                                     *
 * *********************************** */

#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wpa2.h"
//#include "esp_efuse.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_sync.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"

#include "LI_Netif_Esp32.h"

/* *********************************** *
 *                                     *
 *              defines                *
 *                                     *
 * *********************************** */

#define WIFI_CONNECTED_BIT            BIT0
#define WIFI_FAIL_BIT                 BIT1
#define NETWORK_MAXIMUM_CONNECT_RETRY INT_MAX
#define NETWORK_SCAN_MAX_DEVICES	  15

/* *********************************** *
 *                                     *
 *              prototypes             *
 *                                     *
 * *********************************** */

static bool wifi_manager_fetch_wifi_sta_config();
static void NETIF_Callback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

/* *********************************** *
 *                                     *
 *              Global Vars            *
 *                                     *
 * *********************************** */

/**
 * The actual WiFi settings in use
 */
struct wifi_settings_t wifi_settings = {
	.ap_ssid = DEFAULT_AP_SSID,
	.ap_pwd = DEFAULT_AP_PASSWORD,
	.ap_channel = DEFAULT_AP_CHANNEL,
	.ap_ssid_hidden = DEFAULT_AP_SSID_HIDDEN,
	.ap_bandwidth = DEFAULT_AP_BANDWIDTH,
	.sta_only = DEFAULT_STA_ONLY,
	.sta_power_save = DEFAULT_STA_POWER_SAVE,
	.sta_static_ip = 0,
};

Netif_Wifi_st fallbackNetwork = {.user = "empty", .pswd = "empty"};
const char wifi_manager_nvs_namespace[] = "espwifimgr";
static EventGroupHandle_t s_wifi_event_group;
static const char *TAG        = "WIFI_TAG";
static int s_retry_num        = 0;
static void (*Network_Callback)(Netif_event_et, Netif_Message_st*);
static Netif_Message_st network_message;
wifi_config_t* wifi_manager_config_sta = NULL;
Netif_Wifi_st *config;
static esp_netif_t *esp_netif_ap = NULL;
static esp_netif_t *esp_netif_sta = NULL;
bool has_connection = false;


/* *********************************** *
 *                                     *
 *          public functions           *
 *                                     *
 * *********************************** */

void NETIF_Init(Netif_Wifi_st *config_device, void (*callback)())
{
    config = (Netif_Wifi_st*)malloc(sizeof(Netif_Wifi_st));
    Network_Callback = callback;
    esp_err_t esp_err;
    size_t sz;
    nvs_handle handle;
    // Recuperar dados do nvs e utilizar no fluxo da inicialização

    nvs_flash_init();
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_sync_create());
	config->auth_type = NETWORK_WPA2_PERSONAL; // AuthMode default é o wpa2 personal
	sprintf(config->ssid, "default");
	sprintf(config->pswd, "default");

	wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));

    if (wifi_manager_fetch_wifi_sta_config())
    {
        memcpy(config->ssid, wifi_manager_config_sta->sta.ssid, 32);
        memcpy(config->pswd, wifi_manager_config_sta->sta.password, 64);
        
        printf("Credenciais:\n");
        printf("- User: %s\n", config->user);
        printf("- PSWD: %s\n", config->pswd);
        printf("- SSID: %s\n", config->ssid);
        printf("- Mode: %s\n", config->auth_type == NETWORK_WPA2_PERSONAL ? "WPA2_PERSONAL" : "WPA2_ENTERPRISE");
    }

    s_wifi_event_group = xEventGroupCreate();
    esp_netif_ap = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_t instance_give_ip;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_sta = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &NETIF_Callback, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &NETIF_Callback, NULL, &instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &NETIF_Callback, NULL, &instance_give_ip));


    wifi_config_t wifi_config_sta = {
    .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                        .capable = true,
                        .required = false
                        },
            },
    };
    memcpy(wifi_config_sta.sta.ssid, (uint8_t*)config->ssid, 32);
    memcpy (wifi_config_sta.sta.password, (uint8_t*)config->pswd, 64);

    wifi_config_t ap_config = {
		.ap = {
			.ssid_len = 0,
			.channel = wifi_settings.ap_channel,
			.ssid_hidden = wifi_settings.ap_ssid_hidden,
			.max_connection = DEFAULT_AP_MAX_CONNECTIONS,
			.beacon_interval = DEFAULT_AP_BEACON_INTERVAL,
		},
	};
	uint8_t mac[6];
	esp_efuse_mac_get_default(mac);
	sprintf((char*)wifi_settings.ap_ssid, "%s_%X%X%X", "LIAP", mac[3], mac[4], mac[5]);
	memcpy(ap_config.ap.ssid, wifi_settings.ap_ssid, 32);

    /* if the password lenght is under 8 char which is the minium for WPA2, the access point starts as open */
	if (strlen((char *)wifi_settings.ap_pwd) < WPA2_MINIMUM_PASSWORD_LENGTH)
	{
		ap_config.ap.authmode = WIFI_AUTH_OPEN;
		memset(ap_config.ap.password, 0x00, 64);
	}
	else
	{
		ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
		memcpy(ap_config.ap.password, wifi_settings.ap_pwd, 64);
	}

    if(config->auth_type == NETWORK_WPA2_ENTERPRISE)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)config->user, strlen(config->user)));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_sta_wpa2_ent_set_username((uint8_t *)config->user, strlen(config->user)));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_sta_wpa2_ent_set_password((uint8_t *)config->pswd, strlen((char *)config->pswd)));
        ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_enable());

        printf("user_wpa_enterprise: %s\n", config->user);
        printf("ssid: %s\n", config->ssid);
		printf("pswd: %s\n", config->pswd);

    }

    /* DHCP AP configuration */
	esp_netif_dhcps_stop(esp_netif_ap); /* DHCP client/server must be stopped before setting new IP information. */
	esp_netif_ip_info_t ap_ip_info;
	memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));
	inet_pton(AF_INET, DEFAULT_AP_IP, &ap_ip_info.ip);
	inet_pton(AF_INET, DEFAULT_AP_GATEWAY, &ap_ip_info.gw);
	inet_pton(AF_INET, DEFAULT_AP_NETMASK, &ap_ip_info.netmask);
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(esp_netif_ap));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config) );
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) 
    {
		printf("Conectado na rede\n");
        //Network_Callback(NETIF_INTERFACE_CONNECTED, &network_message);
    }
    else if (bits & WIFI_FAIL_BIT) 
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%.32s, password:%.64s", config->ssid, config->pswd);
    } 
    else 
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    // vEventGroupDelete(s_wifi_event_group);
}

bool NETIF_Deinit(void)
{
    // ToDo: Implemente netif deinit procedure
    return false;
}

/* *********************************** *
 *                                     *
 *          private functions          *
 *                                     *
 * *********************************** */

static void NETIF_Callback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{   
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        esp_wifi_connect();
		Network_Callback(NETIF_INTERFACE_STARTED, &network_message);
		ESP_LOGI(TAG, "wifi_init_sta finished.");
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
		xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		Network_Callback(NETIF_INTERFACE_DISCONNECTED, &network_message);
        if (s_retry_num < NETWORK_MAXIMUM_CONNECT_RETRY) 
        {
			wifi_sta_list_t wifi_sta_list;
			esp_wifi_ap_get_sta_list(&wifi_sta_list);
			if (wifi_sta_list.num > 0 && s_retry_num > 2)
			{
				return;
			}
            esp_wifi_connect();
            s_retry_num++;
			
            ESP_LOGI(TAG, "retry to connect to the AP");
			has_connection = false;
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
		Network_Callback(NETIF_INTERFACE_CONNECTED, &network_message);
		has_connection = true;
    }
	else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED)
	{
		ip_event_ap_staipassigned_t* event = (ip_event_ap_staipassigned_t*) event_data;
		ESP_LOGE(TAG, "assigned ip:" IPSTR, IP2STR(&event->ip));
		Network_Callback(NETIF_INTERFACE_GOT_ACCESSED, &network_message);
	}
}

static bool wifi_manager_fetch_wifi_sta_config()
{
	nvs_handle handle;
	esp_err_t esp_err;
	if(nvs_sync_lock( portMAX_DELAY ))
    {
		esp_err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READONLY, &handle);

		if(esp_err != ESP_OK){
			nvs_sync_unlock();
			return false;
		}

		if(wifi_manager_config_sta == NULL){
			wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
		}
		memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));

		/* allocate buffer */
		size_t sz = sizeof(wifi_settings);
		uint8_t *buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
		memset(buff, 0x00, sizeof(sz));

		/* ssid */
		sz = sizeof(wifi_manager_config_sta->sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.ssid, buff, sz);

		/* password */
		sz = sizeof(wifi_manager_config_sta->sta.password);
		esp_err = nvs_get_blob(handle, "password", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.password, buff, sz);

		/* settings */
		sz = sizeof(wifi_settings);
		esp_err = nvs_get_blob(handle, "settings", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(&wifi_settings, buff, sz);

        uint8_t *tmp_mod = (uint8_t*)malloc(WIFI_AUTHMODE_BLOB_SIZE);
        esp_err = nvs_get_blob(handle, "mode", tmp_mod, &sz);

        // Verifico se possui o authmode salvo
        if(esp_err == ESP_OK || esp_err == ESP_ERR_NVS_NOT_FOUND)
        {   
            // Caso seja o modo wpa2 enterprise seto na struct de configuração
            if (strcmp(AUTHMODE_WPA2_ENTERPRISE, (char*)tmp_mod) == 0) 
            {
                config->auth_type = NETWORK_WPA2_ENTERPRISE;

                uint8_t* tmp_user = (uint8_t*)malloc(64);
                memset(tmp_user,0x00,64);
                sz = sizeof(config->user);
		
		        esp_err = nvs_get_blob(handle, "user", tmp_user, &sz);

                if (esp_err == ESP_OK || esp_err == ESP_ERR_NVS_NOT_FOUND)
                {
                    strcpy(config->user, (char*)tmp_user);
                }

                uint8_t* tmp_pswd = (uint8_t*)malloc(64);
                memset(tmp_user,0x00,64);
                sz = sizeof(config->pswd);
		
		        esp_err = nvs_get_blob(handle, "password", tmp_pswd, &sz);

                if (esp_err == ESP_OK || esp_err == ESP_ERR_NVS_NOT_FOUND)
                {
                    strcpy(config->pswd, (char*)tmp_pswd);
                }

                printf("Modo WPA2 Enterprise:\n");
                printf("- User: %s\n", config->user);
                printf("- PSWD: %s\n", config->pswd);
            }
        }

		free(buff);
		nvs_close(handle);
		nvs_sync_unlock();


		ESP_LOGI(TAG, "wifi_manager_fetch_wifi_sta_config: ssid:%s password:%s",wifi_manager_config_sta->sta.ssid,wifi_manager_config_sta->sta.password);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_ssid:%s",wifi_settings.ap_ssid);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_pwd:%s",wifi_settings.ap_pwd);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_channel:%i",wifi_settings.ap_channel);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_hidden (1 = yes):%i",wifi_settings.ap_ssid_hidden);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz)%i",wifi_settings.ap_bandwidth);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_only (0 = APSTA, 1 = STA when connected):%i",wifi_settings.sta_only);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_power_save (1 = yes):%i",wifi_settings.sta_power_save);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_static_ip (0 = dhcp client, 1 = static ip):%i",wifi_settings.sta_static_ip);

		return wifi_manager_config_sta->sta.ssid[0] != '\0';


	}
	else{
		return false;
	}

}

bool NETIF_GetConnectionStatus()
{
	return has_connection;
}

char *NETIF_GetSSID()
{
	if (wifi_manager_config_sta != NULL)
	{
		return (char*)wifi_manager_config_sta->sta.ssid;
	} else {
		return NULL;
	}
}

int NETIF_ScanNetwork(Netif_scan_result_st *result)
{    
	wifi_ap_record_t* ap_info = (wifi_ap_record_t*)malloc(NETWORK_SCAN_MAX_DEVICES*sizeof(wifi_ap_record_t));
    
	uint16_t number = NETWORK_SCAN_MAX_DEVICES;

    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(TAG, "Total APs scanned = %u", number);

    for (int i = 0; (i < number); i++)
    {
        strcpy(result[i].name, (char*)ap_info[i].ssid);
        printf("SSID: %s\n", result[i].name);
        result[i].rssi = ap_info[i].rssi;
        printf("RSSI: %d\n", result[i].rssi);

        switch(ap_info[i].authmode)
        {
            case WIFI_AUTH_WPA2_ENTERPRISE:
            {
                result[i].auth = NETWORK_WPA2_ENTERPRISE;
            }
            break;

            default:
            {
                result[i].auth = NETWORK_WPA2_PERSONAL;
            }
            break;
        }
        printf("AuthMode: %s\n", result[i].auth == NETWORK_WPA2_PERSONAL ? "WPA2_Personal" : "WPA2_Enterprise");
    }

    return number;
}

esp_err_t NETIF_SetConfig(Netif_Wifi_st newConfig)
{
    nvs_handle handle;
	esp_err_t esp_err = ESP_OK;
	size_t sz;

	/* variables used to check if write is really needed */
	wifi_config_t tmp_conf; // Crio uma configuração temporária para recuperar na flash e comparar com a que temos atual
	struct wifi_settings_t tmp_settings; // Precisamos também da struct completa do wifimanager pois em certo ponto armazenamos toda a struct de uma vez
	memset(&tmp_conf, 0x00, sizeof(tmp_conf));
	memset(&tmp_settings, 0x00, sizeof(tmp_settings));
	bool change = false;

    memcpy(wifi_manager_config_sta->sta.ssid, newConfig.ssid, sizeof(newConfig.ssid));
    memcpy(wifi_manager_config_sta->sta.password, newConfig.pswd, sizeof(newConfig.pswd));

	ESP_LOGI(TAG, "About to save config to flash!!");
	ESP_LOGI(TAG, "SSID: %s", wifi_manager_config_sta->sta.ssid);
	ESP_LOGI(TAG, "Password: %s", wifi_manager_config_sta->sta.password);
	ESP_LOGI(TAG, "mode: %d", newConfig.auth_type);
	ESP_LOGI(TAG, "user: %s", newConfig.user);

	if (wifi_manager_config_sta && nvs_sync_lock(portMAX_DELAY))
	{
		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK)
		{
			nvs_sync_unlock();
			return esp_err;
		}

		//Aloco a memória para receber a informação do modo de autenticação
		uint8_t* tmp_mod = (uint8_t*)malloc(30);
		memset(tmp_mod,0x00,30);
		sz = sizeof(newConfig.auth_type_str);
		
		esp_err = nvs_get_blob(handle, "mode", tmp_mod, &sz);
		printf("tmp_mod: %s esp_err: %d\n", (char *)tmp_mod, esp_err);
		if ((esp_err == ESP_OK || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp((char *)tmp_mod, newConfig.auth_type_str) != 0)
		{
			printf("Salvando Mode: %s\n", newConfig.auth_type_str);
			esp_err = nvs_set_blob(handle, "mode", (uint8_t*)newConfig.auth_type_str, 30);
			if (esp_err != ESP_OK)
			{
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;
		}
		
		// Aloco a memória para receber a informação do usuário (usuário utilizado para a conexão wpa2enterprise)
		uint8_t* tmp_user = (uint8_t*)malloc(64);
		memset(tmp_user,0x00,64);
		sz = sizeof(newConfig.user);
		
		esp_err = nvs_get_blob(handle, "user", tmp_user, &sz);
		printf("tmp_user: %s esp_err: %d\n", (char *)tmp_user, esp_err);
		if ((esp_err == ESP_OK || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp((char *)tmp_user, newConfig.user) != 0)
		{
			printf("Salvando User: %s\n", newConfig.user);
			esp_err = nvs_set_blob(handle, "user", (uint8_t*)newConfig.user, 64);
			if (esp_err != ESP_OK)
			{
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;
		}

		sz = sizeof(tmp_conf.sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", tmp_conf.sta.ssid, &sz);
		if ((esp_err == ESP_OK || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp((char *)tmp_conf.sta.ssid, (char *)wifi_manager_config_sta->sta.ssid) != 0)
		{
			/* different ssid or ssid does not exist in flash: save new ssid */
			printf("Salvando SSID: %s\n", wifi_manager_config_sta->sta.ssid);
			esp_err = nvs_set_blob(handle, "ssid", wifi_manager_config_sta->sta.ssid, 32);
			if (esp_err != ESP_OK)
			{
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: ssid:%s", wifi_manager_config_sta->sta.ssid);
		}

		sz = sizeof(tmp_conf.sta.password);
		esp_err = nvs_get_blob(handle, "password", tmp_conf.sta.password, &sz);
		if ((esp_err == ESP_OK || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp((char *)tmp_conf.sta.password, (char *)wifi_manager_config_sta->sta.password) != 0)
		{
			printf("Salvando password: %s\n", wifi_manager_config_sta->sta.password);
			/* different password or password does not exist in flash: save new password */
			esp_err = nvs_set_blob(handle, "password", wifi_manager_config_sta->sta.password, 64);
			if (esp_err != ESP_OK)
			{
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: password:%s", wifi_manager_config_sta->sta.password);
		}

		sz = sizeof(tmp_settings);
		esp_err = nvs_get_blob(handle, "settings", &tmp_settings, &sz);
		if ((esp_err == ESP_OK || esp_err == ESP_ERR_NVS_NOT_FOUND) &&
			(strcmp((char *)tmp_settings.ap_ssid, (char *)wifi_settings.ap_ssid) != 0 ||
			 strcmp((char *)tmp_settings.ap_pwd, (char *)wifi_settings.ap_pwd) != 0 ||
			 tmp_settings.ap_ssid_hidden != wifi_settings.ap_ssid_hidden ||
			 tmp_settings.ap_bandwidth != wifi_settings.ap_bandwidth ||
			 tmp_settings.sta_only != wifi_settings.sta_only ||
			 tmp_settings.sta_power_save != wifi_settings.sta_power_save ||
			 tmp_settings.ap_channel != wifi_settings.ap_channel))
		{
			printf("Salvando toda a struct de settings padrão do WiFiManager\n");
			esp_err = nvs_set_blob(handle, "settings", &wifi_settings, sizeof(wifi_settings));
			if (esp_err != ESP_OK)
			{
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;

			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_ssid: %s", wifi_settings.ap_ssid);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_pwd: %s", wifi_settings.ap_pwd);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_channel: %i", wifi_settings.ap_channel);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_hidden (1 = yes): %i", wifi_settings.ap_ssid_hidden);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz): %i", wifi_settings.ap_bandwidth);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_only (0 = APSTA, 1 = STA when connected): %i", wifi_settings.sta_only);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_power_save (1 = yes): %i", wifi_settings.sta_power_save);
		}

		if (change)
		{
			esp_err = nvs_commit(handle);
		}
		else
		{
			ESP_LOGI(TAG, "Wifi config was not saved to flash because no change has been detected.");
		}

		if (esp_err != ESP_OK)
			return esp_err;

		nvs_close(handle);
		nvs_sync_unlock();
	}
	else
	{
		ESP_LOGE(TAG, "wifi_manager_save_sta_config failed to acquire nvs_sync mutex");
	}

    return esp_err;
}

esp_err_t NETIF_GetConfig(Netif_Wifi_st *currentConfig)
{
	memcpy(currentConfig, config, sizeof(Netif_Wifi_st));
	return ESP_OK;
}

esp_err_t NETIF_TryConnect(Netif_Wifi_st config)
{	
	esp_err_t esp_err = ESP_OK;
	ESP_LOGW("TRY","Tentando reconectar na nova rede");
	
	wifi_config_t wifi_config_sta = {
    .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                        .capable = true,
                        .required = false
                        },
            },
    };
    memcpy(wifi_config_sta.sta.ssid, (uint8_t*)config.ssid, 32);
    memcpy (wifi_config_sta.sta.password, (uint8_t*)config.pswd, 64);

	if(config.auth_type == NETWORK_WPA2_ENTERPRISE)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)config.user, strlen(config.user)));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_sta_wpa2_ent_set_username((uint8_t *)config.user, strlen(config.user)));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_sta_wpa2_ent_set_password((uint8_t *)config.pswd, strlen((char *)config.pswd)));
        ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_enable());

        printf("user_wpa_enterprise: %s\n", config.user);
        printf("ssid: %s\n", config.ssid);
		printf("pswd: %s\n", config.pswd);

    } else {
		ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_disable());
	}

	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));

	s_retry_num = 0;
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_disconnect());
	NETIF_Callback(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL);

	return esp_err;
}

void NETIF_StopTryConnect(void)
{
	s_retry_num = NETWORK_MAXIMUM_CONNECT_RETRY;
	ESP_LOGI("NETIF", "Retry number max: %s", s_retry_num == NETWORK_MAXIMUM_CONNECT_RETRY ? "TRUE" : "FALSE");
}

esp_netif_t *NETIF_GetEspNetifSTA()
{
	return esp_netif_sta;
}

void NETIF_SetFallBackNetwork(char *ssid, char* password)
{
	if (ssid == NULL || password == NULL) return;

	strcpy(fallbackNetwork.ssid, ssid);
	strcpy(fallbackNetwork.pswd, password);
	strcpy(fallbackNetwork.user, "empty");
	strcpy(fallbackNetwork.auth_type_str, AUTHMODE_WPA2_PERSONAL);
	fallbackNetwork.auth_type = 0;
}

int NETIF_GetRSSI(void)
{
	wifi_ap_record_t ap_info;
	esp_wifi_sta_get_ap_info(&ap_info);
	return ap_info.rssi;
}