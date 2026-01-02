idf_component_register(
    SRCS
        "sx_bootstrap.c"
        "sx_dispatcher.c"
        "sx_orchestrator.c"
        "sx_event_string_pool.c"
        "sx_lazy_loader.c"
        "sx_event_handler.c"
        "sx_error_handler.c"
        "sx_event_handlers/ui_input_handler.c"
        "sx_event_handlers/chatbot_handler.c"
        "sx_event_handlers/audio_handler.c"
        "sx_event_handlers/radio_handler.c"
        "sx_event_handlers/event_handlers_ota.c"
        "sx_service_if.c"  # P1.4: Service lifecycle interface (Section 6.1 SIMPLEXL_ARCH v1.3)
        "sx_metrics.c"  # P2.6: Metrics collection system (Section 9.1 SIMPLEXL_ARCH v1.3)
        "sx_selftest.c"  # Smoke test module
    INCLUDE_DIRS
        "include"
        "sx_event_handlers"
    PRIV_INCLUDE_DIRS
        "../sx_services/include"
        "../sx_protocol/include"
    REQUIRES
        nvs_flash
        esp_event
        sx_platform
        sx_ui
        sx_assets
        # Phase 3: Removed sx_services from REQUIRES to eliminate circular dependency
        # Event handlers can still include service headers in .c files via PRIV_INCLUDE_DIRS
        # ESP-IDF will link through sx_services → sx_core dependency
    PRIV_REQUIRES
        esp_http_client
        json
        # These are needed because service headers included in .c files depend on them
        # PRIV_REQUIRES ensures they're only available during compilation, not as public API
)
