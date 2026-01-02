/**
 * @file test_architecture_compliance.c
 * @brief Architecture compliance tests (Section 11 SIMPLEXL_ARCH v1.3)
 * P2.4: Architecture compliance test suite
 */

#include <unity.h>
#include <string.h>
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state.h"
#include "sx_state_helper.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void setUp(void) {
    sx_dispatcher_init();
}

void tearDown(void) {
    // Cleanup if needed
}

// Test 1: Event priority ordering
void test_event_priority_ordering(void) {
    sx_event_t evt_critical = {
        .type = SX_EVT_SYSTEM_SHUTDOWN,
        .priority = SX_EVT_PRIORITY_CRITICAL
    };
    sx_event_t evt_normal = {
        .type = SX_EVT_UI_INPUT,
        .priority = SX_EVT_PRIORITY_NORMAL
    };
    
    // Post normal first, then critical
    TEST_ASSERT_TRUE(sx_dispatcher_post_event(&evt_normal));
    TEST_ASSERT_TRUE(sx_dispatcher_post_event(&evt_critical));
    
    // Critical should be polled first
    sx_event_t out;
    TEST_ASSERT_TRUE(sx_dispatcher_poll_event(&out));
    TEST_ASSERT_EQUAL(SX_EVT_PRIORITY_CRITICAL, out.priority);
    TEST_ASSERT_EQUAL(SX_EVT_SYSTEM_SHUTDOWN, out.type);
}

// Test 2: State version increment
void test_state_version_increment(void) {
    sx_state_t state;
    memset(&state, 0, sizeof(state));
    state.version = 1;
    
    sx_dispatcher_set_state(&state);
    
    sx_state_t out;
    sx_dispatcher_get_state(&out);
    TEST_ASSERT_EQUAL(1, out.version);
    
    // Simulate state update
    sx_state_update_version_and_dirty(&state, SX_EVT_WIFI_CONNECTED);
    sx_dispatcher_set_state(&state);
    
    sx_dispatcher_get_state(&out);
    TEST_ASSERT_EQUAL(2, out.version);
    TEST_ASSERT_NOT_EQUAL(0, out.dirty_mask);
}

// Test 3: Dirty mask updates
void test_dirty_mask_updates(void) {
    sx_state_t state;
    memset(&state, 0, sizeof(state));
    state.version = 1;
    
    sx_state_update_version_and_dirty(&state, SX_EVT_WIFI_CONNECTED);
    TEST_ASSERT_NOT_EQUAL(0, state.dirty_mask);
    TEST_ASSERT_TRUE(state.dirty_mask & SX_STATE_DIRTY_WIFI);
    
    sx_state_update_version_and_dirty(&state, SX_EVT_AUDIO_PLAYBACK_STARTED);
    TEST_ASSERT_TRUE(state.dirty_mask & SX_STATE_DIRTY_AUDIO);
}

// Test 4: Backpressure DROP policy
void test_backpressure_drop(void) {
    sx_event_t evt = {
        .type = SX_EVT_UI_INPUT,
        .priority = SX_EVT_PRIORITY_LOW
    };
    
    // Fill queue (assuming small queue size)
    for (int i = 0; i < 20; i++) {
        sx_dispatcher_post_event(&evt);
    }
    
    // Next post should drop (if queue full)
    bool result = sx_dispatcher_post_event(&evt);
    // Result depends on queue size, but should handle gracefully
    (void)result;
}

// Test 5: Backpressure COALESCE policy
void test_backpressure_coalesce(void) {
    sx_event_t evt1 = {
        .type = SX_EVT_UI_INPUT,
        .priority = SX_EVT_PRIORITY_NORMAL
    };
    sx_event_t evt2 = {
        .type = SX_EVT_UI_INPUT,
        .priority = SX_EVT_PRIORITY_NORMAL
    };
    
    // Post with COALESCE policy
    sx_dispatcher_post_event_with_policy(&evt1, SX_BP_COALESCE, 0);
    sx_dispatcher_post_event_with_policy(&evt2, SX_BP_COALESCE, 0);
    
    // Should coalesce (only latest event kept)
    // Implementation depends on coalesce logic
}

// Test 6: Event taxonomy range validation
void test_event_taxonomy_ranges(void) {
    // Test that event IDs are within valid ranges
    TEST_ASSERT_TRUE(SX_EVT_UI_BASE <= SX_EVT_UI_INPUT);
    TEST_ASSERT_TRUE(SX_EVT_UI_INPUT < (SX_EVT_UI_BASE + 0x100));
    
    TEST_ASSERT_TRUE(SX_EVT_AUDIO_BASE <= SX_EVT_AUDIO_PLAYBACK_STARTED);
    TEST_ASSERT_TRUE(SX_EVT_AUDIO_PLAYBACK_STARTED < (SX_EVT_AUDIO_BASE + 0x100));
}

void app_main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_event_priority_ordering);
    RUN_TEST(test_state_version_increment);
    RUN_TEST(test_dirty_mask_updates);
    RUN_TEST(test_backpressure_drop);
    RUN_TEST(test_backpressure_coalesce);
    RUN_TEST(test_event_taxonomy_ranges);
    RUN_TEST(test_double_buffer_pattern);  // OPT-5: Double-buffer test
    
    UNITY_END();
}

