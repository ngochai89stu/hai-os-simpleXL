idf_component_register(
    SRCS
        "sx_platform.c"
        "sx_spi_bus_manager.c"
        "sx_platform_volume.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        driver
        esp_lcd
        espressif__esp_lcd_touch
        sx_codec_common
    PRIV_REQUIRES
        # Example driver, will be made configurable in later phases
        esp_lcd_st7796
        espressif__esp_lcd_touch_ft5x06
)

