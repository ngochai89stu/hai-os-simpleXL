#include <unity.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "test_dispatcher.h"
#include "test_state.h"

// External test declarations
extern void test_event_handler_init(void);
extern void test_event_handler_register(void);
extern void test_event_handler_unregister(void);
extern void test_event_handler_process(void);
extern void test_event_handler_unregistered(void);
extern void test_event_handler_invalid_register(void);
extern void test_event_handler_invalid_process(void);

extern void test_event_priority_routing(void);
extern void test_event_default_priority(void);
extern void test_event_priority_queue_capacity(void);
extern void test_event_low_priority(void);

extern void test_string_pool_metrics_init(void);
extern void test_string_pool_metrics_hits(void);
extern void test_string_pool_metrics_misses(void);
extern void test_string_pool_metrics_fallbacks(void);
extern void test_string_pool_metrics_peak(void);
extern void test_string_pool_metrics_reset(void);

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
    
    // Run event handler registry tests
    RUN_TEST(test_event_handler_init);
    RUN_TEST(test_event_handler_register);
    RUN_TEST(test_event_handler_unregister);
    RUN_TEST(test_event_handler_process);
    RUN_TEST(test_event_handler_unregistered);
    RUN_TEST(test_event_handler_invalid_register);
    RUN_TEST(test_event_handler_invalid_process);
    
    // Run event priority tests
    RUN_TEST(test_event_priority_routing);
    RUN_TEST(test_event_default_priority);
    RUN_TEST(test_event_priority_queue_capacity);
    RUN_TEST(test_event_low_priority);
    
    // Run string pool metrics tests
    RUN_TEST(test_string_pool_metrics_init);
    RUN_TEST(test_string_pool_metrics_hits);
    RUN_TEST(test_string_pool_metrics_misses);
    RUN_TEST(test_string_pool_metrics_fallbacks);
    RUN_TEST(test_string_pool_metrics_peak);
    RUN_TEST(test_string_pool_metrics_reset);
    
    UNITY_END();
    
    // Keep task alive for monitor
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

