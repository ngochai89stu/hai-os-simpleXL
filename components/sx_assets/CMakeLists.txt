idf_component_register(
    SRCS
        "sx_assets.c"
        "generated/bootscreen_img.c"
        "generated/flashscreen_img.c"
    INCLUDE_DIRS
        "include"
        "generated"
    REQUIRES
        driver
        fatfs
        sdmmc
        esp_lvgl_port
)


