# Unit tests for core modules

idf_component_register(
    SRCS 
        "test_dispatcher.c"
        "test_state.c"
    INCLUDE_DIRS 
        "."
        "../main"
    REQUIRES 
        unity
        sx_core
        freertos
)
