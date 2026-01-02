# SIMPLEXL Quality Gates
## Ngưỡng Chất Lượng và Performance Requirements

> **Version:** 1.3  
> **Section:** 10 SIMPLEXL_ARCH v1.3  
> **Mục đích:** Định nghĩa ngưỡng chất lượng và performance cho CI và production

---

## 1. TỔNG QUAN

Tài liệu này định nghĩa quality gates (ngưỡng chất lượng) cho SimpleXL architecture:
- **CI Gates:** Checks bắt buộc trong CI pipeline
- **Performance Thresholds:** Ngưỡng performance cho board thật
- **Memory Budgets:** Giới hạn memory usage
- **Metrics Thresholds:** Ngưỡng cho observability metrics

---

## 2. CI GATES (Bắt Buộc)

### 2.1 Architecture Compliance Checks

**MUST PASS:**

1. ✅ **Forbidden Includes Check:**
   - Script: `scripts/check_architecture.sh`
   - **Threshold:** 0 violations
   - **Action:** Fail build if violations found

2. ✅ **Build (Debug Profile):**
   - **Threshold:** Build must succeed
   - **Action:** Fail CI if build fails

3. ✅ **Unit Tests (Minimum):**
   - **Tests Required:**
     - Dispatcher event post/dispatch
     - State version increment
     - Dirty mask updates
   - **Threshold:** All tests PASS
   - **Action:** Fail CI if tests fail

4. ✅ **Static Analysis (if enabled):**
   - Tools: `clang-tidy` / `cppcheck`
   - **Threshold:** No critical issues
   - **Action:** Warn on issues, fail on critical

---

## 3. PERFORMANCE THRESHOLDS (Board Thật)

### 3.1 Target Board: ESP32-S3 N16R8 / PSRAM 8MB

#### 3.1.1 Boot Performance

| Metric | Threshold | Measurement |
|--------|-----------|-------------|
| Boot heap free | >= 100 KB | After bootstrap complete |
| PSRAM free | >= 6 MB | After bootstrap complete |
| Boot time | <= 5 seconds | From reset to UI ready |

#### 3.1.2 Runtime Performance

| Metric | Threshold | Measurement |
|--------|-----------|-------------|
| UI frame time (avg) | <= 16 ms | 60 FPS target |
| UI frame time (P95) | <= 20 ms | 95th percentile |
| UI input latency | <= 50 ms | Touch → render (P95) |
| Audio timing events | No backlog > 2× tick window | Audio task |
| Event queue depth (CRITICAL) | <= 4 | Max queue size 8 |
| Event queue depth (HIGH) | <= 8 | Max queue size 16 |
| Event queue depth (NORMAL) | <= 16 | Max queue size 32 |
| Event queue depth (LOW) | <= 8 | Max queue size 16 |

#### 3.1.3 Memory Budgets

| Resource | Budget | Notes |
|----------|--------|-------|
| `sx_state_t` size | <= 8 KB | Snapshot size |
| Event payload size | <= 256 bytes | Per event (POD only) |
| Screen LVGL objects | <= 2 KB per screen | Average per screen |
| Service internal state | <= 4 KB per service | Average per service |

---

## 4. METRICS THRESHOLDS

### 4.1 Event Metrics

| Metric | Threshold | Notes |
|--------|-----------|-------|
| Dropped events (CRITICAL) | = 0 | Never drop CRITICAL |
| Dropped events (HIGH) | <= 10/hour | Rate-limited |
| Dropped events (NORMAL) | <= 100/hour | Acceptable for telemetry |
| Dropped events (LOW) | <= 1000/hour | Acceptable for logging |
| Coalesced events | No limit | Optimization feature |

### 4.2 State Metrics

| Metric | Threshold | Notes |
|--------|-----------|-------|
| State version increment rate | <= 1000/sec | Max update rate |
| Dirty mask coverage | All domains tracked | All state domains have dirty bits |

### 4.3 UI Metrics

| Metric | Threshold | Notes |
|--------|-----------|-------|
| UI render time (avg) | <= 16 ms | 60 FPS |
| UI render time (P95) | <= 20 ms | Smooth rendering |
| UI render time (P99) | <= 30 ms | Acceptable spikes |

### 4.4 Memory Metrics

| Metric | Threshold | Notes |
|--------|-----------|-------|
| Heap free (min) | >= 50 KB | Minimum free heap |
| PSRAM free (min) | >= 4 MB | Minimum free PSRAM |
| Heap fragmentation | <= 20% | Acceptable fragmentation |

---

## 5. ARCHITECTURE COMPLIANCE THRESHOLDS

### 5.1 Enforcement Checks

| Check | Threshold | Notes |
|-------|-----------|-------|
| LVGL calls outside UI task | = 0 | Runtime assert enabled |
| Direct `lvgl.h` includes | = 0 | Must use `sx_lvgl.h` |
| `sx_lvgl.h` includes outside sx_ui | = 0 | Compile-time guard |
| Service LVGL violations | = 0 | Services use events |
| Architecture boundary violations | = 0 | Script check passes |

---

## 6. BACKPRESSURE POLICY THRESHOLDS

### 6.1 Queue Sizing

| Priority | Queue Size | Drop Policy | Block Policy |
|----------|------------|-------------|--------------|
| CRITICAL | 8 | Not allowed | Block up to 100ms |
| HIGH | 16 | Drop if full | Block up to 50ms |
| NORMAL | 32 | Drop if full | Not allowed |
| LOW | 16 | Drop if full | Not allowed |

### 6.2 Drop Thresholds

| Priority | Max Drops/Hour | Action |
|----------|----------------|--------|
| CRITICAL | 0 | Alert immediately |
| HIGH | 10 | Log warning |
| NORMAL | 100 | Log info |
| LOW | 1000 | Acceptable |

---

## 7. LIFECYCLE THRESHOLDS

### 7.1 Service Lifecycle

| Phase | Max Time | Notes |
|-------|----------|-------|
| Service init | <= 1 second | Per service |
| Service start | <= 500 ms | Per service |
| Service stop | <= 500 ms | Per service |
| Service deinit | <= 1 second | Per service |

### 7.2 Screen Lifecycle

| Phase | Max Time | Notes |
|-------|----------|-------|
| Screen create | <= 100 ms | LVGL object creation |
| Screen destroy | <= 50 ms | LVGL object cleanup |
| Screen on_enter | <= 50 ms | Animation start |
| Screen on_exit | <= 50 ms | Animation stop |

---

## 8. TEST COVERAGE THRESHOLDS

### 8.1 Unit Test Coverage

| Component | Min Coverage | Notes |
|-----------|--------------|-------|
| sx_dispatcher | >= 80% | Core event system |
| sx_orchestrator | >= 70% | State management |
| sx_event_handler | >= 70% | Event processing |
| sx_state_helper | >= 90% | State version/dirty_mask |

### 8.2 Architecture Compliance Tests

| Test Category | Required Tests | Notes |
|---------------|----------------|-------|
| Event post/dispatch | All priorities | Test priority ordering |
| Backpressure policies | DROP, COALESCE, BLOCK | Test all policies |
| State version/dirty_mask | Version increment, mask updates | Test state changes |
| LVGL call firewall | Runtime assert | Test thread verification |

---

## 9. DOCUMENTATION THRESHOLDS

### 9.1 Required Documentation

| Document | Status | Notes |
|----------|--------|-------|
| SIMPLEXL_ARCH_v1.3.md | ✅ Required | Architecture spec |
| SIMPLEXL_RESOURCE_OWNERSHIP.md | ✅ Required | Resource ownership table |
| SIMPLEXL_QUALITY_GATES.md | ✅ Required | This document |
| API documentation | ✅ Recommended | Function comments |

---

## 10. CI PIPELINE GATES

### 10.1 Pre-commit Checks

1. ✅ `check_architecture.sh` - Must pass
2. ✅ Build (debug) - Must succeed
3. ✅ Linter (if enabled) - No critical issues

### 10.2 CI Pipeline Checks

1. ✅ Build (debug + release) - Must succeed
2. ✅ Unit tests - All must pass
3. ✅ Architecture compliance tests - All must pass
4. ✅ Static analysis (if enabled) - No critical issues
5. ✅ Documentation check - Required docs exist

### 10.3 Pre-release Checks

1. ✅ Performance tests on target board - All thresholds met
2. ✅ Memory leak tests - No leaks detected
3. ✅ Stress tests - System stable under load
4. ✅ Integration tests - All features working

---

## 11. THRESHOLD ENFORCEMENT

### 11.1 CI Enforcement

- **Hard Failures:** Build fails, tests fail, architecture violations
- **Soft Warnings:** Performance below threshold, high memory usage

### 11.2 Production Monitoring

- **Metrics Collection:** Required metrics tracked (Section 9.2)
- **Alerting:** Alert on critical threshold violations
- **Logging:** Log warnings for non-critical violations

---

## 12. BOARD-SPECIFIC THRESHOLDS

### 12.1 ESP32-S3 N16R8 (8MB PSRAM)

**Default thresholds as defined above.**

### 12.2 Other Boards

Thresholds should be adjusted based on:
- Available RAM/PSRAM
- CPU frequency
- Display resolution
- Feature set enabled

---

**Kết thúc Quality Gates Document.**






