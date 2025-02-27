#ifndef _LI_WEBSERVER_ESP_
#define _LI_WEBSERVER_ESP_

#ifdef __cplusplus
extern "C" {
#endif

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
bool WS_IsLogged(void);

bool WS_PostReception(httpd_req_t *req, char *buf, int size_buf);
void WS_Redirect(httpd_req_t *req, const char *data);

#ifdef __cplusplus
}
#endif

#endif /*_LI_WEBSERVER_ESP_*/