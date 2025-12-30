#include <unity.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "test_dispatcher.h"
#include "test_state.h"

// Test runner main function
void app_main(void) {
    // Wait a bit for the serial port to be ready
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    UNITY_BEGIN();
    
    // Run dispatcher tests
    RUN_TEST(test_dispatcher_init);
    RUN_TEST(test_event_post_and_poll);
    RUN_TEST(test_state_get_set);
    RUN_TEST(test_event_queue_full);
    RUN_TEST(test_state_thread_safety);
    RUN_TEST(test_invalid_event_post);
    RUN_TEST(test_invalid_event_poll);
    RUN_TEST(test_invalid_state_ops);
    
    // Run state tests
    RUN_TEST(test_initial_state);
    RUN_TEST(test_state_sequence);
    RUN_TEST(test_device_state_transitions);
    RUN_TEST(test_ui_state_fields);
    RUN_TEST(test_message_buffers);
    RUN_TEST(test_state_immutability);
    RUN_TEST(test_wifi_state);
    RUN_TEST(test_audio_state);
    
    UNITY_END();
    
    // Keep task alive for monitor
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

