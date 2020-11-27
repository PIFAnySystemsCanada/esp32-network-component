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
