# Integration tests for services

idf_component_register(
    SRCS 
        "test_audio_service.c"
        "test_chatbot_service.c"
    INCLUDE_DIRS 
        "."
        "../main"
    REQUIRES 
        unity
        sx_services
        sx_core
        freertos
)









