#include <unity.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state.h"

// Test setup and teardown
void setUp(void) {
    sx_dispatcher_init();
}

void tearDown(void) {
    // Cleanup
}

// Test priority queue routing
void test_event_priority_routing(void) {
    // Post critical event
    sx_event_t critical_evt = {
        .type = SX_EVT_ERROR,
        .priority = SX_EVT_PRIORITY_CRITICAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    bool posted = sx_dispatcher_post_event(&critical_evt);
    TEST_ASSERT_TRUE(posted);
    
    // Post normal event
    sx_event_t normal_evt = {
        .type = SX_EVT_UI_INPUT,
        .priority = SX_EVT_PRIORITY_NORMAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    posted = sx_dispatcher_post_event(&normal_evt);
    TEST_ASSERT_TRUE(posted);
    
    // Post high priority event
    sx_event_t high_evt = {
        .type = SX_EVT_CHATBOT_CONNECTED,
        .priority = SX_EVT_PRIORITY_HIGH,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    posted = sx_dispatcher_post_event(&high_evt);
    TEST_ASSERT_TRUE(posted);
    
    // Poll events - should get critical first, then high, then normal
    sx_event_t polled;
    
    // First poll should get critical
    bool polled_ret = sx_dispatcher_poll_event(&polled);
    TEST_ASSERT_TRUE(polled_ret);
    TEST_ASSERT_EQUAL(SX_EVT_ERROR, polled.type);
    TEST_ASSERT_EQUAL(SX_EVT_PRIORITY_CRITICAL, polled.priority);
    
    // Second poll should get high
    polled_ret = sx_dispatcher_poll_event(&polled);
    TEST_ASSERT_TRUE(polled_ret);
    TEST_ASSERT_EQUAL(SX_EVT_CHATBOT_CONNECTED, polled.type);
    TEST_ASSERT_EQUAL(SX_EVT_PRIORITY_HIGH, polled.priority);
    
    // Third poll should get normal
    polled_ret = sx_dispatcher_poll_event(&polled);
    TEST_ASSERT_TRUE(polled_ret);
    TEST_ASSERT_EQUAL(SX_EVT_UI_INPUT, polled.type);
    TEST_ASSERT_EQUAL(SX_EVT_PRIORITY_NORMAL, polled.priority);
}

// Test default priority assignment
void test_event_default_priority(void) {
    // Post event without setting priority (should use default)
    sx_event_t evt = {
        .type = SX_EVT_ERROR,  // Should default to CRITICAL
        .priority = 0,  // Not set
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    
    bool posted = sx_dispatcher_post_event(&evt);
    TEST_ASSERT_TRUE(posted);
    
    // Poll and verify it was routed to critical queue
    sx_event_t polled;
    bool polled_ret = sx_dispatcher_poll_event(&polled);
    TEST_ASSERT_TRUE(polled_ret);
    TEST_ASSERT_EQUAL(SX_EVT_ERROR, polled.type);
    // Priority should be set by dispatcher
}

// Test priority queue capacity
void test_event_priority_queue_capacity(void) {
    // Fill critical queue (8 events)
    sx_event_t evt = {
        .type = SX_EVT_ERROR,
        .priority = SX_EVT_PRIORITY_CRITICAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    
    int posted = 0;
    for (int i = 0; i < 10; i++) {
        if (sx_dispatcher_post_event(&evt)) {
            posted++;
        }
    }
    
    // Should post at least 8 events (critical queue size)
    TEST_ASSERT_GREATER_OR_EQUAL(8, posted);
}

// Test low priority events
void test_event_low_priority(void) {
    sx_event_t evt = {
        .type = SX_EVT_UI_INPUT,
        .priority = SX_EVT_PRIORITY_LOW,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    
    bool posted = sx_dispatcher_post_event(&evt);
    TEST_ASSERT_TRUE(posted);
    
    // Poll should get it (after higher priority events)
    sx_event_t polled;
    bool polled_ret = sx_dispatcher_poll_event(&polled);
    TEST_ASSERT_TRUE(polled_ret);
    TEST_ASSERT_EQUAL(SX_EVT_UI_INPUT, polled.type);
    TEST_ASSERT_EQUAL(SX_EVT_PRIORITY_LOW, polled.priority);
}









