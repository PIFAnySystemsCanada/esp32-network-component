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

#if CONFIG_ESP_WIFI_ENABLED
#ifdef CONFIG_ESP_BLUFI_ENABLED
#include "esp_bt.h"
#include "esp_blufi_api.h"
#include "blufi.h"
#include "esp_blufi.h"
#endif

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

#ifdef CONFIG_ESP_BLUFI_ENABLED
static const char *BLUFI_TAG = "BLUFI";

static void (*custom_command_callback)(const char*) = NULL;

/* store the station info for send back to phone */
static bool gl_sta_connected = false;
static bool ble_is_connected = false;
static uint8_t gl_sta_bssid[6];
static uint8_t gl_sta_ssid[32];
static int gl_sta_ssid_len;

static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

void set_custom_command_callback(void (*callback)(const char*))
{
    if (callback!=NULL)
    {
        custom_command_callback = callback;
    }
}


#endif

#ifdef CONFIG_ESP_WIFI_REBOOT_ENABLED
static uint16_t retrycount = 0;
#endif

#ifdef CONFIG_ESP_MANUAL_WIFI_ENABLED
static wifi_config_t wifi_config_1 = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            // authmode sets the minimum required auth mode in order to connect.
            // If this is configured for an auth mode that the AP does not support, it will not
            // connect. In fact, the WIFI driver will not even try to connect. Set this to the minimum
            // required mode. WIFI_AUTH_WPA_PSK is probably the best one.
    	    .threshold.authmode = WIFI_AUTH_WPA_PSK,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            // We can do Protected Management Frames, but will connect to any AP even one that doesn't support PMF.
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

#ifdef CONFIG_ESP_WIFI_SSID2_ENABLED
static wifi_config_t wifi_config_2 = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID2,
            .password = CONFIG_ESP_WIFI_PASSWORD2,
            .threshold.authmode = WIFI_AUTH_WPA_PSK,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
#endif

static wifi_config_t *wifi_config = &wifi_config_1;
#endif

#ifdef CONFIG_ESP_BLUFI_ENABLED
static wifi_config_t sta_config;
static wifi_config_t *wifi_config = &sta_config;
#endif

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

void set_wifi_led_connected_callback(void (*callback)())
{
    if (callback!=NULL)
    {
        led_connected_callback = callback;
    }
}

void set_wifi_led_disconnected_callback(void (*callback)())
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
            ESP_LOGI(TAG, "Disconnected from %s, retrying....", wifi_config->sta.ssid);
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

#ifdef CONFIG_ESP_WIFI_SSID2_ENABLED
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
            ESP_LOGI(TAG, "Connecting to %s...", wifi_config->sta.ssid);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
        // Delay so we don't hammer the AP
        vTaskDelay((WIFI_LOOP_DELAY_MS * 1000) / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) 
    {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG,"Attempting to connect to the %s", wifi_config->sta.ssid);
                esp_wifi_connect();
                break;
#ifdef CONFIG_ESP_BLUFI_ENABLED    
            case WIFI_EVENT_STA_CONNECTED:
                gl_sta_connected = true;
                wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t*) event_data;
                memcpy(gl_sta_bssid, event->bssid, 6);
                memcpy(gl_sta_ssid, event->ssid, event->ssid_len);
                gl_sta_ssid_len = event->ssid_len;
                break;
            case WIFI_EVENT_SCAN_DONE: {
                uint16_t apCount = 0;
                esp_wifi_scan_get_ap_num(&apCount);
                if (apCount == 0) {
                    ESP_LOGI(BLUFI_TAG, "Nothing AP found");
                    break;
                }
                wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
                if (!ap_list) {
                    ESP_LOGE(BLUFI_TAG, "malloc error, ap_list is NULL");
                    break;
                }
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, ap_list));
                esp_blufi_ap_record_t * blufi_ap_list = (esp_blufi_ap_record_t *)malloc(apCount * sizeof(esp_blufi_ap_record_t));
                if (!blufi_ap_list) {
                    if (ap_list) {
                        free(ap_list);
                    }
                    ESP_LOGE(BLUFI_TAG, "malloc error, blufi_ap_list is NULL");
                    break;
                }
                for (int i = 0; i < apCount; ++i)
                {
                    blufi_ap_list[i].rssi = ap_list[i].rssi;
                    memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
                }

                if (ble_is_connected == true) {
                    esp_blufi_send_wifi_list(apCount, blufi_ap_list);
                } else {
                    ESP_LOGI(BLUFI_TAG, "BLUFI BLE is not connected yet\n");
                }

                esp_wifi_scan_stop();
                free(ap_list);
                free(blufi_ap_list);
                break;
            }
            case WIFI_EVENT_AP_START: {
                wifi_mode_t mode;
                if (ble_is_connected == true) {
                    esp_wifi_get_mode(&mode);
                    if (gl_sta_connected) {
                        esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, NULL);
                    } else {
                        esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, NULL);
                    }
                } else {
                    ESP_LOGI(BLUFI_TAG, "BLUFI BLE is not connected yet");
                }
                break;
            }
#endif    
            case WIFI_EVENT_STA_DISCONNECTED:
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
                led_disconnected();
#ifdef CONFIG_ESP_BLUFI_ENABLED    
                gl_sta_connected = false;
                memset(gl_sta_ssid, 0, 32);
                memset(gl_sta_bssid, 0, 6);
                gl_sta_ssid_len = 0;
#endif
                ESP_LOGI(TAG,"Disconnected from the %s", wifi_config->sta.ssid);
                break;
            default:
                break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;
        ESP_LOGI(TAG, "WIFI Got IP Address");
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "WIFIIP:" IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "WIFIMASK:" IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "WIFIGW:" IPSTR, IP2STR(&ip_info->gw));
        ESP_LOGI(TAG, "~~~~~~~~~~~");

#ifdef CONFIG_ESP_BLUFI_ENABLED
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;
        esp_wifi_get_mode(&mode);
        memset(&info, 0, sizeof(esp_blufi_extra_info_t));
        memcpy(info.sta_bssid, gl_sta_bssid, 6);
        info.sta_bssid_set = true;
        info.sta_ssid = gl_sta_ssid;
        info.sta_ssid_len = gl_sta_ssid_len;
        if (ble_is_connected == true) {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
        } else {
            ESP_LOGI(BLUFI_TAG, "BLUFI BLE is not connected yet\n");
        }

#endif
        // Check if the IP number if valid before firing events
        if (esp_ip4_addr1_16(&event->ip_info.ip)==169)
        {
            ESP_LOGW(TAG, "Got IP, but local one - not firing events");
        }
        else
        {
            led_connected();
            xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

void wifi_init_sta(void)
{
    ESP_LOGI(TAG, "wifi_init_sta started");
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_country_t wifi_country = {
        .cc = "CA",
        .schan = 1,
        .nchan = 11,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
#ifndef CONFIG_ESP_BLUFI_ENABLED
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
#endif
    ESP_ERROR_CHECK(esp_wifi_set_country(&wifi_country));
    ESP_ERROR_CHECK(esp_wifi_start());

#ifndef CONFIG_ESP_BLUFI_ENABLED
// WIFI Power management WIFI_PS_NONE not available when BLUFI is in use
#ifdef CONFIG_PM_ENABLE
    ESP_LOGI(TAG, "WIFI Power management configured");
    esp_wifi_set_ps(DEFAULT_PS_MODE);
#else
    ESP_LOGI(TAG, "WIFI Power management disabled");
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif
#endif

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // Start a thread to monitor the WIFI connection
    xTaskCreate(wifi_connected, THREAD_WIFI_NAME, THREAD_WIFI_STACKSIZE, NULL, THREAD_WIFI_PRIORITY, NULL);
}

#ifdef CONFIG_ESP_BLUFI_ENABLED
static esp_blufi_callbacks_t blufi_callbacks = {
    .event_cb = blufi_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};
#endif

void wifi_setup(void)
{
    ESP_LOGI(TAG, "wifi_setup started.");
#ifndef CONFIG_ESP_BLUFI_ENABLED
    if (sizeof(CONFIG_ESP_WIFI_SSID) == 0)
    {
        ESP_LOGE(TAG, "CONFIG_ESP_WIFI_SSID macro has a zero length! WIFI will not work.");
    }
#endif
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "wifi_setup finished.");

#ifdef CONFIG_ESP_BLUFI_ENABLED    
    ESP_LOGI(TAG, "blufli setup begin.");
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGI(TAG,"%s initialize bt controller failed: %s\n", __func__, esp_err_to_name(ret));
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG,"%s enable bt controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_blufi_host_and_cb_init(&blufi_callbacks);
    if (ret) {
        ESP_LOGE(TAG,"%s initialise failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG,"BLUFI VERSION %04x\n", esp_blufi_get_version());
    ESP_LOGI(TAG, "blufli setup end.");
#endif
}

void wifi_connect(void)
{
    wifi_enabled = true;
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
}

#ifdef CONFIG_ESP_BLUFI_ENABLED
static bluficmd_t **blufi_commands = NULL;
static int command_count = 0;

void register_command_set(bluficmd_t *commands[], int count)
{
    blufi_commands = commands;
    command_count = count;
    for (int i=0; i<count; i++) {
        ESP_LOGI(TAG, "Registered CMD: %s", blufi_commands[i]->command);
    }
}

#ifdef CONFIG_BT_DEVICE_NAME
// Unfortunately, the Espressif BluFi code hard codes the Bluetooth name for the device into the
// BluFi library, once advertising starts, one can't change the name. So, we copy the init code here
// to allow us to set the name outselves and then start advertising
// Copied directly from $IDFROOT/components/bt/common/btc/profile/esp/blufi/bluedroid_host/esp_blufi.c
// References to the structure below are here:
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_ble.html#_CPPv418esp_ble_adv_data_t
static uint8_t blufi_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
};

static esp_ble_adv_data_t blufi_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 16,
    .p_service_uuid = blufi_service_uuid128,
    .flag = 0x6,
};

#endif

static void bt_advertise() 
{
#ifdef CONFIG_BT_DEVICE_NAME
        // Overwrite the BT name set in esp_blufi_adv_start
        ESP_LOGI(BLUFI_TAG, "Bluetooth device name set to %s", CONFIG_BT_DEVICE_NAME);
        // esp_blufi_adv_start does these two lines in esp_blufi.c
        esp_ble_gap_set_device_name(CONFIG_BT_DEVICE_NAME);
        esp_ble_gap_config_adv_data(&blufi_adv_data);
#else
        // If the name if not defined, use the default init code and name
        esp_blufi_adv_start();
#endif        
}

static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    /* actually, should post to blufi_task handle the procedure,
     * now, as a example, we do it more simply */
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        ESP_LOGI(BLUFI_TAG, "BLUFI init finish");
        bt_advertise();
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        ESP_LOGI(BLUFI_TAG, "BLUFI deinit finish");
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        ESP_LOGI(BLUFI_TAG, "BLUFI ble connect");
        ble_is_connected = true;
        esp_blufi_adv_stop();
        blufi_security_init();
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        ESP_LOGI(BLUFI_TAG, "BLUFI ble disconnect");
        ble_is_connected = false;
        blufi_security_deinit();
        bt_advertise();
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        ESP_LOGI(BLUFI_TAG, "BLUFI Set WIFI opmode %d", param->wifi_mode.op_mode);
        ESP_ERROR_CHECK( esp_wifi_set_mode(param->wifi_mode.op_mode) );
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        ESP_LOGI(BLUFI_TAG, "BLUFI request wifi connect to AP");
        /* there is no wifi callback when the device has already connected to this wifi
        so disconnect wifi before connection.
        */
        esp_wifi_disconnect();
        esp_wifi_connect();
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        ESP_LOGI(BLUFI_TAG, "BLUFI requset wifi disconnect from AP");
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        ESP_LOGE(BLUFI_TAG, "BLUFI report error, error code %d", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;

        esp_wifi_get_mode(&mode);

        if (gl_sta_connected) {
            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, gl_sta_bssid, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = gl_sta_ssid;
            info.sta_ssid_len = gl_sta_ssid_len;
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
        } else {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, NULL);
        }
        ESP_LOGI(BLUFI_TAG, "BLUFI get wifi status from AP");
        break;
    }
    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        ESP_LOGI(BLUFI_TAG, "blufi close a gatt connection");
        esp_blufi_disconnect();
        break;
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        /* TODO */
        break;
	case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        sta_config.sta.bssid_set = 1;
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        ESP_LOGI(BLUFI_TAG, "Recv STA BSSID %s", sta_config.sta.ssid);
        break;
	case ESP_BLUFI_EVENT_RECV_STA_SSID:
        strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        ESP_LOGI(BLUFI_TAG, "Recv STA SSID %s", sta_config.sta.ssid);
        break;
	case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        ESP_LOGI(BLUFI_TAG, "Recv STA PASSWORD %s", sta_config.sta.password);
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
	case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
	case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
	case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
	case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
        ESP_LOGE(BLUFI_TAG, "Recv SOFTAP command but Ap Mode is not supported");
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_LIST:{
        wifi_scan_config_t scanConf = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = false
        };
        esp_wifi_scan_start(&scanConf, true);
        break;
    }
    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:{
        esp_log_buffer_hex("Custom Data", param->custom_data.data, param->custom_data.data_len);
        char *buffer = (char*)param->custom_data.data;
        buffer[param->custom_data.data_len] = '\0';
        if (custom_command_callback==NULL)
        {
            ESP_LOGI(TAG, "Ignoring custom cmd: %s", buffer);
        }
        else
        {
            ESP_LOGI(TAG, "Processing Custom cmd: %s", buffer);
            custom_command_callback(buffer);
        }
        break;
    }
	case ESP_BLUFI_EVENT_RECV_USERNAME:
	case ESP_BLUFI_EVENT_RECV_CA_CERT:
	case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
	case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
	case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
	case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
        // Ignore these events
        break;
    default:
        break;
    }
}
#endif

#endif
