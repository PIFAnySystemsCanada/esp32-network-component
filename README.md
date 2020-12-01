# ESP-IDF WIFI component

## What is it?

This is a component for the ESP-IDF 4.1 or better.  It is intended for IOT situations were devices are tested in a lab first, and then deployed to the field. The module is designed to work where the WIFI connection is unstable.

It supports the following features:

* support for two SSID's (one for development, one for field)
* retry on connection failure or connection drop - expects the WIFI connection to be flakey.
* able to check if the WIFI connection has been established and working
* able to wait until the WIFI connection has been established

## Including the Component

The repo is designed to be included in a ESP-IDF application as a component. To add it as a component, change to your component directory, and add it as a git submodule:

```bash
cd components
git submodule add git@github.com:mbuckaway/esp32-wifi-component.git wifi
```

### Configure WIFI the application

Start the command below to setup configuration:

```bash
idf.py menuconfig
```

Browse to the WIFI Configuration section and answer the questions. Help is available to describe each item.

## Using the Component

### Initialization

Use the following code in your app_main function:

```
    wifi_setup();
    wifi_connect();
```

esp_event_loop_create_default is required for wifi. Be aware some other components may try to init the event loop leading to an error you can safely ignore.

### WaitForConnect

In most applications, a network connection is necessary to start processing. This component provides:

```
    wifi_waitforconnect();
```

This function will cause the current thread to wait until the WIFI code has assigned an IP number. This function will wait indefinitely if no connection is found. 

### LED Callbacks

It is useful for a headless device to use status LEDs to indicate what is going on. Some ESP32 boards have built in LEDs, but adding GPIOs for external LED's is easy. The WIFI code features two callbacks. While they are intended to support changing status LEDs, they can be used for anything. However, they should not be used to check for a connection as the wifi_wififorconnected() is intended for this feature and uses a queue to check WIFI status.

```
set_led_connected_callback((void *callback)())
set_led_disconnected_callback((void *callback)())
```
