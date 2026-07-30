idf_component_register(SRCS "main.c" "brain_config.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_http_server nvs_flash driver esp_netif esp_event esp_eth
             esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_spi esp_timer
             esp_driver_usb_serial_jtag
             mouth_espnow)
