# ESP-IDF Network component

## What is it?

This is a component for the ESP-IDF 4.x.  5.x is not yet supported. It is intended for IOT situations were devices are tested in a lab first, and then deployed to the field. This module provides both ethernet and wifi functionality. The module is designed to work where the WIFI connection is unstable.

The WIFI driver supports the following features:

* BluFi support for configuring WIFI SSID and credentials on the fly
* BluFi callback support to commands using BluFi custom data
* hard coded support for two SSID's (one for development, one for field) with credentials
* retry on connection failure or connection drop - expects the WIFI connection to be flakey.
* able to check if the WIFI connection has been established and working
* able to wait (pause startup) until the WIFI connection has been established (useful for NTP time support, etc.)
* support for two status LED's depending on if the WIFI is connected, dropped, reconnecting, etc

The ethernet driver supports the following features:

* able to check if the ethernet connection has been established and working
* able to wait until the ethernet connection has been established (waits for an IP number)
* support for two status LED's depending on if the Ethernet is connected and has an IP number

The WIFI and Ethernet drivers are based on the samples provided in the ESP-IDF, and some code added to handle restarting a WIFI connection and waiting for an IP number to be assigned.

For more information on using this component, see the [WIKI](https://github.com/PIFAnySystemsCanada/esp32-network-component/wiki).
