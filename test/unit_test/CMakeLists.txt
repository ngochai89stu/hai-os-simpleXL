# Unit tests for core modules

idf_component_register(
    SRCS 
        "test_dispatcher.c"
        "test_state.c"
        "test_event_handler.c"
        "test_event_priority.c"
        "test_string_pool_metrics.c"
        "test_architecture_compliance.c"  # P2.4: Architecture compliance tests
        "test_event_performance.c"  # OPT-4: Performance tests
    INCLUDE_DIRS 
        "."
        "../main"
    REQUIRES 
        unity
        sx_core
        freertos
)
