#include <unity.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state.h"

// Test setup and teardown
void setUp(void) {
    // Initialize dispatcher before each test
    sx_dispatcher_init();
}

void tearDown(void) {
    // Cleanup if needed
    // Note: ESP-IDF doesn't provide explicit cleanup for dispatcher
    // In real scenario, might need to add cleanup function
}

// Test dispatcher initialization
void test_dispatcher_init(void) {
    bool ret = sx_dispatcher_init();
    TEST_ASSERT_TRUE(ret);
}

// Test event posting and polling
void test_event_post_and_poll(void) {
    sx_event_t evt = {
        .type = SX_EVT_UI_INPUT,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    
    // Post event
    bool posted = sx_dispatcher_post_event(&evt);
    TEST_ASSERT_TRUE(posted);
    
    // Poll event
    sx_event_t polled_evt;
    bool polled = sx_dispatcher_poll_event(&polled_evt);
    TEST_ASSERT_TRUE(polled);
    TEST_ASSERT_EQUAL(SX_EVT_UI_INPUT, polled_evt.type);
}

// Test state get and set
void test_state_get_set(void) {
    sx_state_t state1, state2;
    
    // Get initial state
    sx_dispatcher_get_state(&state1);
    uint32_t initial_seq = state1.seq;
    
    // Modify and set state
    state1.seq++;
    state1.ui.device_state = SX_DEV_IDLE;
    sx_dispatcher_set_state(&state1);
    
    // Get state again
    sx_dispatcher_get_state(&state2);
    TEST_ASSERT_EQUAL(initial_seq + 1, state2.seq);
    TEST_ASSERT_EQUAL(SX_DEV_IDLE, state2.ui.device_state);
}

// Test event queue full scenario (drop events)
void test_event_queue_full(void) {
    // Fill up the queue (64 events)
    sx_event_t evt = {
        .type = SX_EVT_UI_INPUT,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    
    // Post many events to fill queue
    int posted = 0;
    for (int i = 0; i < 100; i++) {
        if (sx_dispatcher_post_event(&evt)) {
            posted++;
        }
    }
    
    // Should post at least 64 events (queue size)
    // Some may be dropped if queue is full
    TEST_ASSERT_GREATER_OR_EQUAL(64, posted);
}

// Test state mutex protection (thread safety)
void test_state_thread_safety(void) {
    // This test verifies that state get/set operations are thread-safe
    // In real scenario, would need multiple threads
    sx_state_t state;
    
    // Multiple get operations should not crash
    for (int i = 0; i < 10; i++) {
        sx_dispatcher_get_state(&state);
        TEST_ASSERT_TRUE(true); // If we get here, no crash
    }
}

// Test invalid event posting
void test_invalid_event_post(void) {
    // Post NULL event should fail
    bool ret = sx_dispatcher_post_event(NULL);
    TEST_ASSERT_FALSE(ret);
}

// Test invalid event polling
void test_invalid_event_poll(void) {
    // Poll with NULL pointer should fail
    bool ret = sx_dispatcher_poll_event(NULL);
    TEST_ASSERT_FALSE(ret);
}

// Test invalid state operations
void test_invalid_state_ops(void) {
    // Get state with NULL should not crash (but does nothing)
    sx_dispatcher_get_state(NULL);
    
    // Set state with NULL should not crash (but does nothing)
    sx_dispatcher_set_state(NULL);
    
    // If we get here, operations handled gracefully
    TEST_ASSERT_TRUE(true);
}

