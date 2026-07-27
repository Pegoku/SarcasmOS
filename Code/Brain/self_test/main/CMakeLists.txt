idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_i2s
        esp_driver_spi
        esp_driver_usb_serial_jtag
        esp_event
        esp_netif
        esp_system
        esp_wifi
        nvs_flash
        spi_flash
)
