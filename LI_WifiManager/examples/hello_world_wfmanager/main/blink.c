/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "LI_Netif_Esp32.h"
#include "APP_WebServer_Esp32.h"

/* Can use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO 21

void app_callback(Network_event_et evt, Network_Message_st* msg)
{
    switch(evt)
    { 
        case NETWORK_INTERFACE_SETTED:
        {
            printf("NETWORK_INTERFACE_SETTED\n");
        }
        break;

        case NETWORK_INTERFACE_CONNECTED:
        {
            printf("NETWORK_INTERFACE_CONNECTED\n");
        }
        break;

        case NETWORK_INTERFACE_CONNECTION_FAILED:
        {
            printf("NETWORK_INTERFACE_CONNECTION_FAILED\n");
        }
        break;

        case NETWORK_INTERFACE_DISCONNECTED:
        {
            printf("NETWORK_INTERFACE_DISCONNECTED\n");

        }
        break;

        case NETWORK_PLATFORM_PROTOCOL_SETTED:
        {
            printf("NETWORK_PLATFORM_PROTOCOL_SETTED\n");
        }
        break;

        case NETWORK_PLATFORM_PROTOCOL_CONNECTED:
        {
            printf("NETWORK_PLATFORM_PROTOCOL_CONNECTED\n");
        }
        break;

        case NETWORK_PLATFORM_PROTOCOL_MESSAGE_RECEIVED:
        {
            printf("NETWORK_PLATFORM_PROTOCOL_MESSAGE_RECEIVED\n");
        }
        break;

        case NETWORK_PLATFORM_PROTOCOL_CONNECTION_FAILED:
        {
            printf("NETWORK_PLATFORM_PROTOCOL_CONNECTION_FAILED\n");
        }
        break;

        case NETWORK_PLATFORM_PROTOCOL_DISCONNECTED:
        {
            printf("NETWORK_PLATFORM_PROTOCOL_DISCONNECTED\n");
        }
        break;

        case NETWORK_INTERNAL_PROTOCOL_CONNECTED:
        {
            printf("NETWORK_INTERNAL_PROTOCOL_CONNECTED\n");
        }
        break;

        case NETWORK_INTERNAL_PROTOCOL_MESSAGE_RECEIVED:
        {
            printf("NETWORK_INTERNAL_PROTOCOL_MESSAGE_RECEIVED\n");
        }
        break;

        case NETWORK_INTERNAL_PROTOCOL_CONNECTION_FAILED:
        {
            printf("NETWORK_INTERNAL_PROTOCOL_CONNECTION_FAILED\n");
        }
        break;

        case NETWORK_INTERNAL_PROTOCOL_DISCONNECTED:
        {
            printf("NETWORK_INTERNAL_PROTOCOL_DISCONNECTED\n");
        }
        break;

        case NETWORK_PROCEDURE_FINISHED:
        {
            printf("NETWORK_PROCEDURE_FINISHED\n");
        }
        break;

    }
}

esp_err_t TESTE_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "ACHOUU!");
    return ESP_OK;
}

esp_err_t TESTE2_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "ACHOUU2!");
    return ESP_OK;
}

void app_main(void)
{
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    NETIF_Init(app_callback);

    httpd_uri_t urls[] = {
    {
        .uri      = "/teste",
        .method   = HTTP_GET,
        .handler  = TESTE_handler,
        .user_ctx = NULL
    },
    {
        .uri      = "/teste2",
        .method   = HTTP_GET,
        .handler  = TESTE2_handler,
        .user_ctx = NULL
    }};

    WS_Menu_list_st menu_list[] = {
        {
            .label = "Teste",
            .url = "/teste"
        },
        {
            .label = "Teste2",
            .url = "/teste2"
        }
    };

    WS_Init(urls, 2, menu_list, 2);
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while(1) {
        /* Blink off (output low) */
        printf("Turning off the LED\n");
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(15000 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        printf("Turning on the LED\n");
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
}
