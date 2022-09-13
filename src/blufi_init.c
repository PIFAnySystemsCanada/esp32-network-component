
#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_blufi.h"

#ifdef CONFIG_ESP_BLUFI_ENABLED
#include "blufi.h"

#if !CONFIG_BT_BLUEDROID_ENABLED
#error Bludriod is required for BluFi Support
#endif
#if !CONFIG_BT_ENABLED
#error Bluetooth must enabled for BluFi Support
#endif

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

static const char *BLUFI_INIT_TAG = "BLUFIINIT";

esp_err_t esp_blufi_host_init(void)
{
    int ret;
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(BLUFI_INIT_TAG,"%s init bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(BLUFI_INIT_TAG,"%s init bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ESP_LOGI(BLUFI_INIT_TAG,"BD ADDR: "ESP_BD_ADDR_STR"\n", ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));

    return ESP_OK;

}

esp_err_t esp_blufi_gap_register_callback(void)
{
   int rc;
   rc = esp_ble_gap_register_callback(esp_blufi_gap_event_handler);
    if(rc){
        return rc;
    }
    return esp_blufi_profile_init();
}

esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *blufi_callbacks)
{
    esp_err_t ret = ESP_OK;

    ret = esp_blufi_host_init();
    if (ret) {
        ESP_LOGE(BLUFI_INIT_TAG,"%s initialise host failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_blufi_register_callbacks(blufi_callbacks);
    if(ret){
        ESP_LOGE(BLUFI_INIT_TAG,"%s blufi register failed, error code = %x\n", __func__, ret);
        return ret;
    }

    ret = esp_blufi_gap_register_callback();
    if(ret){
        ESP_LOGE(BLUFI_INIT_TAG,"%s gap register failed, error code = %x\n", __func__, ret);
        return ret;
    }

    return ESP_OK;

}

#endif /* CONFIG_ESP_BLUFI_ENABLED */