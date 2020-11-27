# ESP-IDF WIFI component

## What is it?

This is a component for the ESP-IDF 4.1 or better.  It is intended for IOT situations were devices are tested in a lab first, and then deployed to the field. The module is designed to work where the WIFI connection is unstable.

It supports the following features:

* support for two SSID's (one for development, one for field)
* retry on connection failure or connection drop - expects the WIFI connection to be flakey.
* able to check if the WIFI connection has been established and working
* able to wait until the WIFI connection has been established

## Including the Component

The repo is designed to be included in a ESP-IDF application as a component. To add it as a company, change to your component directory, and add it as a git submodule:

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

Two functions can be defined:

```
#define LED_DISCONNECTED_FUNCTION functionname()
#define LED_CONNECTED_FUNCTION functionname2()
```

These functions are empty by default, but can be defined to enable controlling LED's when WIFI connects and disconnection. If either are defined, the include "led.h" is included in the code to pull in these functions. Proper callbacks will be implemented in the future.
