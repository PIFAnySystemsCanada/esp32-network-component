/*
    Ethernet component for ESP32

    Based on Espressif Sample code

    (C) 2021 Mark Buckaway - Apache License Version 2.0, January 2004
*/

#pragma once

#if CONFIG_ESP_ETHERNET_ENABLED

// Ethernet Monitor Thread
#define THREAD_ETHERNET_NAME "ethernet_connected"
#define THREAD_ETHERNET_STACKSIZE configMINIMAL_STACK_SIZE * 4
#define THREAD_ETHERNET_PRIORITY 4

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up the wifi API and must be called once and only once per application. Typically called
 * in the app_main function and must be called before calling wifi_connect.
 */
void ethernet_setup(void);

/**
 * @brief Monitors the wifi queue and waits for the connection to an AP to be created. If already connected,
 * the call just exits. This function can be used to ensure a connection to an AP and an IP number has been
 * assigned.
 */
void ethernet_waitforconnect(void);

/**
 * Sets the callback when the Ethernet connection is made and an IP number is assigned. It is intended to change
 * the status of LED's,  but can be used for anything. The callback should do processing quickly and return.
 * If the callback takes to long, it will causes errors.
 */

void set_ethernet_led_connected_callback(void (*callback)());

/**
 * Sets the callback when the Ethernet connection is lost. It is intended to change
 * the status of LED's,  but can be used for anything. The callback should do processing quickly and return.
 * If the callback takes to long, it will causes errors.
 */

void set_ethernet_led_disconnected_callback(void (*callback)());


#ifdef __cplusplus
}
#endif

#endif
