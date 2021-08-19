#include "lwip/err.h"
#include "lwip/sys.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "network.h"

static const char *TAG = "NETCTRL";

#if !CONFIG_ESP_ETHERNET_ENABLED && !CONFIG_ESP_WIFI_ENABLED
#error Networking is required. WIFI or Ethernet must be defined.
#endif
void network_setup(void)
{
    // Setup networking and the event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#ifdef CONFIG_ESP_ETHERNET_ENABLED
    ESP_LOGI(TAG, "Configuring Ethernet");
    ethernet_setup();
#endif

#ifdef CONFIG_ESP_WIFI_ENABLED
    ESP_LOGI(TAG, "Configuring WIFI");
    wifi_setup();
#endif
}

void network_waitforconnect(void)
{
#ifdef CONFIG_ESP_ETHERNET_ENABLED
    ESP_LOGI(TAG, "Waiting for ethernet to connect...");
    ethernet_waitforconnect();
#endif

#ifdef CONFIG_ESP_WIFI_ENABLED
    ESP_LOGI(TAG, "Waiting for WIFI to connect...");
    wifi_connect();
    wifi_waitforconnect();
#endif
}

void set_network_led_connected_callback(void (*callback)())
{
#ifdef CONFIG_ESP_ETHERNET_ENABLED
    set_ethernet_led_connected_callback(callback);
#endif

#ifdef CONFIG_ESP_WIFI_ENABLED
    set_wifi_led_connected_callback(callback);
#endif
}

void set_network_led_disconnected_callback(void (*callback)())
{
#ifdef CONFIG_ESP_ETHERNET_ENABLED
    set_ethernet_led_disconnected_callback(callback);
#endif

#ifdef CONFIG_ESP_WIFI_ENABLED
    set_wifi_led_disconnected_callback(callback);
#endif
}