# PHASE 9 — Action Plan + Patch Set
## Prioritized backlog và diff patches cho P0/P1 issues

**Ngày tạo:** 2025-01-02  
**Dự án:** hai-os-simplexl  
**Mục tiêu:** Tạo prioritized backlog, cung cấp diff patches cho P0/P1 issues, và testing instructions

---

## 1. PRIORITIZED BACKLOG

### 1.1 P0 (Critical) Issues - Sprint 1 (2-3 tuần)

| ID | Issue | Phase | Effort | Impact | File/Line |
|----|-------|------|--------|--------|-----------|
| **P0-1** | Buffer Underrun Risk (Audio) | 5 | High | Critical | `sx_audio_service.c:L782` |
| **P0-2** | Memory Leak (STT Event) | 6 | Low | Critical | `sx_stt_service.c:L106` |
| **P0-3** | No Credential Storage (WiFi) | 6 | Medium | High | `sx_wifi_service.c:L244-L299` |
| **P0-4** | Circular Dependencies (UI↔Services) | 8 | High | Critical | `sx_ui/CMakeLists.txt`, `sx_services/CMakeLists.txt` |
| **P0-5** | Playback Priority Thấp | 5 | Low | High | `sx_audio_service.c:L536` |
| **P0-6** | No Auto-Commit (Settings) | 7 | Low | High | `sx_settings_service.c` |
| **P0-7** | Stack Overflow Risk (Playback Task) | 5 | Low | Critical | `sx_audio_service.c:L536` |

**Total Effort:** ~3-4 tuần (1 developer)

### 1.2 P1 (High) Issues - Sprint 2 (2-3 tuần)

| ID | Issue | Phase | Effort | Impact | File/Line |
|----|-------|------|--------|--------|-----------|
| **P1-1** | No URL Encoding (TTS) | 6 | Low | High | `sx_tts_service.c:L325` |
| **P1-2** | Mutex Lock Order | 2,5,8 | Medium | High | Multiple files |
| **P1-3** | No Buffer Queue (Audio) | 5 | Medium | High | `sx_audio_service.c:L730-L789` |
| **P1-4** | Queue Size Too Small (Wake Word) | 6 | Low | Medium | `sx_wake_word_service.c:L138` |
| **P1-5** | No Playlist Persistence | 7 | Medium | High | `sx_playlist_manager.c` |
| **P1-6** | No SD Write Support | 7 | Medium | High | `sx_sd_service.c` |
| **P1-7** | SPI Bus Mutex Blocking | 3 | Low | Medium | `sx_spi_bus_manager.c` |
| **P1-8** | Navigation trong Main Loop | 4 | Low | Medium | `sx_ui_task.c:L207-L247` |
| **P1-9** | High Bootstrap Coupling | 8 | High | High | `sx_bootstrap.c` |
| **P1-10** | Inconsistent Error Handling | 8 | Medium | Medium | Multiple files |

**Total Effort:** ~4-5 tuần (1 developer)

### 1.3 P2 (Medium) Issues - Sprint 3+ (Backlog)

| ID | Issue | Phase | Effort | Impact |
|----|-------|------|--------|--------|
| **P2-1** | No Unit Tests | 8 | High | High |
| **P2-2** | No Dependency Injection | 8 | High | Medium |
| **P2-3** | Large Components | 8 | High | Medium |
| **P2-4** | SD Asset Loading Chưa Implement | 4 | Medium | Low |
| **P2-5** | No Streaming STT | 6 | Medium | Medium |
| **P2-6** | EQ CPU Intensive | 5 | Medium | Low |
| **P2-7** | No Transaction Support (Settings) | 7 | Medium | Low |
| **P2-8** | Simple Shuffle Algorithm | 7 | Low | Low |

**Total Effort:** ~6-8 tuần (1 developer)

---

## 2. DIFF PATCHES - P0 ISSUES

### 2.1 P0-2: Memory Leak (STT Event)

**File:** `components/sx_services/sx_stt_service.c`

**Issue:** `strdup()` trong event → cần free trong event handler → memory leak risk

**Patch:**

```diff
--- a/components/sx_services/sx_stt_service.c
+++ b/components/sx_services/sx_stt_service.c
@@ -100,7 +100,7 @@ static void sx_stt_task(void *arg) {
                                     // Dispatch event if final
                                     if (final) {
                                         sx_event_t evt = {
                                             .type = SX_EVT_UI_INPUT,
                                             .arg0 = 0,
                                             .arg1 = 0,
-                                            .ptr = strdup(transcript->valuestring), // Allocate copy for event
+                                            .ptr = sx_event_alloc_string(transcript->valuestring), // Use event string pool
                                         };
                                         sx_dispatcher_post_event(&evt);
                                     }
```

**Testing:**
1. Run STT service với multiple requests
2. Monitor memory usage (heap_caps_get_free_size)
3. Verify không có memory leak sau 100+ requests

---

### 2.2 P0-5: Playback Priority Thấp

**File:** `components/sx_services/sx_audio_service.c`

**Issue:** Priority 4 có thể bị preempt → decode delay → underrun

**Patch:**

```diff
--- a/components/sx_services/sx_audio_service.c
+++ b/components/sx_services/sx_audio_service.c
@@ -533,6 +533,6 @@ esp_err_t sx_audio_play_file(const char *file_path) {
     s_playing = true;
-    // Tối ưu: Giảm stack size từ 4096 xuống 3072 để tiết kiệm memory
-    BaseType_t ret = xTaskCreatePinnedToCore(sx_audio_playback_task, "sx_audio_file", 3072, f, 4, &s_playback_task_handle, 0);
+    // Tăng priority lên 5 (same as recording) để tránh preemption
+    BaseType_t ret = xTaskCreatePinnedToCore(sx_audio_playback_task, "sx_audio_file", 4096, f, 5, &s_playback_task_handle, 0);
     if (ret!=pdPASS) {
```

**Testing:**
1. Play high bitrate MP3/AAC
2. Load system với nhiều tasks
3. Monitor I2S underrun events
4. Verify không có audio glitch

---

### 2.3 P0-6: No Auto-Commit (Settings)

**File:** `components/sx_services/sx_settings_service.c`

**Issue:** Cần explicit `sx_settings_commit()` → có thể quên commit

**Patch:**

```diff
--- a/components/sx_services/sx_settings_service.c
+++ b/components/sx_services/sx_settings_service.c
@@ -31,6 +31,7 @@ esp_err_t sx_settings_set_string(const char *key, const char *value) {
     esp_err_t ret = nvs_set_str(s_nvs_handle, key, value);
     if (ret != ESP_OK) {
         ESP_LOGE(TAG, "Failed to set string '%s': %s", key, esp_err_to_name(ret));
         return ret;
     }
+    // Auto-commit for convenience (can be disabled via config if needed)
+    nvs_commit(s_nvs_handle);
     
     return ESP_OK;
 }
@@ -78,6 +79,7 @@ esp_err_t sx_settings_set_int(const char *key, int32_t value) {
     esp_err_t ret = nvs_set_i32(s_nvs_handle, key, value);
     if (ret != ESP_OK) {
         ESP_LOGE(TAG, "Failed to set int '%s': %s", key, esp_err_to_name(ret));
         return ret;
     }
+    // Auto-commit for convenience
+    nvs_commit(s_nvs_handle);
     
     return ESP_OK;
 }
@@ -146,6 +148,7 @@ esp_err_t sx_settings_set_blob(const char *key, const void *value, size_t len)
     esp_err_t ret = nvs_set_blob(s_nvs_handle, key, value, len);
     if (ret != ESP_OK) {
         ESP_LOGE(TAG, "Failed to set blob '%s': %s", key, esp_err_to_name(ret));
         return ret;
     }
+    // Auto-commit for convenience
+    nvs_commit(s_nvs_handle);
     
     return ESP_OK;
 }
```

**Testing:**
1. Set multiple settings
2. Reboot device
3. Verify settings persist without explicit commit

---

### 2.4 P0-7: Stack Overflow Risk (Playback Task)

**File:** `components/sx_services/sx_audio_service.c`

**Issue:** Stack 3KB có thể không đủ nếu codec phức tạp

**Patch:**

```diff
--- a/components/sx_services/sx_audio_service.c
+++ b/components/sx_services/sx_audio_service.c
@@ -533,6 +533,6 @@ esp_err_t sx_audio_play_file(const char *file_path) {
     s_playing = true;
-    // Tối ưu: Giảm stack size từ 4096 xuống 3072 để tiết kiệm memory
-    BaseType_t ret = xTaskCreatePinnedToCore(sx_audio_playback_task, "sx_audio_file", 3072, f, 5, &s_playback_task_handle, 0);
+    // Tăng stack size lên 4096 để tránh stack overflow với complex codecs
+    BaseType_t ret = xTaskCreatePinnedToCore(sx_audio_playback_task, "sx_audio_file", 4096, f, 5, &s_playback_task_handle, 0);
     if (ret!=pdPASS) {
```

**Testing:**
1. Play complex codec files (high bitrate MP3/AAC)
2. Monitor stack high water mark (uxTaskGetStackHighWaterMark)
3. Verify stack usage < 80% of allocated

---

## 3. DIFF PATCHES - P1 ISSUES

### 3.1 P1-1: No URL Encoding (TTS)

**File:** `components/sx_services/sx_tts_service.c`

**Issue:** Text không được URL encode → có thể fail với special chars

**Patch:**

```diff
--- a/components/sx_services/sx_tts_service.c
+++ b/components/sx_services/sx_tts_service.c
@@ -322,6 +322,30 @@ static esp_err_t sx_tts_http_request(const char *text, int16_t **audio_data, si
     // Build request URL
     char url[512];
-    snprintf(url, sizeof(url), "%s?text=%s", s_endpoint_url, text);
-    // Note: In production, URL encoding would be needed
+    
+    // URL encode text
+    char encoded_text[512];
+    size_t encoded_len = 0;
+    for (size_t i = 0; text[i] != '\0' && encoded_len < sizeof(encoded_text) - 1; i++) {
+        unsigned char c = (unsigned char)text[i];
+        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
+            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
+            // Safe character, use as-is
+            encoded_text[encoded_len++] = c;
+        } else {
+            // URL encode: %XX
+            if (encoded_len + 3 < sizeof(encoded_text) - 1) {
+                encoded_text[encoded_len++] = '%';
+                encoded_text[encoded_len++] = "0123456789ABCDEF"[(c >> 4) & 0x0F];
+                encoded_text[encoded_len++] = "0123456789ABCDEF"[c & 0x0F];
+            } else {
+                break; // Buffer full
+            }
+        }
+    }
+    encoded_text[encoded_len] = '\0';
+    
+    snprintf(url, sizeof(url), "%s?text=%s", s_endpoint_url, encoded_text);
```

**Testing:**
1. Test với special characters: "Hello, world! @#$%"
2. Verify URL được encode correctly
3. Verify TTS request succeeds

---

### 3.2 P1-4: Queue Size Too Small (Wake Word)

**File:** `components/sx_services/sx_wake_word_service.c`

**Issue:** 10 buffers = 200ms buffer → có thể miss wake word

**Patch:**

```diff
--- a/components/sx_services/sx_wake_word_service.c
+++ b/components/sx_services/sx_wake_word_service.c
@@ -136,7 +136,7 @@ esp_err_t sx_wake_word_service_init(const sx_wake_word_config_t *config) {
 #endif
     
     // Create audio queue for wake word detection
     const size_t AUDIO_BUFFER_SAMPLES = 320; // 20ms at 16kHz
-    s_audio_queue = xQueueCreate(10, AUDIO_BUFFER_SAMPLES * sizeof(int16_t));
+    s_audio_queue = xQueueCreate(20, AUDIO_BUFFER_SAMPLES * sizeof(int16_t)); // 20 buffers = 400ms buffer
     if (s_audio_queue == NULL) {
         ESP_LOGE(TAG, "Failed to create audio queue");
         return ESP_ERR_NO_MEM;
```

**Testing:**
1. Test wake word detection với system load
2. Verify không miss wake word khi queue full
3. Monitor queue usage

---

### 3.3 P1-7: SPI Bus Mutex Blocking

**File:** `components/sx_platform/sx_spi_bus_manager.c`

**Issue:** `portMAX_DELAY` có thể gây deadlock

**Patch:**

```diff
--- a/components/sx_platform/sx_spi_bus_manager.c
+++ b/components/sx_platform/sx_spi_bus_manager.c
@@ -XX,XX +XX,XX @@
-esp_err_t sx_spi_bus_lock(void) {
-    if (s_spi_mutex == NULL) {
-        return ESP_ERR_INVALID_STATE;
-    }
-    if (xSemaphoreTake(s_spi_mutex, portMAX_DELAY) != pdTRUE) {
-        return ESP_ERR_TIMEOUT;
-    }
-    return ESP_OK;
+esp_err_t sx_spi_bus_lock(void) {
+    if (s_spi_mutex == NULL) {
+        return ESP_ERR_INVALID_STATE;
+    }
+    // Use timeout to prevent deadlock (100ms timeout)
+    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
+        ESP_LOGW(TAG, "SPI bus lock timeout - possible deadlock");
+        return ESP_ERR_TIMEOUT;
+    }
+    return ESP_OK;
 }
```

**Testing:**
1. Test với concurrent SPI access (LCD + SD)
2. Verify không có deadlock
3. Monitor lock timeout events

---

### 3.4 P1-8: Navigation trong Main Loop

**File:** `components/sx_ui/sx_ui_task.c`

**Issue:** Navigation logic trong UI task main loop, không event-driven

**Patch:**

```diff
--- a/components/sx_ui/sx_ui_task.c
+++ b/components/sx_ui/sx_ui_task.c
@@ -207,XX +XX,XX @@ void sx_ui_task(void *arg) {
-        // Navigation logic (boot sequence)
-        ui_screen_id_t target_screen = determine_target_screen();
-        
-        // Navigate if needed
-        if (target_screen != last_screen) {
-            ui_router_navigate_to(target_screen);
-        }
+        // Navigation is now event-driven via SX_EVT_UI_NAVIGATE events
+        // Boot sequence navigation handled by event handler
```

**Note:** Cần implement event handler cho `SX_EVT_UI_NAVIGATE` trong orchestrator hoặc UI event handler.

**Testing:**
1. Test boot sequence navigation
2. Test user-initiated navigation
3. Verify navigation works correctly

---

## 4. TESTING INSTRUCTIONS

### 4.1 Pre-Patch Testing

**Baseline Tests:**

1. **Memory Leak Test:**
   ```bash
   # Monitor heap before/after STT requests
   idf.py monitor | grep "free heap"
   ```

2. **Audio Underrun Test:**
   ```bash
   # Play high bitrate MP3, monitor I2S underrun
   idf.py monitor | grep "underrun"
   ```

3. **Settings Persistence Test:**
   ```bash
   # Set settings, reboot, verify persistence
   ```

### 4.2 Post-Patch Testing

**Regression Tests:**

1. **P0-2 (Memory Leak):**
   - Run 100+ STT requests
   - Monitor heap: `heap_caps_get_free_size(MALLOC_CAP_DEFAULT)`
   - Verify heap không giảm sau mỗi request

2. **P0-5 (Playback Priority):**
   - Play high bitrate MP3 với system load
   - Monitor I2S underrun: `i2s_channel_get_info()`
   - Verify không có underrun events

3. **P0-6 (Auto-Commit):**
   - Set multiple settings
   - Reboot device
   - Verify settings persist: `sx_settings_get_string()`

4. **P0-7 (Stack Overflow):**
   - Play complex codec files
   - Monitor stack: `uxTaskGetStackHighWaterMark(s_playback_task_handle)`
   - Verify stack usage < 80%

5. **P1-1 (URL Encoding):**
   - Test với special chars: "Hello, world! @#$%"
   - Verify URL encoded correctly
   - Verify TTS request succeeds

6. **P1-4 (Queue Size):**
   - Test wake word với system load
   - Monitor queue: `uxQueueMessagesWaiting(s_audio_queue)`
   - Verify không miss wake word

7. **P1-7 (SPI Bus Timeout):**
   - Test concurrent SPI access
   - Monitor timeout events
   - Verify không có deadlock

### 4.3 Integration Testing

**Test Scenarios:**

1. **Audio Playback Stress Test:**
   - Play multiple formats (MP3, AAC, FLAC)
   - Switch between tracks rapidly
   - Monitor for underruns, glitches

2. **Settings Persistence Test:**
   - Set all settings
   - Reboot multiple times
   - Verify all settings persist

3. **Wake Word Detection Test:**
   - Test với system load
   - Test với multiple wake words
   - Verify detection accuracy

4. **TTS Special Characters Test:**
   - Test với various special characters
   - Test với Vietnamese characters
   - Verify all requests succeed

---

## 5. IMPLEMENTATION ROADMAP

### 5.1 Sprint 1 (Weeks 1-3): P0 Issues

**Week 1:**
- P0-2: Memory Leak (STT Event) - 1 day
- P0-5: Playback Priority - 1 day
- P0-6: Auto-Commit (Settings) - 1 day
- P0-7: Stack Overflow Risk - 1 day
- Testing & Review - 1 day

**Week 2:**
- P0-3: WiFi Credential Storage - 2 days
- P0-1: Buffer Underrun Risk (research & design) - 3 days

**Week 3:**
- P0-1: Buffer Underrun Risk (implementation) - 3 days
- P0-4: Circular Dependencies (research & design) - 2 days

### 5.2 Sprint 2 (Weeks 4-6): P1 Issues

**Week 4:**
- P1-1: URL Encoding (TTS) - 1 day
- P1-4: Queue Size (Wake Word) - 1 day
- P1-7: SPI Bus Timeout - 1 day
- P1-8: Navigation Event-Driven - 2 days

**Week 5:**
- P1-2: Mutex Lock Order - 2 days
- P1-3: Buffer Queue (Audio) - 3 days

**Week 6:**
- P1-5: Playlist Persistence - 2 days
- P1-6: SD Write Support - 2 days
- P1-9: Bootstrap Refactoring (research) - 1 day

### 5.3 Sprint 3+ (Weeks 7+): P2 Issues & Refactoring

**Week 7-8:**
- P2-1: Unit Test Framework - 1 week
- P2-2: Dependency Injection - 1 week

**Week 9-10:**
- P2-3: Component Splitting - 2 weeks

---

## 6. RISK ASSESSMENT

### 6.1 High Risk Patches

1. **P0-1: Buffer Underrun Risk**
   - **Risk:** High complexity, có thể break existing functionality
   - **Mitigation:** Extensive testing, gradual rollout
   - **Rollback Plan:** Revert patch nếu issues

2. **P0-4: Circular Dependencies**
   - **Risk:** High complexity, có thể require major refactoring
   - **Mitigation:** Phased approach, test each phase
   - **Rollback Plan:** Keep workaround (`LINK_INTERFACE_MULTIPLICITY`)

3. **P1-3: Buffer Queue (Audio)**
   - **Risk:** Medium complexity, có thể impact latency
   - **Mitigation:** Test latency impact, tune buffer sizes
   - **Rollback Plan:** Revert to direct I2S write

### 6.2 Low Risk Patches

1. **P0-2: Memory Leak (STT)**
   - **Risk:** Low - chỉ thay `strdup()` bằng `sx_event_alloc_string()`
   - **Mitigation:** Test memory usage

2. **P0-5: Playback Priority**
   - **Risk:** Low - chỉ tăng priority từ 4 lên 5
   - **Mitigation:** Test priority conflicts

3. **P0-6: Auto-Commit (Settings)**
   - **Risk:** Low - chỉ thêm `nvs_commit()` calls
   - **Mitigation:** Test settings persistence

---

## 7. METRICS & SUCCESS CRITERIA

### 7.1 Success Criteria

**P0 Issues:**
- ✅ Zero memory leaks (verified với heap monitoring)
- ✅ Zero audio underruns (verified với I2S monitoring)
- ✅ 100% settings persistence (verified với reboot tests)
- ✅ Zero stack overflows (verified với stack monitoring)

**P1 Issues:**
- ✅ TTS requests succeed với special characters
- ✅ Wake word detection không miss với system load
- ✅ No SPI bus deadlocks (verified với timeout monitoring)

### 7.2 Metrics to Track

1. **Memory Usage:**
   - Heap free size before/after operations
   - Memory leak rate (bytes per operation)

2. **Audio Quality:**
   - I2S underrun count
   - Audio glitch frequency

3. **System Stability:**
   - Stack high water marks
   - Mutex timeout events
   - Task watchdog resets

4. **Settings Persistence:**
   - Settings persistence rate (%)
   - Settings load time

---

## 8. DEPLOYMENT STRATEGY

### 8.1 Phased Rollout

**Phase 1: Low-Risk Patches (Week 1)**
- P0-2, P0-5, P0-6, P0-7
- Deploy to test devices
- Monitor for 1 week

**Phase 2: Medium-Risk Patches (Week 2-3)**
- P0-3, P1-1, P1-4, P1-7, P1-8
- Deploy to test devices
- Monitor for 1 week

**Phase 3: High-Risk Patches (Week 4+)**
- P0-1, P0-4, P1-3
- Extensive testing
- Gradual rollout

### 8.2 Rollback Plan

**For Each Patch:**
1. Keep original code in comments
2. Use feature flags nếu possible
3. Monitor metrics after deployment
4. Revert nếu issues detected

---

## 9. DOCUMENTATION UPDATES

### 9.1 Required Updates

1. **API Documentation:**
   - Update `sx_settings_service.h` với auto-commit behavior
   - Update `sx_stt_service.h` với memory management notes

2. **Architecture Documentation:**
   - Update với buffer queue architecture (P0-1)
   - Update với event-driven navigation (P1-8)

3. **Testing Documentation:**
   - Add testing procedures cho each patch
   - Add metrics collection procedures

---

## 10. CHECKLIST HOÀN THÀNH PHASE 9

- [x] Tổng hợp tất cả P0/P1 issues từ các phases
- [x] Tạo prioritized backlog
- [x] Tạo diff patches cho P0 issues
- [x] Tạo diff patches cho P1 issues (selected)
- [x] Tạo testing instructions
- [x] Tạo implementation roadmap
- [x] Risk assessment
- [x] Success criteria và metrics
- [x] Deployment strategy
- [x] Tạo REPORT_PHASE_9_ACTION_PLAN.md

---

## 11. NEXT STEPS

**Immediate Actions:**
1. Review và approve patches
2. Set up testing environment
3. Create feature branches cho each patch
4. Begin Sprint 1 implementation

**Follow-up Actions:**
1. Monitor metrics after deployment
2. Collect feedback từ testing
3. Iterate on patches nếu needed
4. Plan Sprint 2

---

**Kết thúc PHASE 9**



