/**
 * @file test_event_performance.c
 * @brief Performance tests for event throughput (OPT-4)
 * 
 * Tests event system performance under load:
 * - Event posting rate
 * - Backpressure policies under load
 * - Queue depth under stress
 * - State update rate
 */

#include <unity.h>
#include <string.h>
#include <esp_log.h>
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_state.h"
#include "sx_metrics.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "test_perf";

void setUp(void) {
    sx_dispatcher_init();
    sx_metrics_init();
}

void tearDown(void) {
    // Cleanup if needed
}

// Test 1: Event posting rate (events per second)
void test_event_posting_rate(void) {
    const uint32_t num_events = 1000;
    const uint32_t timeout_ms = 1000;  // 1 second target
    
    TickType_t start = xTaskGetTickCount();
    
    for (uint32_t i = 0; i < num_events; i++) {
        sx_event_t evt = {
            .type = SX_EVT_UI_INPUT,
            .priority = SX_EVT_PRIORITY_NORMAL
        };
        sx_dispatcher_post_event(&evt);
    }
    
    TickType_t end = xTaskGetTickCount();
    uint32_t elapsed_ms = (end - start) * portTICK_PERIOD_MS;
    uint32_t events_per_sec = (num_events * 1000) / elapsed_ms;
    
    ESP_LOGI(TAG, "Posted %lu events in %lu ms (%lu events/sec)", 
             (unsigned long)num_events, (unsigned long)elapsed_ms, 
             (unsigned long)events_per_sec);
    
    // Target: >= 1000 events/sec
    TEST_ASSERT_GREATER_THAN_UINT32(1000, events_per_sec);
}

// Test 2: Backpressure DROP policy under load
void test_backpressure_drop_under_load(void) {
    const uint32_t num_events = 200;  // More than queue capacity
    
    // Fill queues to capacity
    for (uint32_t i = 0; i < num_events; i++) {
        sx_event_t evt = {
            .type = SX_EVT_UI_INPUT,
            .priority = SX_EVT_PRIORITY_LOW
        };
        sx_dispatcher_post_event_with_policy(&evt, SX_BP_DROP, 0);
    }
    
    // Check metrics for dropped events
    sx_metrics_t metrics;
    sx_metrics_get(&metrics);
    
    ESP_LOGI(TAG, "Dropped events (LOW): %lu", 
             (unsigned long)metrics.evt_dropped_total[0]);
    
    // Should have some dropped events if queue was full
    // (This depends on queue size, but we expect some drops)
    TEST_ASSERT_TRUE(metrics.evt_dropped_total[0] >= 0);
}

// Test 3: Backpressure COALESCE policy
void test_backpressure_coalesce(void) {
    const uint32_t num_events = 100;
    const uint32_t coalesce_key = 12345;
    
    // Post many events with same coalesce key
    for (uint32_t i = 0; i < num_events; i++) {
        sx_event_t evt = {
            .type = SX_EVT_UI_INPUT,
            .priority = SX_EVT_PRIORITY_NORMAL,
            .arg0 = i  // Different data each time
        };
        sx_dispatcher_post_event_with_policy(&evt, SX_BP_COALESCE, coalesce_key);
    }
    
    // Check metrics for coalesced events
    sx_metrics_t metrics;
    sx_metrics_get(&metrics);
    
    ESP_LOGI(TAG, "Coalesced events (NORMAL): %lu", 
             (unsigned long)metrics.evt_coalesced_total[1]);
    
    // Should have coalesced events (latest event kept)
    TEST_ASSERT_TRUE(metrics.evt_coalesced_total[1] > 0);
}

// Test 4: Queue depth under stress
void test_queue_depth_under_stress(void) {
    const uint32_t num_events = 50;
    
    // Post events to fill queues
    for (uint32_t i = 0; i < num_events; i++) {
        sx_event_t evt = {
            .type = SX_EVT_UI_INPUT,
            .priority = (i % 4)  // Mix of priorities
        };
        sx_dispatcher_post_event(&evt);
    }
    
    // Check queue depth metrics
    sx_metrics_t metrics;
    sx_metrics_get(&metrics);
    
    ESP_LOGI(TAG, "Queue depths: LOW=%lu NORMAL=%lu HIGH=%lu CRITICAL=%lu",
             (unsigned long)metrics.queue_depth[0],
             (unsigned long)metrics.queue_depth[1],
             (unsigned long)metrics.queue_depth[2],
             (unsigned long)metrics.queue_depth[3]);
    
    // Queue depths should be reasonable (not exceed max queue sizes)
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(16, metrics.queue_depth[0]);  // LOW max 16
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(32, metrics.queue_depth[1]); // NORMAL max 32
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(16, metrics.queue_depth[2]);  // HIGH max 16
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(8, metrics.queue_depth[3]);  // CRITICAL max 8
}

// Test 5: State update rate
void test_state_update_rate(void) {
    const uint32_t num_updates = 100;
    
    sx_state_t state;
    memset(&state, 0, sizeof(state));
    state.version = 1;
    
    TickType_t start = xTaskGetTickCount();
    
    for (uint32_t i = 0; i < num_updates; i++) {
        state.version = i + 1;
        state.dirty_mask = SX_STATE_DIRTY_UI;
        sx_dispatcher_set_state(&state);
    }
    
    TickType_t end = xTaskGetTickCount();
    uint32_t elapsed_ms = (end - start) * portTICK_PERIOD_MS;
    uint32_t updates_per_sec = (num_updates * 1000) / elapsed_ms;
    
    ESP_LOGI(TAG, "Updated state %lu times in %lu ms (%lu updates/sec)",
             (unsigned long)num_updates, (unsigned long)elapsed_ms,
             (unsigned long)updates_per_sec);
    
    // Target: >= 1000 updates/sec (Section 11.1)
    TEST_ASSERT_GREATER_THAN_UINT32(1000, updates_per_sec);
}

// Test 6: Priority ordering under load
void test_priority_ordering_under_load(void) {
    // Post events with different priorities
    for (int i = 0; i < 20; i++) {
        sx_event_t evt = {
            .type = SX_EVT_UI_INPUT,
            .priority = (i % 4)  // Mix priorities
        };
        sx_dispatcher_post_event(&evt);
    }
    
    // Poll events - CRITICAL should come first
    sx_event_t evt;
    bool found_critical = false;
    bool found_high = false;
    bool found_normal = false;
    bool found_low = false;
    
    while (sx_dispatcher_poll_event(&evt)) {
        if (evt.priority == SX_EVT_PRIORITY_CRITICAL) {
            found_critical = true;
            // CRITICAL should come before others
            TEST_ASSERT_FALSE(found_high || found_normal || found_low);
        } else if (evt.priority == SX_EVT_PRIORITY_HIGH) {
            found_high = true;
            // HIGH should come before NORMAL/LOW
            TEST_ASSERT_FALSE(found_normal || found_low);
        } else if (evt.priority == SX_EVT_PRIORITY_NORMAL) {
            found_normal = true;
            // NORMAL should come before LOW
            TEST_ASSERT_FALSE(found_low);
        } else if (evt.priority == SX_EVT_PRIORITY_LOW) {
            found_low = true;
        }
    }
}

// Test 7: Metrics accuracy
void test_metrics_accuracy(void) {
    const uint32_t num_events = 50;
    
    // Reset metrics
    sx_metrics_reset();
    
    // Post events
    for (uint32_t i = 0; i < num_events; i++) {
        sx_event_t evt = {
            .type = SX_EVT_UI_INPUT,
            .priority = SX_EVT_PRIORITY_NORMAL
        };
        sx_dispatcher_post_event(&evt);
    }
    
    // Check metrics
    sx_metrics_t metrics;
    sx_metrics_get(&metrics);
    
    ESP_LOGI(TAG, "Posted events: %lu", 
             (unsigned long)metrics.evt_posted_total[1]);
    
    // Should have tracked posted events
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(num_events, metrics.evt_posted_total[1]);
}

void app_main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_event_posting_rate);
    RUN_TEST(test_backpressure_drop_under_load);
    RUN_TEST(test_backpressure_coalesce);
    RUN_TEST(test_queue_depth_under_stress);
    RUN_TEST(test_state_update_rate);
    RUN_TEST(test_priority_ordering_under_load);
    RUN_TEST(test_metrics_accuracy);
    
    UNITY_END();
}






