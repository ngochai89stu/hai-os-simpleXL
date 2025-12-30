#include <unity.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "sx_state.h"
#include "sx_dispatcher.h"

// Test setup
void setUp(void) {
    sx_dispatcher_init();
}

void tearDown(void) {
    // Cleanup
}

// Test initial state values
void test_initial_state(void) {
    sx_state_t state;
    sx_dispatcher_get_state(&state);
    
    // Check initial values
    TEST_ASSERT_EQUAL(0, state.seq);
    TEST_ASSERT_EQUAL(SX_DEV_BOOTING, state.ui.device_state);
    TEST_ASSERT_NOT_NULL(state.ui.status_text);
    TEST_ASSERT_NOT_NULL(state.ui.emotion_id);
}

// Test state sequence increment
void test_state_sequence(void) {
    sx_state_t state1, state2;
    
    sx_dispatcher_get_state(&state1);
    uint32_t seq1 = state1.seq;
    
    // Modify and set
    state1.seq++;
    sx_dispatcher_set_state(&state1);
    
    // Get again
    sx_dispatcher_get_state(&state2);
    TEST_ASSERT_EQUAL(seq1 + 1, state2.seq);
}

// Test device state transitions
void test_device_state_transitions(void) {
    sx_state_t state;
    
    sx_dispatcher_get_state(&state);
    
    // Test all state transitions
    state.seq++;
    state.ui.device_state = SX_DEV_IDLE;
    sx_dispatcher_set_state(&state);
    
    sx_dispatcher_get_state(&state);
    TEST_ASSERT_EQUAL(SX_DEV_IDLE, state.ui.device_state);
    
    state.seq++;
    state.ui.device_state = SX_DEV_BUSY;
    sx_dispatcher_set_state(&state);
    
    sx_dispatcher_get_state(&state);
    TEST_ASSERT_EQUAL(SX_DEV_BUSY, state.ui.device_state);
    
    state.seq++;
    state.ui.device_state = SX_DEV_ERROR;
    sx_dispatcher_set_state(&state);
    
    sx_dispatcher_get_state(&state);
    TEST_ASSERT_EQUAL(SX_DEV_ERROR, state.ui.device_state);
}

// Test UI state fields
void test_ui_state_fields(void) {
    sx_state_t state;
    
    sx_dispatcher_get_state(&state);
    
    // Test status_text
    TEST_ASSERT_NOT_NULL(state.ui.status_text);
    
    // Test emotion_id
    TEST_ASSERT_NOT_NULL(state.ui.emotion_id);
    
    // Test message buffers
    TEST_ASSERT_EQUAL(SX_UI_MESSAGE_MAX_LEN, sizeof(state.ui.last_user_message));
    TEST_ASSERT_EQUAL(SX_UI_MESSAGE_MAX_LEN, sizeof(state.ui.last_assistant_message));
}

// Test message buffer operations
void test_message_buffers(void) {
    sx_state_t state;
    
    sx_dispatcher_get_state(&state);
    
    // Test that buffers are initialized (empty)
    TEST_ASSERT_EQUAL('\0', state.ui.last_user_message[0]);
    TEST_ASSERT_EQUAL('\0', state.ui.last_assistant_message[0]);
    
    // Test buffer size
    TEST_ASSERT_EQUAL(SX_UI_MESSAGE_MAX_LEN, sizeof(state.ui.last_user_message));
}

// Test state immutability pattern
void test_state_immutability(void) {
    sx_state_t state1, state2;
    
    sx_dispatcher_get_state(&state1);
    uint32_t seq1 = state1.seq;
    
    // Modify local copy
    state1.seq = 999;
    state1.ui.device_state = SX_DEV_ERROR;
    
    // Get state again - should be unchanged
    sx_dispatcher_get_state(&state2);
    TEST_ASSERT_EQUAL(seq1, state2.seq);
    TEST_ASSERT_NOT_EQUAL(SX_DEV_ERROR, state2.ui.device_state);
}

// Test wifi state structure
void test_wifi_state(void) {
    sx_state_t state;
    
    sx_dispatcher_get_state(&state);
    
    // Check wifi state fields exist
    // Initial values may be false/0
    TEST_ASSERT_FALSE(state.wifi.initialized);
    TEST_ASSERT_FALSE(state.wifi.connected);
}

// Test audio state structure
void test_audio_state(void) {
    sx_state_t state;
    
    sx_dispatcher_get_state(&state);
    
    // Check audio state fields exist
    TEST_ASSERT_FALSE(state.audio.initialized);
    TEST_ASSERT_FALSE(state.audio.playing);
    TEST_ASSERT_EQUAL(0, state.audio.volume);
}

