#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "iot_button.h"
#include "led_indicator.h"
#include "qrcode.h"

#define BOOT_BUTTON_NUM GPIO_NUM_0 // Most development boards have "boot" button attached to GPIO0
#define BUTTON_ACTIVE_LEVEL 0
#define INBUILT_LED_NUM GPIO_NUM_2 // Inbuilt led
#define LED_ACTIVE_LEVEL true
#define WIFI_CONNECTED_BIT BIT0 // Signal we are connected to the AP with an IP
#define SERVICE_NAME_PREFIX "PROV_"
#define PROV_QR_VERSION "v1"
#define PROV_TRANSPORT_SOFTAP "softap"
#define QRCODE_BASE_URL "https://espressif.github.io/esp-jumpstart/qrcode.html"
#define PROV_MGR_MAX_RETRY_CNT 5

enum // blinking type and priority
{
    BLINK_PROVISIONING = 0, // higher priority
    BLINK_RECONNECTING,
    BLINK_CONNECTING,
    BLINK_CONNECTED,
    BLINK_MAX,
};

static const char *TAG = "main";

static const char *button_event_table[] = {
    "BUTTON_PRESS_DOWN",
    "BUTTON_PRESS_UP",
    "BUTTON_PRESS_REPEAT",
    "BUTTON_PRESS_REPEAT_DONE",
    "BUTTON_SINGLE_CLICK",
    "BUTTON_DOUBLE_CLICK",
    "BUTTON_MULTIPLE_CLICK",
    "BUTTON_LONG_PRESS_START",
    "BUTTON_LONG_PRESS_HOLD",
    "BUTTON_LONG_PRESS_UP",
    "BUTTON_PRESS_END",
};

static const blink_step_t provisioning[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 100},  // step1: turn on LED 100 ms
    {LED_BLINK_HOLD, LED_STATE_OFF, 100}, // step2: turn off LED 100 ms
    {LED_BLINK_LOOP, 0, 0},               // step3: loop from step1
};

static const blink_step_t reconnecting[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 100},  // step1: turn on LED 100 ms
    {LED_BLINK_HOLD, LED_STATE_OFF, 400}, // step2: turn off LED 400 ms
    {LED_BLINK_LOOP, 0, 0},               // step3: loop from step1
};

static const blink_step_t connecting[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 100},  // step1: turn on LED 100 ms
    {LED_BLINK_HOLD, LED_STATE_OFF, 900}, // step2: turn off LED 900 ms
    {LED_BLINK_LOOP, 0, 0},               // step3: loop from step1
};

static const blink_step_t connected[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 1000}, // step1: turn on LED 1 s
    {LED_BLINK_LOOP, 0, 0},               // step2: loop from step1
};

blink_step_t const *led_indicator_blink_lists[] = {
    [BLINK_PROVISIONING] = provisioning, // Provisioning
    [BLINK_RECONNECTING] = reconnecting, // Reconnecting to AP if lose connection
    [BLINK_CONNECTING] = connecting,     // Connecting to AP
    [BLINK_CONNECTED] = connected,       // Connected to AP succeed
    [BLINK_MAX] = NULL,
};

static led_indicator_handle_t inbuilt_led_handle = NULL;

static EventGroupHandle_t event_group = NULL; // FreeRTOS event group to signal when we are connected

static void button_init(gpio_num_t gpio_num);
static void button_event_cb(void *arg, void *data);
static void led_indicator_init(gpio_num_t led_gpio, led_indicator_handle_t *led_handle);
static void default_nvs_init(void);
static void wifi_init(void);
static void wifi_start_sta(void);
static void prov_init(void);
static void prov_start(void);
static void get_device_service_name(char *service_name, size_t max);
static void wifi_prov_print_qr(const char *name, const char *pop, const char *transport);
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void prov_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void app_main(void)
{
    event_group = xEventGroupCreate(); // Initialize event group
    button_init(BOOT_BUTTON_NUM);
    led_indicator_init(INBUILT_LED_NUM, &inbuilt_led_handle);
    default_nvs_init();
    ESP_ERROR_CHECK(esp_netif_init());                                                                                        // Initialize TCP/IP
    ESP_ERROR_CHECK(esp_event_loop_create_default());                                                                         // Initialize the event loop
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, NULL));       // Register our event handler for IP related events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));      // Register our event handler for Wi-Fi related events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL, NULL)); // Register our event handler for Provisioning related events
    wifi_init();
    prov_init();
    prov_start();
    xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT, pdTRUE, pdTRUE, portMAX_DELAY); // Wait for Wi-Fi connection
    ESP_LOGI(TAG, "Hola Mundo!");
}

static void button_init(gpio_num_t gpio_num)
{
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = gpio_num,
            .active_level = BUTTON_ACTIVE_LEVEL,
        },
    };
    button_handle_t btn = iot_button_create(&btn_cfg);
    assert(btn);
    ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, (void *)BUTTON_SINGLE_CLICK));
    ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, button_event_cb, (void *)BUTTON_LONG_PRESS_UP));
}

static void button_event_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Button event %s", button_event_table[(button_event_t)data]);
    switch ((button_event_t)data)
    {
    case BUTTON_SINGLE_CLICK:
        break;
    case BUTTON_LONG_PRESS_UP:
        ESP_ERROR_CHECK(wifi_prov_mgr_reset_provisioning()); // Reset provisioned status of the device
        esp_restart();
        break;
    default:
        break;
    }
}

static void led_indicator_init(gpio_num_t led_gpio, led_indicator_handle_t *led_handle)
{
    led_indicator_gpio_config_t gpio_config = {
        .is_active_level_high = LED_ACTIVE_LEVEL,
        .gpio_num = led_gpio,
    };
    const led_indicator_config_t config = {
        .mode = LED_GPIO_MODE,
        .led_indicator_gpio_config = &gpio_config,
        .blink_lists = led_indicator_blink_lists,
        .blink_list_num = BLINK_MAX,
    };
    *led_handle = led_indicator_create(&config);
    assert(*led_handle != NULL);
}

static void default_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();                                             // Initialize NVS partition
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) // NVS partition was truncated and needs to be erased
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init(); // Retry
    }
    ESP_ERROR_CHECK(ret);
}

static void wifi_init(void) // Initialize Wi-Fi including netif with default config
{
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

static void wifi_start_sta(void) // Start Wi-Fi in station mode
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void prov_init(void) // Initialize provisioning service
{
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,                    // SoftAP transport with HTTP server for handling the provisioning commands
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE, // WIFI_PROV_EVENT_HANDLER_NONE when using SoftAP transport
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
}

static void prov_start(void) // Start provisioning service
{
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned)); // This internally checks if the Wi-Fi credentials are stored in NVS
    if (!provisioned)
    {
        ESP_LOGI(TAG, "Starting provisioning");
        ESP_ERROR_CHECK(led_indicator_start(inbuilt_led_handle, BLINK_PROVISIONING));
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));                                                      // Translates to Wi-Fi SSID in SoftAP mode
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;                                                             // WIFI_PROV_SECURITY_1 is secure communication that consists of a secure handshake using key exchange, proof of possession (pop), and encryption/decryption of messages
        const char *pop = "abcd1234";                                                                                     // A proof-of-possession should be a string with a length > 0
        wifi_prov_security1_params_t *sec_params = pop;                                                                   // This is the structure for passing security parameters for the WIFI_PROV_SECURITY_1
        const char *service_key = NULL;                                                                                   // Translates to Wi-Fi password in SoftAP mode
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *)sec_params, service_name, service_key)); // Start provisioning service
        wifi_prov_print_qr(service_name, pop, PROV_TRANSPORT_SOFTAP);                                                     // Print QR code for provisioning
    }
    else
    {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
        wifi_prov_mgr_deinit(); // We don't need the manager as device is already provisioned, so let's release it's resources
        wifi_start_sta();
    }
}

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = SERVICE_NAME_PREFIX;
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, eth_mac));
    snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

static void wifi_prov_print_qr(const char *name, const char *pop, const char *transport)
{
    if (!name || !transport)
    {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[150] = {0};
    if (pop)
    {
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}", PROV_QR_VERSION, name, pop, transport);
    }
    else
    {
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"transport\":\"%s\"}", PROV_QR_VERSION, name, transport);
    }
    ESP_LOGI(TAG, "Scan this QR code from the provisioning application for Provisioning.");
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_qrcode_generate(&cfg, payload));
    ESP_LOGI(TAG, "If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s", QRCODE_BASE_URL, payload);
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) // Event handler for catching IP related events
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected with IP      : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Connected with Netmask : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Connected with Gateway : " IPSTR, IP2STR(&event->ip_info.gw));
        ESP_ERROR_CHECK(led_indicator_stop(inbuilt_led_handle, BLINK_CONNECTING));
        ESP_ERROR_CHECK(led_indicator_stop(inbuilt_led_handle, BLINK_RECONNECTING));
        ESP_ERROR_CHECK(led_indicator_start(inbuilt_led_handle, BLINK_CONNECTED));
        xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT); // Signal main application to continue execution
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) // Event handler for catching Wi-Fi related events
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Station started");
            ESP_ERROR_CHECK(led_indicator_start(inbuilt_led_handle, BLINK_CONNECTING));
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGE(TAG, "Disconnected. Connecting to the AP again...");
            ESP_ERROR_CHECK(led_indicator_start(inbuilt_led_handle, BLINK_RECONNECTING));
            esp_wifi_connect();
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "SoftAP transport: Connected!");
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "SoftAP transport: Disconnected!");
            break;
        default:
            break;
        }
    }
}

static void prov_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static int retries;
    if (event_base == WIFI_PROV_EVENT)
    {
        switch (event_id)
        {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;
        case WIFI_PROV_CRED_RECV:
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Received Wi-Fi credentials\n\tSSID     : %s\n\tPassword : %s", (const char *)wifi_sta_cfg->ssid, (const char *)wifi_sta_cfg->password);
            break;
        case WIFI_PROV_CRED_FAIL:
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
            ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s\n\tPlease reset to factory and retry provisioning", (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
            retries++;
            if (retries >= PROV_MGR_MAX_RETRY_CNT)
            {
                ESP_LOGI(TAG, "Failed to connect with provisioned AP, reseting provisioned credentials");
                ESP_ERROR_CHECK(wifi_prov_mgr_reset_sm_state_on_failure());
                retries = 0;
            }
            break;
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            ESP_ERROR_CHECK(led_indicator_stop(inbuilt_led_handle, BLINK_PROVISIONING));
            break;
        case WIFI_PROV_END:
            wifi_prov_mgr_deinit(); // De-initialize manager once provisioning is finished
            break;
        default:
            break;
        }
    }
}