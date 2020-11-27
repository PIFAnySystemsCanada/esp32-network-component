/*
    WIFI component for ESP32

    Based on Espressif Sample code

    (C) 2020 Mark Buckaway - Apache License Version 2.0, January 2004
*/


#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void wifi_setup(void);
void wifi_connect(void);
void wifi_waitforconnect(void);

#ifdef __cplusplus
}
#endif
