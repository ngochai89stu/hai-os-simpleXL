# Tuân theo SIMPLEXL_ARCH v1.3:
# - sx_assets KHÔNG được include LVGL
# - Generated files (bootscreen_img.c, flashscreen_img.c) include lvgl.h
#   nên chúng được move sang sx_ui component (ui_assets_wrapper.c)
# - sx_assets chỉ quản lý raw data và SD card assets

idf_component_register(
    SRCS
        "sx_assets.c"
        # KHÔNG include generated/bootscreen_img.c và generated/flashscreen_img.c
        # vì chúng include lvgl.h - vi phạm kiến trúc
        # Chúng được access qua sx_ui/ui_assets_wrapper.c
    INCLUDE_DIRS
        "include"
        # KHÔNG include "generated" vì generated headers include lvgl.h
    REQUIRES
        driver
        fatfs
        sdmmc
        # KHÔNG REQUIRES esp_lvgl_port - tuân theo SIMPLEXL_ARCH v1.3
)


