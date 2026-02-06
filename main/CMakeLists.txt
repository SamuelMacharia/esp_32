idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash esp_http_server
                       INCLUDE_DIRS ""
                       REQUIRES esp_psram esp_wifi esp_timer nvs_flash esp_http_server)
