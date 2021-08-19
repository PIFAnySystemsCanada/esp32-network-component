idf_component_register(SRCS  "wifi.c" "ethernet.c" "network.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash)
