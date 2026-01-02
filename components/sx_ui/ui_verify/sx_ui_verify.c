#include "sx_ui_verify.h"

#if SX_UI_VERIFY_MODE

#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_router.h"
#include "ui_screen_registry.h"
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)

static const char *TAG = "sx_ui_verify";

#define MAX_SCREENS SCREEN_ID_MAX
static sx_ui_verify_screen_evidence_t s_evidence[MAX_SCREENS];
static bool s_initialized = false;
static bool s_walking = false;
static TaskHandle_t s_walk_task_handle = NULL;

// Screen names for reporting
static const char* s_screen_names[MAX_SCREENS] = {
    "Boot", "Flash", "Home", "Chat", "Wakeword Feedback",
    "Music Online", "Music Player", "Radio", "SD Card Music", "IR Control",
    "Settings", "WiFi Setup", "Bluetooth", "Display", "Equalizer",
    "OTA Update", "About", "Google Navigation", "Permission", "Screensaver",
    "Screensaver Setting", "Audio Effects", "Startup Image", "Voice Settings",
    "Network Diagnostic", "Snapshot Manager", "Diagnostics", "Introspection",
    "Dev Console", "Touch Debug"
};

static uint64_t get_timestamp_ms(void) {
    return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void sx_ui_verify_init(void) {
    if (s_initialized) {
        ESP_LOGW(TAG, "Verify system already initialized");
        return;
    }
    
    memset(s_evidence, 0, sizeof(s_evidence));
    for (int i = 0; i < MAX_SCREENS; i++) {
        s_evidence[i].screen_id = (ui_screen_id_t)i;
        s_evidence[i].screen_name = s_screen_names[i];
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "UI verification system initialized (max screens: %d)", MAX_SCREENS);
}

void sx_ui_verify_on_create(ui_screen_id_t screen_id, const char *screen_name, 
                            void *container, void *root) {
    if (!s_initialized || screen_id >= MAX_SCREENS) {
        return;
    }
    
    sx_ui_verify_screen_evidence_t *ev = &s_evidence[screen_id];
    ev->create_count++;
    ev->container_ptr = container;
    ev->root_ptr = root;
    ev->last_create_ts = get_timestamp_ms();
    
    if (screen_name) {
        ev->screen_name = screen_name;
    }
    
    ESP_LOGI(TAG, "[CREATE] id=%d name=%s container=%p root=%p count=%d",
             screen_id, ev->screen_name, container, root, ev->create_count);
    
    // Validate
    if (root == NULL) {
        ESP_LOGW(TAG, "[CREATE] WARNING: Screen %d (%s) created with NULL root!", 
                 screen_id, ev->screen_name);
    }
    if (container == NULL) {
        ESP_LOGE(TAG, "[CREATE] ERROR: Screen %d (%s) created with NULL container!", 
                 screen_id, ev->screen_name);
    }
}

void sx_ui_verify_on_show(ui_screen_id_t screen_id) {
    if (!s_initialized || screen_id >= MAX_SCREENS) {
        return;
    }
    
    sx_ui_verify_screen_evidence_t *ev = &s_evidence[screen_id];
    ev->show_count++;
    ev->last_show_ts = get_timestamp_ms();
    
    ESP_LOGI(TAG, "[SHOW] id=%d name=%s count=%d", 
             screen_id, ev->screen_name, ev->show_count);
}

void sx_ui_verify_on_hide(ui_screen_id_t screen_id) {
    if (!s_initialized || screen_id >= MAX_SCREENS) {
        return;
    }
    
    sx_ui_verify_screen_evidence_t *ev = &s_evidence[screen_id];
    ev->hide_count++;
    
    ESP_LOGI(TAG, "[HIDE] id=%d name=%s count=%d", 
             screen_id, ev->screen_name, ev->hide_count);
}

void sx_ui_verify_on_destroy(ui_screen_id_t screen_id) {
    if (!s_initialized || screen_id >= MAX_SCREENS) {
        return;
    }
    
    sx_ui_verify_screen_evidence_t *ev = &s_evidence[screen_id];
    ev->destroy_count++;
    
    ESP_LOGI(TAG, "[DESTROY] id=%d name=%s count=%d", 
             screen_id, ev->screen_name, ev->destroy_count);
}

void sx_ui_verify_mark_touch_ok(ui_screen_id_t screen_id) {
    if (!s_initialized || screen_id >= MAX_SCREENS) {
        return;
    }
    
    sx_ui_verify_screen_evidence_t *ev = &s_evidence[screen_id];
    ev->touch_ok = true;
    ev->touch_count++;
    
    ESP_LOGI(TAG, "[TOUCH] screen=%d name=%s clicked count=%d", 
             screen_id, ev->screen_name, ev->touch_count);
}

const sx_ui_verify_screen_evidence_t* sx_ui_verify_get_evidence(ui_screen_id_t screen_id) {
    if (!s_initialized || screen_id >= MAX_SCREENS) {
        return NULL;
    }
    return &s_evidence[screen_id];
}

static void screen_walk_task(void *arg) {
    ESP_LOGI(TAG, "Screen walk task started");
    s_walking = true;
    
    // Wait a bit for HOME to be fully visible
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Walk through all screens (skip BOOT and FLASH to avoid breaking boot flow)
    for (ui_screen_id_t id = SCREEN_ID_BOOT; id < SCREEN_ID_MAX; id++) {
        // Skip BOOT and FLASH in walk mode (they're part of boot sequence)
        if (id == SCREEN_ID_BOOT || id == SCREEN_ID_FLASH) {
            ESP_LOGI(TAG, "[WALK] Skipping screen %d (%s) - part of boot sequence", 
                     id, s_screen_names[id]);
            continue;
        }
        
        ESP_LOGI(TAG, "[WALK] Navigating to screen %d (%s)", id, s_screen_names[id]);
        
        // Navigate to screen
        if (lvgl_port_lock(0)) {
            ui_router_navigate_to(id);
            lvgl_port_unlock();
        }
        
        // Wait for onCreate/onShow to complete
        vTaskDelay(pdMS_TO_TICKS(800));
        
        // Check evidence
        const sx_ui_verify_screen_evidence_t *ev = sx_ui_verify_get_evidence(id);
        if (ev && ev->create_count > 0) {
            ESP_LOGI(TAG, "[WALK] ✓ Screen %d created (root=%p, create_count=%d)", 
                     id, ev->root_ptr, ev->create_count);
        } else {
            ESP_LOGW(TAG, "[WALK] ✗ Screen %d NOT created (no evidence)", id);
        }
    }
    
    // Navigate back to HOME
    ESP_LOGI(TAG, "[WALK] Returning to HOME");
    if (lvgl_port_lock(0)) {
        ui_router_navigate_to(SCREEN_ID_HOME);
        lvgl_port_unlock();
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Dump summary
    sx_ui_verify_dump_report();
    
    s_walking = false;
    s_walk_task_handle = NULL;
    ESP_LOGI(TAG, "Screen walk task completed");
    vTaskDelete(NULL);
}

void sx_ui_verify_start_screen_walk(void) {
    if (s_walking) {
        ESP_LOGW(TAG, "Screen walk already in progress");
        return;
    }
    
    if (s_walk_task_handle != NULL) {
        ESP_LOGW(TAG, "Screen walk task already exists");
        return;
    }
    
    ESP_LOGI(TAG, "Starting screen walk mode");
    xTaskCreate(screen_walk_task, "screen_walk", 4096, NULL, 5, &s_walk_task_handle);
}

bool sx_ui_verify_is_walking(void) {
    return s_walking;
}

void sx_ui_verify_dump_report(void) {
    if (!s_initialized) {
        ESP_LOGE(TAG, "Verify system not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "UI VERIFICATION SUMMARY REPORT");
    ESP_LOGI(TAG, "========================================");
    
    int created_count = 0;
    int touch_ok_count = 0;
    
    for (int i = 0; i < MAX_SCREENS; i++) {
        const sx_ui_verify_screen_evidence_t *ev = &s_evidence[i];
        
        const char *status = "FAIL";
        if (ev->create_count > 0 && ev->root_ptr != NULL) {
            status = "PASS";
            created_count++;
        }
        
        const char *touch_status = ev->touch_ok ? "TOUCH_OK" : "NO_TOUCH";
        if (ev->touch_ok) {
            touch_ok_count++;
        }
        
        ESP_LOGI(TAG, "Screen %2d: %-25s | %s | create=%d show=%d hide=%d destroy=%d | root=%p | %s",
                 i, ev->screen_name, status, 
                 ev->create_count, ev->show_count, ev->hide_count, ev->destroy_count,
                 ev->root_ptr, touch_status);
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Summary: %d/%d screens created, %d screens with touch events",
             created_count, MAX_SCREENS, touch_ok_count);
    ESP_LOGI(TAG, "========================================");
}

const char* sx_ui_verify_get_report_string(void) {
    // Simple string representation (could be enhanced)
    static char report[512];
    snprintf(report, sizeof(report), "Verification report: see logs for details");
    return report;
}

#endif // SX_UI_VERIFY_MODE

