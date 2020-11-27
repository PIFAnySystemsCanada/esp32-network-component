/*
    WIFI component for ESP32

    Based on Espressif Sample code

    (C) 2020 Mark Buckaway - Apache License Version 2.0, January 2004
*/


#pragma once

// WIFI Monitor Thread
#define THREAD_WIFI_NAME "wifi_connected"
#define THREAD_WIFI_STACKSIZE configMINIMAL_STACK_SIZE * 4
#define THREAD_WIFI_PRIORITY 4

#ifdef __cplusplus
extern "C" {
#endif

void wifi_setup(void);
void wifi_connect(void);
void wifi_waitforconnect(void);

#ifdef __cplusplus
}
#endif
