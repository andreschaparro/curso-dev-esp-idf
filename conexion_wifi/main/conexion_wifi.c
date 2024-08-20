#include <stdio.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define EXAMPLE_ESP_WIFI_STA_SSID CONFIG_ESP_WIFI_REMOTE_AP_SSID       // STA Configuration
#define EXAMPLE_ESP_WIFI_STA_PASSWD CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD // STA Configuration
#define WIFI_CONNECTED_BIT BIT0                                        // Signal we are connected to the AP with an IP

static const char *TAG = "main";

static EventGroupHandle_t event_group = NULL; // FreeRTOS event group to signal when we are connected

static void default_nvs_init();
static void wifi_init_sta();
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void app_main(void)
{
    event_group = xEventGroupCreate();
    default_nvs_init();                                                                                                  // Initialize event group
    ESP_ERROR_CHECK(esp_netif_init());                                                                                   // Initialize TCP/IP
    ESP_ERROR_CHECK(esp_event_loop_create_default());                                                                    // Initialize the event loop
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, NULL));  // Register our event handler for IP related events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL)); // Register our event handler for Wi-Fi related events
    wifi_init_sta();
    xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT, pdTRUE, pdTRUE, portMAX_DELAY); // Wait for Wi-Fi connection
    ESP_LOGI(TAG, "Hola Mundo!");
}

static void default_nvs_init()
{
    esp_err_t ret = nvs_flash_init();                                             // Initialize NVS partition
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) // NVS partition was truncated and needs to be erased
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init(); //  Retry nvs_flash_init
    }
    ESP_ERROR_CHECK(ret);
}

static void wifi_init_sta() // Start Wi-Fi in station mode
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_STA_SSID,
            .password = EXAMPLE_ESP_WIFI_STA_PASSWD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) // Event handler for catching IP related events
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected with IP      : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Connected with Netmask : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Connected with Gateway : " IPSTR, IP2STR(&event->ip_info.gw));
        xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT); // Signal main application to continue execution
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) // Event handler for catching Wi-Fi related events
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Station started");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGE(TAG, "Disconnected. Connecting to the AP again...");
        esp_wifi_connect();
    }
}