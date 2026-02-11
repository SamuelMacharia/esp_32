idf_component_register(SRCS "system.c" "capture.c" "wifi.c" "power.c" "mqtt.c" "main.c"
                       PRIV_REQUIRES spi_flash esp_http_server mqtt
                       INCLUDE_DIRS ""
                       REQUIRES esp_psram esp_wifi esp_timer nvs_flash esp_http_server )
