/* APP_Network 
*
*   Autor: Marcone Tenório da Silva Filho
*   Data:  12/06/2022
*/

/* *********************************** *
 *                                     *
 *              includes               *
 *                                     *
 * *********************************** */

#include <stdint.h>
#include <string.h>

#include "sys/param.h"
#include "nvs_flash.h"

#include "esp_http_server.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"

#include "APP_WebServer_Esp32.h"
#include "LI_Netif_Esp32.h"

/* *********************************** *
 *                                     *
 *              global vars            *
 *                                     *
 * *********************************** */

char default_login[] = "admin";
char default_password[] = "password";
bool flag_isLogged = false;

bool post_reception(httpd_req_t *req, char *buf, int size_buf);
void redirect(httpd_req_t *req, const char *data);

// uint16_t func_type_cmp(cJSON* func_type);
// uint16_t data_type_size(cJSON* data_type);

WB_Server_st web_server;
const char TAG[] = "WS";

esp_err_t index_handler(httpd_req_t *req)
{
    if (flag_isLogged) redirect(req, "/home.html");
    else redirect(req, "/login.html");
    return ESP_OK;
}

esp_err_t login_handler(httpd_req_t *req)
{
    extern const unsigned char loginhtml_start[] asm("_binary_login_html_start");
    extern const unsigned char loginhtml_end[] asm("_binary_login_html_end");
    const size_t loginhtml_size = (loginhtml_end - loginhtml_start);

    httpd_resp_send_chunk(req, (const char*)loginhtml_start, loginhtml_size);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t home_handler(httpd_req_t *req)
{
    if (!flag_isLogged)
    {
        redirect(req, "/login.html");
        return ESP_OK;
    }

    NETIF_StopTryConnect(); // Um procedimento de scan vai ser feito assim que essa página iniciar, é interessante parar as tentativas de reconexão nesse momento

    extern const unsigned char homehtml_start[] asm("_binary_home_html_start");
    extern const unsigned char homehtml_end[] asm("_binary_home_html_end");
    const size_t homehtml_size = (homehtml_end - homehtml_start);

    httpd_resp_send_chunk(req, (const char*)homehtml_start, homehtml_size);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t get_config_handler(httpd_req_t *req)
{
    redirect(req, "/login.html");
    return ESP_OK;

    // Device_Config_st config;
    // memcpy(&config, Device_GetConfig(), sizeof(Device_Config_st));

    // cJSON* config_json = NULL;
    // config_json = cJSON_CreateObject();

    // cJSON_AddStringToObject(config_json, "id", config.id.device_id);
    // cJSON_AddNumberToObject(config_json, "op_mode", config.mode);
    // cJSON_AddNumberToObject(config_json, "iface", config.network.iface);
    // cJSON_AddNumberToObject(config_json, "input", config.network.input);
    // cJSON_AddNumberToObject(config_json, "output", config.network.output);
    // cJSON_AddStringToObject(config_json, "ssid", config.network.wifi.ssid);
    // cJSON_AddStringToObject(config_json, "password", config.network.wifi.pswd);
    // cJSON_AddStringToObject(config_json, "mqtt_host", (const char*)config.network.mqtt.host);
    // cJSON_AddNumberToObject(config_json, "mqtt_port", config.network.mqtt.port);

    // ESP_LOGI(TAG, "JSON content: %s",(char *) cJSON_Print(config_json));
    // httpd_resp_sendstr(req, (char *) cJSON_Print(config_json));

    // cJSON_Delete(config_json);
    return ESP_OK;
}

esp_err_t set_config_handler(httpd_req_t *req)
{
    char *buf;
    int   size = (req->content_len) + 1;
    buf = (char *) calloc(size, sizeof(char));
    // Device_Config_st config;
    // memcpy(&config, Device_GetConfig(), sizeof(Device_Config_st));

    if(!post_reception(req, buf, size)) return ESP_FAIL;

    cJSON *payload = cJSON_Parse(buf);
    cJSON *context = cJSON_GetObjectItem(payload, "context");

    if (strcmp(context->valuestring, "netw-config") == 0)
    {
        // pega informações de network e persist
        cJSON *ssid = cJSON_GetObjectItem(payload, "ssid");
        cJSON *password = cJSON_GetObjectItem(payload, "password");
        cJSON *auth = cJSON_GetObjectItem(payload, "auth");
        cJSON *user = cJSON_GetObjectItem(payload, "user");;

        Netif_Wifi_st config;

        strcpy((config.ssid), ssid->valuestring);
        strcpy((config.pswd), password->valuestring);

        if(strcmp(auth->valuestring, "1") == 0)
        {
            strcpy(config.auth_type_str, AUTHMODE_WPA2_ENTERPRISE);
            strcpy((config.user), user->valuestring);
            config.auth_type = 1;
        } else {
            strcpy(config.auth_type_str, AUTHMODE_WPA2_PERSONAL);
            strcpy((config.user), "none");
            config.auth_type = 0;
        }
        
        NETIF_TryConnect(config);
        NETIF_SetConfig(config);
        
    }
    cJSON_Delete(payload);

    free(buf);
    return ESP_OK;
}

esp_err_t credentials_handler(httpd_req_t *req)
{
    char *buf;
    int   size = (req->content_len) + 1;
    buf = (char *) calloc(size, sizeof(char));

    if(!post_reception(req, buf, size)) return ESP_FAIL;

    char username[50];
    char password[50];
    cJSON *payload = cJSON_Parse(buf);
    cJSON *name = cJSON_GetObjectItem(payload, "username");
    cJSON *pswd = cJSON_GetObjectItem(payload, "password");

    strcpy(username, name->valuestring);
    strcpy(password, pswd->valuestring);

    cJSON_Delete(payload);

    if (strcmp(username, default_login) == 0 && strcmp(password, default_password) == 0)
    {
        ESP_LOGI(TAG,"VALIDACAO SUCCESSUL");
        httpd_resp_sendstr(req, "{\"loginStatus\": \"SUCCESS\"}");
        flag_isLogged = true;
    }
    else
    {
        ESP_LOGI(TAG,"VALIDACAO FAIL");
        httpd_resp_sendstr(req, "{\"loginStatus\": \"FAIL\"}");
    }

    free(buf);
    return ESP_OK;
}
esp_err_t style_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG,"ENTROU NO STYLE_HANDLER");
    extern const unsigned char stylecss_start[] asm("_binary_style_css_start");
    extern const unsigned char stylecss_end[] asm("_binary_style_css_end");
    const size_t stylecss_size = (stylecss_end - stylecss_start);

    httpd_resp_send_chunk(req, (const char*)stylecss_start, stylecss_size);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t settings_handler(httpd_req_t *req)
{
    if (!flag_isLogged)
    {
        redirect(req, "/login.html");
        return ESP_OK;
    }

    extern const unsigned char settingshtml_start[] asm("_binary_settings_html_start");
    extern const unsigned char settingshtml_end[] asm("_binary_settings_html_end");
    const size_t settingshtml_size = (settingshtml_end - settingshtml_start);

    httpd_resp_send_chunk(req, (const char*)settingshtml_start, settingshtml_size);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t overview_handler(httpd_req_t *req)
{
    if (!flag_isLogged)
    {
        redirect(req, "/login.html");
        return ESP_OK;
    }

    extern const unsigned char overviewhtml_start[] asm("_binary_overview_html_start");
    extern const unsigned char overviewhtml_end[] asm("_binary_overview_html_end");
    const size_t overviewhtml_size = (overviewhtml_end - overviewhtml_start);

    httpd_resp_send_chunk(req, (const char*)overviewhtml_start, overviewhtml_size);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t logout_handler(httpd_req_t *req)
{
    flag_isLogged = false;
    redirect(req, "/login.html");
    return ESP_OK;
}

esp_err_t reset_handler(httpd_req_t *req)
{
    if (!flag_isLogged)
    {
        redirect(req, "/login.html");
        return ESP_OK;
    }

    esp_restart();
    return ESP_OK;
}

esp_err_t autenticated_handle(httpd_req_t *req)
{
    if (flag_isLogged) httpd_resp_sendstr(req, "{\"loginStatus\": \"SUCCESS\"}");
    else httpd_resp_sendstr(req, "{\"loginStatus\": \"FAIL\"}"); 

    return ESP_OK;
}

esp_err_t resetneed_handler(httpd_req_t *req)
{
    if (!flag_isLogged) httpd_resp_sendstr(req, "{\"resetneed\": \"NO_AUTH\"}");
    if (/*Device_ResetIsNeeded()*/false) httpd_resp_sendstr(req, "{\"resetneed\": \"YES\"}");
    else httpd_resp_sendstr(req, "{\"resetneed\": \"NO\"}"); 

    return ESP_OK;
}

esp_err_t scripts_handler(httpd_req_t *req)
{
    if (!flag_isLogged)
    {
        redirect(req, "/login.html");
        return ESP_OK;
    }
    ESP_LOGI(TAG,"ENTROU NO PAGEMOD_HANDLER");

    extern const unsigned char scrpits_start[] asm("_binary_scripts_js_start");
    extern const unsigned char scripts_end[] asm("_binary_scripts_js_end");
    const size_t scripts_size = (scripts_end - scrpits_start);

    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, (const char*)scrpits_start, scripts_size-1);
    // httpd_resp_send_chunk(req, "\n\r\n\r", strlen("\n\r"));
    // httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t scan_devices_handler(httpd_req_t *req)
{
    if (!flag_isLogged)
    {
        redirect(req, "/login.html");
        return ESP_OK;
    }
    ESP_LOGI(TAG,"ENTROU NO SCAN_DEVICES_HANDLER");

    Network_scan_result_st res[10];
    int number_devices = NETIF_ScanNetwork(res);
    	
    char *response_buffer = (char*)malloc(1024*sizeof(char));
    char *aux_response_buffer = (char*)malloc(100*sizeof(char));

    sprintf(response_buffer, "{\"redes\":[");

    for (int w = 0; w < number_devices; w++)
    {
        sprintf(aux_response_buffer,"{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}", res[w].name, res[w].rssi, res[w].auth);
        strcat(response_buffer, aux_response_buffer);
        if (w < number_devices-1)
        {
            strcat(response_buffer, ",");
        }
    }
    strcat(response_buffer, "]}");
    printf("Scanned device: %s\n", response_buffer);

    httpd_resp_sendstr(req, response_buffer);

    free(response_buffer);
    free(aux_response_buffer);


    return ESP_OK;
}

esp_err_t pages_handler(httpd_req_t *req)
{
    if (!flag_isLogged)
    {
        redirect(req, "/login.html");
        return ESP_OK;
    }
    ESP_LOGI(TAG,"ENTROU NO SCAN_DEVICES_HANDLER");

    Network_scan_result_st res[10];
    int number_devices = NETIF_ScanNetwork(res);
    	
    char *response_buffer = (char*)malloc(1024*sizeof(char));
    char *aux_response_buffer = (char*)malloc(100*sizeof(char));

    sprintf(response_buffer, "{\"redes\":[");

    for (int w = 0; w < number_devices; w++)
    {
        sprintf(aux_response_buffer,"{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}", res[w].name, res[w].rssi, res[w].auth);
        strcat(response_buffer, aux_response_buffer);
        if (w < number_devices-1)
        {
            strcat(response_buffer, ",");
        }
    }
    strcat(response_buffer, "]}");
    printf("Scanned device: %s\n", response_buffer);

    httpd_resp_sendstr(req, response_buffer);

    free(response_buffer);
    free(aux_response_buffer);


    return ESP_OK;
}

httpd_uri_t urls[] = {
    {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = index_handler,
        .user_ctx = NULL
    },
    {
        .uri      = "/login.html",
        .method   = HTTP_GET,
        .handler  = login_handler,
        .user_ctx = NULL
    },
    {
        .uri      = "/home.html",
        .method   = HTTP_GET,
        .handler  = home_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/overview.html",
        .method   = HTTP_GET,
        .handler  = overview_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/settings.html",
        .method   = HTTP_GET,
        .handler  = settings_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/setconfig",
        .method   = HTTP_POST,
        .handler  = set_config_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/getconfig",
        .method   = HTTP_GET,
        .handler  = get_config_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/credentials",
        .method   = HTTP_POST,
        .handler  = credentials_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/style.css",
        .method   = HTTP_GET,
        .handler  = style_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/logout",
        .method   = HTTP_GET,
        .handler  = logout_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/reset",
        .method   = HTTP_GET,
        .handler  = reset_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/autenticated",
        .method   = HTTP_GET,
        .handler  = autenticated_handle,
        .user_ctx = NULL,
    },
    {
        .uri      = "/resetneed",
        .method   = HTTP_GET,
        .handler  = resetneed_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/scripts.js",
        .method   = HTTP_GET,
        .handler  = scripts_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/scanned_devices",
        .method   = HTTP_GET,
        .handler  = scan_devices_handler,
        .user_ctx = NULL,
    },
    {
        .uri      = "/get_pages",
        .method   = HTTP_GET,
        .handler  = pages_handler,
        .user_ctx = NULL,
    }
};

void  WS_Init(httpd_uri_t *app_urls, int number)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.max_uri_handlers = 18;
    config.lru_purge_enable = true;

    httpd_uri_t *union_urls = (httpd_uri_t*)malloc(( sizeof(urls)/sizeof(urls[0]) + number) * sizeof(httpd_uri_t));
    memcpy(union_urls, urls, sizeof(urls));
    if (number > 0)
    {
        memcpy(&union_urls[sizeof(urls)/sizeof(urls[0])], app_urls, number* sizeof(httpd_uri_t));
    }

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) 
    {
        ESP_LOGI(TAG, "Registering URI handlers");

        printf("Num itens novos %d\n", number);
        uint32_t arr_size = (sizeof(urls)/sizeof(urls[0])) + number; 
        printf("Num itens %u\n", arr_size);
        for (int w = 0; w < arr_size; w++)
        {
            printf("registering url: %d\n", w);
            httpd_register_uri_handler(server, &(union_urls[w]));
        }
        return;//server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return;// NULL;
}

void WS_Deinit(void)
{
    return;
}

void strShiftLeft(char *string, size_t shiftLen)
{
    memmove(string, string + shiftLen, strlen(string) + 1);
}

bool post_reception(httpd_req_t *req, char *buf, int size_buf)
{
    int ret = 0, remaining = req->content_len;
    ESP_LOGI(TAG, "Size buf: %i", size_buf);

    while(remaining > 0)
    {
        /* Read the data for the request */
        if((ret = httpd_req_recv(req, buf, remaining)) <= 0)
        {
            if(ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return 0;
        }

        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    if(size_buf > 2)
    {
        buf[ret] = '\0';
        char *ch = strstr(buf, "\r\n");

        while(ch != NULL)
        {
            strShiftLeft(ch, 1);
            *ch = '&';
            ch  = strstr(buf, "\r\n");
        }
    }

    return 1;
}

void redirect(httpd_req_t *req, const char *data)
{
    ESP_LOGI(TAG, "Redirecting to %s", data);
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", data);
    httpd_resp_send(req, NULL, 0);
}