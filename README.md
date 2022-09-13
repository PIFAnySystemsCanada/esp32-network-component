# ESP-IDF Network component

## What is it?

This is a component for the ESP-IDF 4.4 or better.  It is intended for IOT situations were devices are tested in a lab first, and then deployed to the field. This module provides both ethernet and wifi functionality. The module is designed to work where the WIFI connection is unstable. The driver set supports setting the WIFI parameters in the field with the Espressif BluFi app saving the need to hard code them at build time. The option to hard code them is still available.

The WIFI driver supports the following features:

* Automatic configuation via the Espressif BluFi protocol and BluFi app (only the Bluedriod stack is supported)
* Manual configuration support for two SSID's (one for development, one for field)
* retry on connection failure or connection drop - expects the WIFI connection to be flakey.
* able to check if the WIFI connection has been established and working
* able to wait until the WIFI connection has been established
* support for two status LED's depending on if the WIFI is connected, dropped, reconnecting, etc

For the BluFi protocol, get the apps: [Android version](https://github.com/EspressifApp/EspBlufi) and [iOS version](https://itunes.apple.com/cn/app/espblufi/id1450614082?mt=8)
BluFi protocol info is here: [BluFi protocol](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/blufi.html?highlight=blufi#the-frame-formats-defined-in-blufi)

The ethernet driver supports the following features:

* able to check if the ethernet connection has been established and working
* able to wait until the ethernet connection has been established (waits for an IP number)
* support for two status LED's depending on if the Ethernet is connected and has an IP number

The WIFI and Ethernet drivers are based on the samples provided in the ESP-IDF, and some code added to handle restarting a WIFI connection and waiting for an IP number to be assigned.

## Including the Component

The repo is designed to be included in a ESP-IDF application as a component. To add it as a component, change to your component directory, and add it as a git submodule:

```bash
cd components
git submodule add git@github.com:PIFAnySystemsCanada/esp32-network-component.git network
```

### Configure Network the application

Start the command below to setup configuration:

```bash
idf.py menuconfig
```

Browse to the WIFI Configuration section, enable WIFI and answer the questions. Help is available to describe each item. Simularly, for Ethernet support,
enable Ethernet and answer the questions.

The WIFI section supports BluFi. This is an ESP32 WIFI configuration protocol used to setup the WIFI connection.

## Using the LILYGO T-Internet-POE board

The [LILYGO T-Internet-POE](https://www.aliexpress.com/item/4001122992446.html) board is a cheaper alternative to the Espressif ethernet board. For about $25, it has all the hardware to support ethernet connections with POE for power.

When configuring the ethernet driver in this module, select a chip type of LAN8720. The pin assignment defaults are fine but the PHY Address MUST be 0. The config should be as follows:

```
[*] Ethernet Enabled
        Ethernet Type (Internal EMAC)  --->
        Ethernet PHY Device (LAN8720)  --->
(23)    SMI MDC GPIO number
(18)    SMI MDIO GPIO number
(5)     PHY Reset GPIO number
(0)     PHY Address
```

Setting PHY Address to another other than 0 will cause the LAN8720 to fail on power up.

There is two options deep in the config however that if missed will cause the ethernet chip to fail to reset on start. The init of the ethernet driver (ethernet_driver_install call) will just timeout.

In the idf menuconfig:

* go into Component Config->Ethernet
* select "Support ESP32 internal EMAC controller"
* under "RMII clock mode", select "Output RMII clock from internal".
* A new option will appear: "RMII clock GPIO number". Make sure the GPIO number is 17 which is usually the default.
* Back up to the top to Component Config->ESP NETIF Adapter
* Set "Enable backward compatible tcpip_adapter interface" to Y or on

Missing this setting will not allow the LAN8720 chip to start.

## Using the Component

### Initialization

Use the following code in your app_main function:

```
    network_setup();
    network_connect();
```

esp_event_loop_create_default is required for wifi and is run from network_setup. Be aware some other components may try to init the event loop leading to an error you can safely ignore.

### WaitForConnect

In most applications, a network connection is necessary to start processing. This component provides:

```
    network_waitforconnect();
```

This function will cause the current thread to wait until the WIFI code has assigned an IP number. This function will wait indefinitely if no connection is found. 

## WIFI Station Configuration


The default configuration for the stations is:

'''
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

'''

The most important item here is the authmode. The driver has been setup to connect to AP that supports WPA or better. This is the minimum required. For our purposes, this was fine. A later version might expose the selection in the menuconfig setup. Additionally, scan all channels was selected over fastscan and we select AP's to scan by RSSI and not security settings. These can be changed by editing wifi.c, but should be sufficent in most cases.

### LED Callbacks

It is useful for a headless device to use status LEDs to indicate what is going on. Some ESP32 boards have built in LEDs, but adding GPIOs for external LED's is easy. The WIFI code features two callbacks. While they are intended to support changing status LEDs, they can be used for anything. However, they should not be used to check for a connection as the wifi_wififorconnected() is intended for this feature and uses a queue to check WIFI status.

```
set_led_connected_callback((void *callback)())
set_led_disconnected_callback((void *callback)())
```

Usage:

'''
void (*ptr_green)() = &led_green;
set_led_connected_callback(ptr_green);
'''
