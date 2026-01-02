#include <unity.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "sx_event_handler.h"
#include "sx_event.h"
#include "sx_state.h"

// Test handler function
static bool test_handler_called = false;
static sx_event_type_t test_handler_event_type = SX_EVT_NONE;

bool test_event_handler(const sx_event_t *evt, sx_state_t *state) {
    test_handler_called = true;
    test_handler_event_type = evt->type;
    if (state) {
        state->seq++;
    }
    return true;
}

// Test setup and teardown
void setUp(void) {
    test_handler_called = false;
    test_handler_event_type = SX_EVT_NONE;
    sx_event_handler_init();
}

void tearDown(void) {
    // Cleanup
    sx_event_handler_unregister(SX_EVT_UI_INPUT);
}

// Test event handler initialization
void test_event_handler_init(void) {
    esp_err_t ret = sx_event_handler_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

// Test event handler registration
void test_event_handler_register(void) {
    esp_err_t ret = sx_event_handler_register(SX_EVT_UI_INPUT, test_event_handler);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

// Test event handler unregistration
void test_event_handler_unregister(void) {
    // Register first
    sx_event_handler_register(SX_EVT_UI_INPUT, test_event_handler);
    
    // Unregister
    esp_err_t ret = sx_event_handler_unregister(SX_EVT_UI_INPUT);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

// Test event handler processing
void test_event_handler_process(void) {
    // Register handler
    sx_event_handler_register(SX_EVT_UI_INPUT, test_event_handler);
    
    // Create event
    sx_event_t evt = {
        .type = SX_EVT_UI_INPUT,
        .priority = SX_EVT_PRIORITY_NORMAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    
    // Create state
    sx_state_t state = {0};
    state.seq = 100;
    
    // Process event
    bool handled = sx_event_handler_process(&evt, &state);
    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_TRUE(test_handler_called);
    TEST_ASSERT_EQUAL(SX_EVT_UI_INPUT, test_handler_event_type);
    TEST_ASSERT_EQUAL(101, state.seq);
}

// Test unregistered event type
void test_event_handler_unregistered(void) {
    // Don't register handler
    
    // Create event
    sx_event_t evt = {
        .type = SX_EVT_UI_INPUT,
        .priority = SX_EVT_PRIORITY_NORMAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    
    // Create state
    sx_state_t state = {0};
    
    // Process event (should return false)
    bool handled = sx_event_handler_process(&evt, &state);
    TEST_ASSERT_FALSE(handled);
    TEST_ASSERT_FALSE(test_handler_called);
}

// Test invalid event handler registration
void test_event_handler_invalid_register(void) {
    // Register with NULL handler should fail
    esp_err_t ret = sx_event_handler_register(SX_EVT_UI_INPUT, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

// Test invalid event processing
void test_event_handler_invalid_process(void) {
    // Process with NULL event should fail
    sx_state_t state = {0};
    bool handled = sx_event_handler_process(NULL, &state);
    TEST_ASSERT_FALSE(handled);
    
    // Process with NULL state should fail
    sx_event_t evt = {
        .type = SX_EVT_UI_INPUT,
        .priority = SX_EVT_PRIORITY_NORMAL,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = NULL
    };
    handled = sx_event_handler_process(&evt, NULL);
    TEST_ASSERT_FALSE(handled);
}









