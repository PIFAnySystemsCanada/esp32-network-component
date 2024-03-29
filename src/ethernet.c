/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#if CONFIG_ESP_ETHERNET_ENABLED

static const char *TAG = "ETHCTRL";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_ethernet_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define ETHERNET_CONNECTED_BIT BIT0
#define ETHERNET_DISCONNECTED_BIT      BIT1

static void (*led_ethernet_connected_callback)() = NULL;
static void (*led_ethernet_disconnected_callback)() = NULL;

void ethernet_waitforconnect(void)
{
    while (1)
    {
        // Sit and wait until something happens
        EventBits_t bits = xEventGroupWaitBits(s_ethernet_event_group,
                ETHERNET_CONNECTED_BIT,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);

        if (bits & ETHERNET_CONNECTED_BIT) {
            return;
        }
    }
}

void set_ethernet_led_connected_callback(void (*callback)())
{
    if (callback!=NULL)
    {
        led_ethernet_connected_callback = callback;
    }
}

void set_ethernet_led_disconnected_callback(void (*callback)())
{
    if (callback!=NULL)
    {
        led_ethernet_disconnected_callback = callback;
    }
}

static void led_connected()
{
    if (led_ethernet_connected_callback!=NULL)
    {
        led_ethernet_connected_callback();
    }
}

static void led_disconnected()
{
    if (led_ethernet_disconnected_callback!=NULL)
    {
        led_ethernet_disconnected_callback();
    }
}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        xEventGroupClearBits(s_ethernet_event_group, ETHERNET_CONNECTED_BIT);
        xEventGroupSetBits(s_ethernet_event_group, ETHERNET_DISCONNECTED_BIT);
        led_disconnected();
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    if (esp_ip4_addr1_16(&event->ip_info.ip)==169)
    {
        ESP_LOGW(TAG, "Got IP, but local one - not firing events");
    }
    else
    {
        led_connected();
        xEventGroupClearBits(s_ethernet_event_group, ETHERNET_DISCONNECTED_BIT);
        xEventGroupSetBits(s_ethernet_event_group, ETHERNET_CONNECTED_BIT);
    }

}

void ethernet_setup(void)
{
    s_ethernet_event_group = xEventGroupCreate();

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    // Set default handlers to process TCP/IP stuffs
    ESP_ERROR_CHECK(esp_eth_set_default_handlers(eth_netif));
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_ESP_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_ESP_ETH_PHY_RST_GPIO;
    ESP_LOGI(TAG, "Ethernet Address %d and Reset PIN %d", CONFIG_ESP_ETH_PHY_ADDR, CONFIG_ESP_ETH_PHY_RST_GPIO);
#if CONFIG_ESP_USE_INTERNAL_ETHERNET
    mac_config.smi_mdc_gpio_num = CONFIG_ESP_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_ESP_ETH_MDIO_GPIO;
    ESP_LOGI(TAG, "Ethernet MDC PIN %d and MDIO PIN %d", CONFIG_ESP_ETH_MDC_GPIO, CONFIG_ESP_ETH_MDIO_GPIO);
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
#if CONFIG_ESP_ETH_PHY_IP101
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_ESP_ETH_PHY_RTL8201
    esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_ESP_ETH_PHY_LAN8720
    esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);
#elif CONFIG_ESP_ETH_PHY_DP83848
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#endif
#elif CONFIG_ESP_USE_DM9051
    gpio_install_isr_service(0);
    spi_device_handle_t spi_handle = NULL;
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_ESP_DM9051_MISO_GPIO,
        .mosi_io_num = CONFIG_ESP_DM9051_MOSI_GPIO,
        .sclk_io_num = CONFIG_ESP_DM9051_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_ESP_DM9051_SPI_HOST, &buscfg, 1));
    spi_device_interface_config_t devcfg = {
        .command_bits = 1,
        .address_bits = 7,
        .mode = 0,
        .clock_speed_hz = CONFIG_ESP_DM9051_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = CONFIG_ESP_DM9051_CS_GPIO,
        .queue_size = 20
    };
    ESP_ERROR_CHECK(spi_bus_add_device(CONFIG_ESP_DM9051_SPI_HOST, &devcfg, &spi_handle));
    /* dm9051 ethernet driver is based on spi driver */
    eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(spi_handle);
    dm9051_config.int_gpio_num = CONFIG_ESP_DM9051_INT_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_dm9051(&phy_config);
#endif
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_LOGI(TAG, "Installing Ethernet Driver...");
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    /* attach Ethernet driver to TCP/IP stack */
    ESP_LOGI(TAG, "Attaching Ethernet Driver...");
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    /* start Ethernet driver state machine */
    ESP_LOGI(TAG, "Starting Ethernet Driver...");
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}

#endif
