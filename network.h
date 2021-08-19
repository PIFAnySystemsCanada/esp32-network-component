#pragma once

#include "wifi.h"
#include "ethernet.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up the wifi API and must be called once and only once per application. Typically called
 * in the app_main function and must be called before calling wifi_connect.
 */
void network_setup(void);

/**
 * @brief Monitors the WIFI and Ethernet queues and waits for an IP number to be assigned to one or both. If already connected,
 * the call just exits. This function can be used to ensure a connection is setup and an IP number has been
 * assigned.
 */
void network_waitforconnect(void);

/**
 * Sets the callback when the WIFI and/or Ethernet connection is made and an IP number is assigned. It is intended to change
 * the status of LED's,  but can be used for anything. The callback should do processing quickly and return.
 * If the callback takes to long, it will causes errors.
 */
void set_network_led_disconnected_callback(void (*callback)());

/**
 * Sets the callback when the WIFI connection is lost. It is intended to change
 * the status of LED's,  but can be used for anything. The callback should do processing quickly and return.
 * If the callback takes to long, it will causes errors.
 */
void set_network_led_connected_callback(void (*callback)());

#ifdef __cplusplus
}
#endif
