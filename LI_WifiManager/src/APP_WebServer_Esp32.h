#ifndef _LI_WEBSERVER_ESP_
#define _LI_WEBSERVER_ESP_

#include <stdint.h>
#include <stdbool.h>
#include "esp_http_server.h"

typedef struct
{
    bool is_logged;
}WB_Server_st;

typedef struct
{
    char label[64];
    char url[32];
}WS_Menu_list_st;


void WS_Init(httpd_uri_t *app_urls, int number, WS_Menu_list_st* list, int list_len);
void WS_Deinit(void);

#endif /*_LI_WEBSERVER_ESP_*/