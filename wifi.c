/*
    WiFi station driver

*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "sdkconfig.h"

#include "wifi.h"

#if CONFIG_POWER_SAVE_MIN_MODEM
#define DEFAULT_PS_MODE WIFI_PS_MIN_MODEM
#elif CONFIG_POWER_SAVE_MAX_MODEM
#define DEFAULT_PS_MODE WIFI_PS_MAX_MODEM
#elif CONFIG_POWER_SAVE_NONE
#define DEFAULT_PS_MODE WIFI_PS_NONE
#else
#define DEFAULT_PS_MODE WIFI_PS_NONE
#endif /*CONFIG_POWER_SAVE_MODEM*/

// Delay to recheck WIFI status
static int WIFI_LOOP_DELAY_MS = CONFIG_ESP_WIFI_RETRY_DELAY;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_DISCONNECTED_BIT      BIT1

static void (*led_connected_callback)() = NULL;
static void (*led_disconnected_callback)() = NULL;

static const char *TAG = "WIFICTRL";

static bool wifi_enabled = true;

#ifdef CONFIG_ESP_WIFI_REBOOT_ENABLED
static uint16_t retrycount = 0;
#endif

static wifi_config_t wifi_config_1 = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

#ifdef CONFIG_ESP_WIFI_SSID2
static wifi_config_t wifi_config_2 = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID2,
            .password = CONFIG_ESP_WIFI_PASSWORD2,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
#endif

static wifi_config_t *wifi_config = &wifi_config_1;

void wifi_waitforconnect(void)
{
    while (1)
    {
        // Sit and wait until something happens
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            return;
        }
    }
}

void wifi_disable()
{
    wifi_enabled = false;
}

void wifi_disconnect(void)
{
    esp_wifi_disconnect();
}

void set_led_connected_callback(void (*callback)())
{
    if (callback!=NULL)
    {
        led_connected_callback = callback;
    }
}

void set_led_disconnected_callback(void (*callback)())
{
    if (callback!=NULL)
    {
        led_disconnected_callback = callback;
    }
}

static void led_connected()
{
    if (led_connected_callback!=NULL)
    {
        led_connected_callback();
    }
}

static void led_disconnected()
{
    if (led_disconnected_callback!=NULL)
    {
        led_disconnected_callback();
    }
}

static void wifi_connected(void *pvParameter)
{
    while (wifi_enabled)
    {
        // Sit and wait until something happens
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_DISCONNECTED_BIT,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);

        // xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
        // happened.
        if (bits & WIFI_DISCONNECTED_BIT) {
            if (!wifi_enabled)
            {
                ESP_LOGW(TAG, "Wifi has been disabled. Thread aborted.");
                break;
            }
            ESP_LOGI(TAG, "Disconnected from AP, retrying....");
            // Clean the disconnected bit so we don't hit this again unless 
            // we don't connect again.
            xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
            // Make sure the WIFI driver is offline
            esp_wifi_disconnect();
#ifdef CONFIG_ESP_WIFI_REBOOT_ENABLED
            retrycount++;
            if (retrycount>CONFIG_ESP_WIFI_REBOOT_COUNT)
            {
                ESP_LOGE(TAG, "Retry count exceeded...rebooting...");
                esp_restart();
            }
#endif

#ifdef CONFIG_ESP_WIFI_SSID2
            // Flip between our two hard coded AP's
            if (wifi_config == &wifi_config_1)
            {
                wifi_config = &wifi_config_2;
                ESP_LOGI(TAG, "Switching to WIFI 2 config: %s", wifi_config_2.sta.ssid);
            }
            else
            {
                ESP_LOGI(TAG, "Switching to WIFI 1 config: %s", wifi_config_1.sta.ssid);
                wifi_config = &wifi_config_1;
            }
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
#endif
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
        // Delay so we don't hammer the AP
        vTaskDelay((WIFI_LOOP_DELAY_MS * 1000) / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        ESP_LOGI(TAG,"Attempting to connect to the AP");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
        led_disconnected();
        ESP_LOGI(TAG,"Disconnected from the AP");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        led_connected();
        xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    // Event loop is created in app_main
    //ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_country_t wifi_country = {
        .cc = "CA",
        .schan = 1,
        .nchan = 11,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_set_country(&wifi_country));
    ESP_ERROR_CHECK(esp_wifi_start());

#ifdef CONFIG_PM_ENABLE
    ESP_LOGI(TAG, "WIFI Power management configured");
    esp_wifi_set_ps(DEFAULT_PS_MODE);
#else
    ESP_LOGI(TAG, "WIFI Power management disabled");
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // Start a thread to monitor the WIFI connection
    xTaskCreate(wifi_connected, THREAD_WIFI_NAME, THREAD_WIFI_STACKSIZE, NULL, THREAD_WIFI_PRIORITY, NULL);
}

void wifi_setup(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // nvs configured by WIFI
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());    
}

void wifi_connect(void)
{
    wifi_enabled = true;
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
}
