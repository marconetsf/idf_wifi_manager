#ifndef _LI_WEBSERVER_ESP_
#define _LI_WEBSERVER_ESP_

#include <stdint.h>
#include <stdbool.h>
#include "esp_http_server.h"

typedef struct
{
    bool is_logged;
}WB_Server_st;

void WS_Init(httpd_uri_t *app_urls, int number);
void WS_Deinit(void);

#endif /*_LI_WEBSERVER_ESP_*/