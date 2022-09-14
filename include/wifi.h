/*
    WIFI component for ESP32

    Based on Espressif Sample code

    (C) 2020 Mark Buckaway - Apache License Version 2.0, January 2004
*/


#pragma once

#if CONFIG_ESP_WIFI_ENABLED

// WIFI Monitor Thread
#define THREAD_WIFI_NAME "wifi_connected"
#define THREAD_WIFI_STACKSIZE configMINIMAL_STACK_SIZE * 4
#define THREAD_WIFI_PRIORITY 4

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up the wifi API and must be called once and only once per application. Typically called
 * in the app_main function and must be called before calling wifi_connect.
 */
void wifi_setup(void);

/**
 * @brief Setup up the WIFI api to connect to a station and starts the wifi monitoring thread. This must
 * not be called more than once in an applicaton unless wifi_disable is called. No checks are made to 
 * ensure proper call order.
 */
void wifi_connect(void);

/**
 * @brief Monitors the wifi queue and waits for the connection to an AP to be created. If already connected,
 * the call just exits. This function can be used to ensure a connection to an AP and an IP number has been
 * assigned.
 */
void wifi_waitforconnect(void);

/**
 * @brief Disables the wifi reconnect thread and causes if to exit when the wifi connnection drops.
 * The function is intended to be called when the device is shutdown or wifi_discconnect is called
 * when reconnection is not desired. wifi_connect must be called again to setup the wifi connection.
 */
void wifi_disable();

/**
 * @brief Wrapper around esp_wifi_disconnect() and causes the WIFI connection to drop. Used in conjunction
 * with wifi_disable, it can stop wifi completely.
 */
void wifi_disconnect(void);

/**
 * Sets the callback when the WIFI connection is made and an IP number is assigned. It is intended to change
 * the status of LED's,  but can be used for anything. The callback should do processing quickly and return.
 * If the callback takes to long, it will causes errors.
 */

void set_wifi_led_connected_callback(void (*callback)());

/**
 * Sets the callback when the WIFI connection is lost. It is intended to change
 * the status of LED's,  but can be used for anything. The callback should do processing quickly and return.
 * If the callback takes to long, it will causes errors.
 */

void set_wifi_led_disconnected_callback(void (*callback)());


#ifdef __cplusplus
}
#endif

#endif
