#pragma once

// Test function declarations for dispatcher tests
void test_dispatcher_init(void);
void test_event_post_and_poll(void);
void test_state_get_set(void);
void test_event_queue_full(void);
void test_state_thread_safety(void);
void test_invalid_event_post(void);
void test_invalid_event_poll(void);
void test_invalid_state_ops(void);

