#include <unity.h>
#include <string.h>
#include <stdlib.h>
#include "sx_event_string_pool.h"

// Test setup and teardown
void setUp(void) {
    sx_event_string_pool_init();
    sx_event_string_pool_reset_metrics();
}

void tearDown(void) {
    // Cleanup
}

// Test metrics initialization
void test_string_pool_metrics_init(void) {
    sx_event_string_pool_metrics_t metrics;
    sx_event_string_pool_get_metrics(&metrics);
    
    TEST_ASSERT_EQUAL(0, metrics.total_allocations);
    TEST_ASSERT_EQUAL(0, metrics.pool_hits);
    TEST_ASSERT_EQUAL(0, metrics.pool_misses);
    TEST_ASSERT_EQUAL(0, metrics.malloc_fallbacks);
    TEST_ASSERT_EQUAL(0, metrics.current_usage);
    TEST_ASSERT_EQUAL(0, metrics.peak_usage);
}

// Test pool hits tracking
void test_string_pool_metrics_hits(void) {
    char *str1 = sx_event_alloc_string("test1");
    char *str2 = sx_event_alloc_string("test2");
    
    TEST_ASSERT_NOT_NULL(str1);
    TEST_ASSERT_NOT_NULL(str2);
    
    sx_event_string_pool_metrics_t metrics;
    sx_event_string_pool_get_metrics(&metrics);
    
    TEST_ASSERT_EQUAL(2, metrics.total_allocations);
    TEST_ASSERT_EQUAL(2, metrics.pool_hits);
    TEST_ASSERT_EQUAL(0, metrics.pool_misses);
    TEST_ASSERT_EQUAL(0, metrics.malloc_fallbacks);
    TEST_ASSERT_EQUAL(2, metrics.current_usage);
    TEST_ASSERT_EQUAL(2, metrics.peak_usage);
    
    // Free strings
    sx_event_free_string(str1);
    sx_event_free_string(str2);
    
    sx_event_string_pool_get_metrics(&metrics);
    TEST_ASSERT_EQUAL(0, metrics.current_usage);
    TEST_ASSERT_EQUAL(2, metrics.peak_usage); // Peak should remain
}

// Test pool misses tracking
void test_string_pool_metrics_misses(void) {
    // Fill up the pool (16 slots)
    char *strings[20];
    int allocated = 0;
    
    for (int i = 0; i < 20; i++) {
        char test_str[32];
        snprintf(test_str, sizeof(test_str), "test%d", i);
        strings[allocated] = sx_event_alloc_string(test_str);
        if (strings[allocated] != NULL) {
            allocated++;
        }
    }
    
    sx_event_string_pool_metrics_t metrics;
    sx_event_string_pool_get_metrics(&metrics);
    
    // Should have at least 16 pool hits (pool size)
    TEST_ASSERT_GREATER_OR_EQUAL(16, metrics.pool_hits);
    // Should have some misses if pool is full
    if (allocated > 16) {
        TEST_ASSERT_GREATER_THAN(0, metrics.pool_misses);
    }
    
    // Free all
    for (int i = 0; i < allocated; i++) {
        sx_event_free_string(strings[i]);
    }
}

// Test malloc fallbacks tracking
void test_string_pool_metrics_fallbacks(void) {
    // Allocate string longer than pool max length
    char long_str[256];
    memset(long_str, 'A', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';
    
    char *str = sx_event_alloc_string(long_str);
    TEST_ASSERT_NOT_NULL(str);
    
    sx_event_string_pool_metrics_t metrics;
    sx_event_string_pool_get_metrics(&metrics);
    
    TEST_ASSERT_EQUAL(1, metrics.total_allocations);
    TEST_ASSERT_EQUAL(1, metrics.pool_misses);
    TEST_ASSERT_EQUAL(1, metrics.malloc_fallbacks);
    
    sx_event_free_string(str);
}

// Test peak usage tracking
void test_string_pool_metrics_peak(void) {
    char *strings[10];
    
    // Allocate 5 strings
    for (int i = 0; i < 5; i++) {
        char test_str[32];
        snprintf(test_str, sizeof(test_str), "test%d", i);
        strings[i] = sx_event_alloc_string(test_str);
    }
    
    sx_event_string_pool_metrics_t metrics;
    sx_event_string_pool_get_metrics(&metrics);
    TEST_ASSERT_EQUAL(5, metrics.peak_usage);
    
    // Free 2 strings
    sx_event_free_string(strings[0]);
    sx_event_free_string(strings[1]);
    
    sx_event_string_pool_get_metrics(&metrics);
    TEST_ASSERT_EQUAL(3, metrics.current_usage);
    TEST_ASSERT_EQUAL(5, metrics.peak_usage); // Peak should remain
    
    // Free remaining
    for (int i = 2; i < 5; i++) {
        sx_event_free_string(strings[i]);
    }
}

// Test metrics reset
void test_string_pool_metrics_reset(void) {
    // Allocate some strings
    char *str1 = sx_event_alloc_string("test1");
    char *str2 = sx_event_alloc_string("test2");
    
    sx_event_string_pool_reset_metrics();
    
    sx_event_string_pool_metrics_t metrics;
    sx_event_string_pool_get_metrics(&metrics);
    
    // Metrics should be reset (but current_usage should still reflect actual usage)
    TEST_ASSERT_EQUAL(0, metrics.total_allocations);
    TEST_ASSERT_EQUAL(0, metrics.pool_hits);
    TEST_ASSERT_EQUAL(0, metrics.pool_misses);
    TEST_ASSERT_EQUAL(0, metrics.malloc_fallbacks);
    TEST_ASSERT_EQUAL(0, metrics.peak_usage);
    // current_usage should reflect actual pool usage
    TEST_ASSERT_EQUAL(2, metrics.current_usage);
    
    sx_event_free_string(str1);
    sx_event_free_string(str2);
}









