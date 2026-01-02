# Phase 1: Add assets
file(GLOB ASSET_FILES "assets/*.c")

idf_component_register(
    SRCS
        "sx_ui_task.c"
        "ui_router.c"
        "ui_screen_registry.c"
        "ui_icons.c"
        "sx_lvgl_lock.c"
        "sx_lvgl_guard.c"  # P1.1: Runtime guard (Section 7.3 SIMPLEXL_ARCH v1.3)
        "ui_helpers/ui_animation_helpers.c"
        "ui_helpers/ui_theme_helpers.c"
        "ui_helpers/ui_qr_code_helpers.c"
        "ui_helpers/ui_image_helpers.c"
        "ui_helpers/ui_assets_wrapper.c"  # Wrapper để access assets (tuân theo SIMPLEXL_ARCH v1.3)
        "../sx_assets/generated/bootscreen_img.c"  # Generated image (chỉ sx_ui được compile vì include lvgl.h)
        "../sx_assets/generated/flashscreen_img.c"  # Generated image (chỉ sx_ui được compile vì include lvgl.h)
        "ui_components/ui_buttons.c"
        "ui_components/ui_list.c"
        "ui_components/ui_slider.c"
        "screens/screen_common.c"
        "screens/screen_boot.c"
        "screens/screen_flash.c"
        "screens/screen_home.c"
        "screens/screen_chat.c"
        "screens/screen_settings.c"
        "screens/screen_wakeword_feedback.c"
        "screens/screen_music_online_list.c"
        "screens/screen_music_player.c"
        "screens/screen_music_player_spectrum.c"
        "screens/screen_music_player_list.c"
        "screens/screen_radio.c"
        "screens/screen_sd_card_music.c"
        "screens/screen_ir_control.c"
        "screens/screen_wifi_setup.c"
        "screens/screen_display_setting.c"
        "screens/screen_bluetooth_setting.c"
        "screens/screen_equalizer.c"
        "screens/screen_ota.c"
        "screens/screen_about.c"
        "screens/screen_google_navigation.c"
        "screens/screen_ac_control.c"
        "screens/screen_system_info.c"
        "screens/screen_quick_settings.c"
        "screens/screen_permission.c"
        "screens/screen_screensaver.c"
        "screens/screen_screensaver_setting.c"
        "screens/screen_audio_effects.c"
        "screens/screen_startup_image_picker.c"
        "screens/screen_voice_settings.c"
        "screens/screen_network_diagnostic.c"
        "screens/screen_snapshot_manager.c"
        "screens/screen_diagnostics.c"
        "screens/screen_introspection.c"
        "screens/screen_dev_console.c"
        "screens/screen_touch_debug.c"
        "screens/register_all_screens.c"
        "screens/screen_display_helper.c"  # P0.1: Display service event handler
        "sx_screen_if.c"  # P1.4: Screen lifecycle interface (Section 6.2 SIMPLEXL_ARCH v1.3)
        "ui_verify/sx_ui_verify.c"
        "ui_animations.c"
        ${ASSET_FILES}
    INCLUDE_DIRS
        "include"
        "ui_verify"
        "assets"
        "ui_helpers"
        "ui_components"
        "../sx_assets/generated"  # Generated image files (chỉ sx_ui được include)
    PRIV_INCLUDE_DIRS
        "../sx_services/include"
    REQUIRES
        sx_core
        sx_platform
        sx_assets
        sx_services
        esp_lvgl_port
    PRIV_REQUIRES
        esp_http_client
        json
        sx_services
        # These are needed because service headers included in .c files depend on them
)

# Add compile-time guard for sx_lvgl.h (Section 7.5 SIMPLEXL_ARCH v1.3)
# This ensures that only sx_ui component can include LVGL headers
target_compile_definitions(${COMPONENT_LIB} PRIVATE SX_COMPONENT_SX_UI=1)

# Workaround for static archive resolution / dependency cycles between component libs
# Giúp linker resolve symbol qua nhiều pass khi có vòng phụ thuộc giữa các static libs (sx_ui <-> sx_services)
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 3)

# P1.1: Add runtime assert config (Section 7.3 SIMPLEXL_ARCH v1.3)
# CONFIG_SX_ENABLE_UI_THREAD_ASSERT is automatically defined by Kconfig
