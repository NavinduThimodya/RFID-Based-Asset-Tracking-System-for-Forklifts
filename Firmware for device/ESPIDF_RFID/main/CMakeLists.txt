idf_component_register(SRCS "RFID_Asset_Tracking_Device.c"
                    INCLUDE_DIRS "."
                    REQUIRES PN532 esp_event nvs_flash esp_wifi mqtt esp_driver_gpio esp_driver_spi esp_http_server)
