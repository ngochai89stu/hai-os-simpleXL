#include "sx_dispatcher.h"
#include "sx_event_string_pool.h"
#include "sx_metrics.h"  // P2.6: Metrics collection

#include <string.h>
#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

static const char *TAG = "sx_dispatcher";

// Priority queues: 4 queues (one per priority level)
static QueueHandle_t s_evt_q_low;
static QueueHandle_t s_evt_q_normal;
static QueueHandle_t s_evt_q_high;
static QueueHandle_t s_evt_q_critical;

// OPT-5: Double-buffer pattern (Section 5.3 SIMPLEXL_ARCH v1.3)
// Writer ghi vào back buffer, publish atomically.
// Readers luôn đọc front buffer stable (không cần mutex cho read).
static sx_state_t s_state_front;  // Front buffer (read-only for readers)
static sx_state_t s_state_back;   // Back buffer (write-only for orchestrator)
static volatile sx_state_t *s_state_read_ptr = &s_state_front;  // Atomic read pointer
static StaticSemaphore_t s_state_write_mutex_buf;
static SemaphoreHandle_t s_state_write_mutex;  // Only for write operations

// P1.3: Backpressure metrics (Section 4.3 SIMPLEXL_ARCH v1.3)
static uint32_t s_drop_count[4] = {0};  // Per priority: LOW, NORMAL, HIGH, CRITICAL
static uint32_t s_coalesce_count[4] = {0};  // Per priority
static TickType_t s_last_drop_log_time = 0;
static const TickType_t s_drop_log_interval = pdMS_TO_TICKS(5000); // Log every 5 seconds max

// P1.3: Coalesce tracking - keep latest event per coalesce_key
// Simple implementation: track by event type (can be extended to use coalesce_key)
#define MAX_COALESCE_ENTRIES 32
typedef struct {
    sx_event_type_t event_type;
    uint32_t coalesce_key;
    sx_event_t event;
    bool valid;
} coalesce_entry_t;
static coalesce_entry_t s_coalesce_table[MAX_COALESCE_ENTRIES];
static uint32_t s_coalesce_table_size = 0;

bool sx_dispatcher_init(void) {
    // P2.6: Initialize metrics system
    sx_metrics_init();
    
    // Initialize event string pool for memory optimization
    sx_event_string_pool_init();
    
    // Create priority queues
    if (s_evt_q_low == NULL) {
        s_evt_q_low = xQueueCreate(16, sizeof(sx_event_t));
    }
    if (s_evt_q_normal == NULL) {
        s_evt_q_normal = xQueueCreate(32, sizeof(sx_event_t));
    }
    if (s_evt_q_high == NULL) {
        s_evt_q_high = xQueueCreate(16, sizeof(sx_event_t));
    }
    if (s_evt_q_critical == NULL) {
        s_evt_q_critical = xQueueCreate(8, sizeof(sx_event_t));
    }
    
    // OPT-5: Initialize write mutex for double-buffer pattern
    if (s_state_write_mutex == NULL) {
        s_state_write_mutex = xSemaphoreCreateMutexStatic(&s_state_write_mutex_buf);
    }
    
    // OPT-5: Initialize both buffers
    if (s_state_front.seq == 0) {
        memset(&s_state_front, 0, sizeof(s_state_front));
        memset(&s_state_back, 0, sizeof(s_state_back));
        
        // Initialize front buffer
        s_state_front.version = 1;  // P0.4: Initialize version (Section 5.1)
        s_state_front.dirty_mask = 0;  // P0.4: Initialize dirty_mask (Section 5.1)
        s_state_front.seq = 1;
        s_state_front.ui.device_state = SX_DEV_BOOTING;
        s_state_front.ui.status_text = "booting";
        s_state_front.ui.emotion_id = "neutral";
        
        // Initialize back buffer (same initial state)
        s_state_back = s_state_front;
        
        // Set read pointer to front buffer
        s_state_read_ptr = &s_state_front;
    }
    
    return (s_evt_q_low != NULL) && (s_evt_q_normal != NULL) && 
           (s_evt_q_high != NULL) && (s_evt_q_critical != NULL) && 
           (s_state_write_mutex != NULL);
}

// P1.3: Helper to find coalesce entry
static coalesce_entry_t* find_coalesce_entry(sx_event_type_t event_type, uint32_t coalesce_key) {
    for (uint32_t i = 0; i < s_coalesce_table_size && i < MAX_COALESCE_ENTRIES; i++) {
        if (s_coalesce_table[i].valid &&
            s_coalesce_table[i].event_type == event_type &&
            s_coalesce_table[i].coalesce_key == coalesce_key) {
            return &s_coalesce_table[i];
        }
    }
    return NULL;
}

// P1.3: Helper to add/update coalesce entry
static void update_coalesce_entry(sx_event_type_t event_type, uint32_t coalesce_key, const sx_event_t *evt) {
    coalesce_entry_t *entry = find_coalesce_entry(event_type, coalesce_key);
    if (entry) {
        // Update existing entry
        entry->event = *evt;
    } else {
        // Add new entry
        if (s_coalesce_table_size < MAX_COALESCE_ENTRIES) {
            entry = &s_coalesce_table[s_coalesce_table_size++];
            entry->event_type = event_type;
            entry->coalesce_key = coalesce_key;
            entry->event = *evt;
            entry->valid = true;
        } else {
            // Table full - evict oldest (simple: use first entry)
            entry = &s_coalesce_table[0];
            entry->event_type = event_type;
            entry->coalesce_key = coalesce_key;
            entry->event = *evt;
            entry->valid = true;
        }
    }
}

// P1.3: Helper to flush coalesced events to queue
static void flush_coalesced_events(QueueHandle_t target_q, sx_event_priority_t priority) {
    for (uint32_t i = 0; i < s_coalesce_table_size; i++) {
        if (s_coalesce_table[i].valid) {
            TickType_t timeout = (priority == SX_EVT_PRIORITY_CRITICAL) ? pdMS_TO_TICKS(10) : 0;
            if (xQueueSend(target_q, &s_coalesce_table[i].event, timeout) == pdTRUE) {
                s_coalesce_table[i].valid = false;
            }
        }
    }
    // Compact table (remove invalid entries)
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < s_coalesce_table_size; i++) {
        if (s_coalesce_table[i].valid) {
            if (write_idx != i) {
                s_coalesce_table[write_idx] = s_coalesce_table[i];
            }
            write_idx++;
        }
    }
    s_coalesce_table_size = write_idx;
}

bool sx_dispatcher_post_event(const sx_event_t *evt) {
    // P1.3: Default policy is DROP (backward compatible)
    return sx_dispatcher_post_event_with_policy(evt, SX_BP_DROP, 0);
}

bool sx_dispatcher_post_event_with_policy(
    const sx_event_t *evt,
    sx_backpressure_policy_t policy,
    uint32_t coalesce_key
) {
    if (!evt) {
        return false;
    }
    
    // Determine priority (use default if not set)
    sx_event_priority_t priority = evt->priority;
    if (priority == 0) {  // Not set, use default
        priority = SX_EVT_DEFAULT_PRIORITY(evt->type);
    }
    
    // Select queue based on priority
    QueueHandle_t target_q = s_evt_q_normal;
    TickType_t timeout = 0;
    
    switch (priority) {
        case SX_EVT_PRIORITY_CRITICAL:
            target_q = s_evt_q_critical;
            // P1.3: BLOCK policy for CRITICAL (with timeout)
            timeout = (policy == SX_BP_BLOCK) ? pdMS_TO_TICKS(50) : pdMS_TO_TICKS(10);
            break;
        case SX_EVT_PRIORITY_HIGH:
            target_q = s_evt_q_high;
            timeout = (policy == SX_BP_BLOCK) ? pdMS_TO_TICKS(20) : pdMS_TO_TICKS(5);
            break;
        case SX_EVT_PRIORITY_NORMAL:
            target_q = s_evt_q_normal;
            timeout = 0;  // Non-blocking for NORMAL
            break;
        case SX_EVT_PRIORITY_LOW:
            target_q = s_evt_q_low;
            timeout = 0;  // Non-blocking for LOW
            break;
        default:
            target_q = s_evt_q_normal;
            timeout = 0;
            break;
    }
    
    if (target_q == NULL) {
        return false;
    }
    
    // P2.6: Update metrics - event posted
    sx_metrics_inc_evt_posted(priority);
    
    // P1.3: Try to send event
    if (xQueueSend(target_q, evt, timeout) == pdTRUE) {
        // P2.6: Update queue depth
        uint32_t queue_size = uxQueueMessagesWaiting(target_q);
        sx_metrics_set_queue_depth(priority, queue_size);
        return true;
    }
    
    // Queue full - apply backpressure policy
    switch (policy) {
        case SX_BP_DROP: {
            // Drop event and increment counter
            s_drop_count[priority]++;
            
            // P2.6: Update metrics - event dropped
            sx_metrics_inc_evt_dropped(priority);
            
            // Rate-limited logging (max once per 5 seconds)
            TickType_t now = xTaskGetTickCount();
            if (now - s_last_drop_log_time >= s_drop_log_interval) {
                ESP_LOGW(TAG, "Event dropped (type=%d, prio=%d, total_dropped=%lu)", 
                         (int)evt->type, (int)priority, (unsigned long)s_drop_count[priority]);
                s_last_drop_log_time = now;
            }
            return false;
        }
        
        case SX_BP_COALESCE: {
            // P1.3: Keep only latest event (replace older one if exists)
            uint32_t key = (coalesce_key != 0) ? coalesce_key : (uint32_t)evt->type;
            coalesce_entry_t *existing = find_coalesce_entry(evt->type, key);
            
            if (existing) {
                // Replace existing event
                existing->event = *evt;
                s_coalesce_count[priority]++;
                // P2.6: Update metrics - event coalesced
                sx_metrics_inc_evt_coalesced(priority);
                ESP_LOGD(TAG, "Event coalesced (type=%d, key=%lu, prio=%d)", 
                        (int)evt->type, (unsigned long)key, (int)priority);
            } else {
                // Add to coalesce table
                update_coalesce_entry(evt->type, key, evt);
                s_coalesce_count[priority]++;
                // P2.6: Update metrics - event coalesced
                sx_metrics_inc_evt_coalesced(priority);
                ESP_LOGD(TAG, "Event queued for coalesce (type=%d, key=%lu, prio=%d)", 
                        (int)evt->type, (unsigned long)key, (int)priority);
            }
            
            // Try to flush coalesced events periodically
            // (In production, this could be done in a separate task or on queue space available)
            if (uxQueueSpacesAvailable(target_q) > 0) {
                flush_coalesced_events(target_q, priority);
            }
            
            return true;  // Event "accepted" (coalesced)
        }
        
        case SX_BP_BLOCK: {
            // P1.3: Block with longer timeout (only for CRITICAL/HIGH)
            if (priority == SX_EVT_PRIORITY_CRITICAL || priority == SX_EVT_PRIORITY_HIGH) {
                TickType_t block_timeout = (priority == SX_EVT_PRIORITY_CRITICAL) ? 
                                          pdMS_TO_TICKS(100) : pdMS_TO_TICKS(50);
                if (xQueueSend(target_q, evt, block_timeout) == pdTRUE) {
                    return true;
                }
                // Still failed after blocking - drop
                s_drop_count[priority]++;
                ESP_LOGE(TAG, "CRITICAL/HIGH event dropped after blocking (type=%d, prio=%d)", 
                        (int)evt->type, (int)priority);
                return false;
            } else {
                // BLOCK policy not allowed for NORMAL/LOW - fallback to DROP
                ESP_LOGW(TAG, "BLOCK policy not allowed for priority %d, using DROP", (int)priority);
                s_drop_count[priority]++;
                return false;
            }
        }
        
        default:
            // Unknown policy - drop
            s_drop_count[priority]++;
            return false;
    }
}

bool sx_dispatcher_poll_event(sx_event_t *out_evt) {
    if (!out_evt) {
        return false;
    }
    
    // Poll in priority order: critical → high → normal → low
    if (s_evt_q_critical != NULL && xQueueReceive(s_evt_q_critical, out_evt, 0) == pdTRUE) {
        return true;
    }
    if (s_evt_q_high != NULL && xQueueReceive(s_evt_q_high, out_evt, 0) == pdTRUE) {
        return true;
    }
    if (s_evt_q_normal != NULL && xQueueReceive(s_evt_q_normal, out_evt, 0) == pdTRUE) {
        return true;
    }
    if (s_evt_q_low != NULL && xQueueReceive(s_evt_q_low, out_evt, 0) == pdTRUE) {
        return true;
    }
    return false;
}

void sx_dispatcher_set_state(const sx_state_t *state) {
    if (!state || s_state_write_mutex == NULL) {
        return;
    }
    
    // OPT-5: Writer ghi vào back buffer (Section 5.3)
    xSemaphoreTake(s_state_write_mutex, portMAX_DELAY);
    
    // Determine which buffer is currently front (for readers)
    bool front_is_front = (s_state_read_ptr == &s_state_front);
    
    // Write to back buffer (opposite of current front)
    if (front_is_front) {
        s_state_back = *state;  // Write to back buffer
    } else {
        s_state_front = *state;  // Write to front buffer (which becomes new back)
    }
    
    // OPT-5: Atomic publish - swap pointers
    // This is atomic on most architectures (pointer assignment)
    if (front_is_front) {
        s_state_read_ptr = &s_state_back;  // Swap: back becomes front
    } else {
        s_state_read_ptr = &s_state_front;  // Swap: front becomes front again
    }
    
    xSemaphoreGive(s_state_write_mutex);
    
    // Note: Readers can now read from s_state_read_ptr without mutex
    // The buffer that was just written becomes the new stable front buffer
    // Next write will go to the other buffer (which is now the back buffer)
}

void sx_dispatcher_get_state(sx_state_t *out_state) {
    if (!out_state) {
        return;
    }
    
    // OPT-5: Readers đọc từ front buffer stable (không cần mutex)
    // s_state_read_ptr points to stable front buffer
    volatile sx_state_t *read_ptr = s_state_read_ptr;
    *out_state = *read_ptr;  // Copy from stable buffer
    
    // Note: This is lock-free read. Writer only updates pointer atomically.
}

// P1.3: Backpressure metrics (Section 4.3 SIMPLEXL_ARCH v1.3)
void sx_dispatcher_get_drop_count(uint32_t drop_count[4]) {
    if (!drop_count) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        drop_count[i] = s_drop_count[i];
    }
}

void sx_dispatcher_get_coalesce_count(uint32_t coalesce_count[4]) {
    if (!coalesce_count) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        coalesce_count[i] = s_coalesce_count[i];
    }
}
