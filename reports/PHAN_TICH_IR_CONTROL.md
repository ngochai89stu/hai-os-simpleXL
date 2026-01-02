# Phân Tích Sâu Tính Năng IR Control

## 📋 Tổng Quan

Tính năng IR Control cho phép ESP32 điều khiển các thiết bị hồng ngoại (TV, điều hòa, quạt, v.v.) thông qua giao diện người dùng trên màn hình. Tính năng này được triển khai ở hai lớp: **Service Layer** (sx_ir_service) và **UI Layer** (screen_ir_control).

---

## 🔍 Kiến Trúc và Cách Hoạt Động

### 1. Service Layer (`sx_ir_service.c/h`)

#### 1.1 Khởi Tạo và Cấu Hình

```337:353:components/sx_core/sx_bootstrap.c
    // IR (stub)
    sx_ir_config_t ir_cfg = {
        .tx_gpio = 14,  // IR TX pin (confirmed)
        .rx_gpio = -1,  // IR RX not used for now
        .carrier_hz = 38000,
    };
    esp_err_t ir_ret = sx_ir_service_init(&ir_cfg);
    if (ir_ret != ESP_OK) {
        ESP_LOGW(TAG, "IR service init failed (non-critical): %s", esp_err_to_name(ir_ret));
    } else {
        ir_ret = sx_ir_service_start();
        if (ir_ret != ESP_OK) {
            ESP_LOGW(TAG, "IR service start failed (non-critical): %s", esp_err_to_name(ir_ret));
        } else {
            ESP_LOGI(TAG, "IR service started");
        }
    }
```

**Cấu hình:**
- **TX GPIO:** Pin 14 (được xác nhận)
- **RX GPIO:** -1 (chưa sử dụng)
- **Carrier Frequency:** 38kHz (tần số chuẩn cho hầu hết thiết bị IR)

#### 1.2 RMT (Remote Control) Hardware

Service sử dụng ESP32 RMT (Remote Control) peripheral để phát tín hiệu IR:

```28:72:components/sx_services/sx_ir_service.c
    if (s_cfg.tx_gpio >= 0) {
        // Phase 4: Initialize RMT TX channel for IR
        rmt_tx_channel_config_t tx_chan_cfg = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .gpio_num = s_cfg.tx_gpio,
            .mem_block_symbols = 64,
            .resolution_hz = 1000000, // 1MHz = 1us per tick
            .trans_queue_depth = 4,
            .flags = {
                .invert_out = false,
                .with_dma = false,
            },
        };
        
        esp_err_t ret = rmt_new_tx_channel(&tx_chan_cfg, &s_rmt_tx_chan);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "rmt_new_tx_channel failed: %s", esp_err_to_name(ret));
            s_initialized = true;
            return ESP_OK; // non-critical
        }
        
        // Create copy encoder for raw symbols
        rmt_copy_encoder_config_t copy_encoder_cfg = {};
        ret = rmt_new_copy_encoder(&copy_encoder_cfg, &s_rmt_copy_encoder);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "rmt_new_copy_encoder failed: %s", esp_err_to_name(ret));
            rmt_del_channel(s_rmt_tx_chan);
            s_rmt_tx_chan = NULL;
            s_initialized = true;
            return ESP_OK;
        }
        
        // Enable RMT channel
        ret = rmt_enable(s_rmt_tx_chan);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "rmt_enable failed: %s", esp_err_to_name(ret));
            rmt_del_encoder(s_rmt_copy_encoder);
            s_rmt_copy_encoder = NULL;
            rmt_del_channel(s_rmt_tx_chan);
            s_rmt_tx_chan = NULL;
            s_initialized = true;
            return ESP_OK;
        }
        
        ESP_LOGI(TAG, "IR RMT TX initialized: gpio=%d carrier=%lu Hz", s_cfg.tx_gpio, (unsigned long)s_cfg.carrier_hz);
    }
```

**Thông số RMT:**
- **Resolution:** 1MHz (1 microsecond per tick) - đủ chính xác cho IR timing
- **Memory:** 64 symbols per block
- **Queue Depth:** 4 transactions
- **Encoder:** Copy encoder (gửi raw symbols)

#### 1.3 Gửi IR Command

```92:139:components/sx_services/sx_ir_service.c
esp_err_t sx_ir_send_raw(const uint16_t *pulses_us, size_t count) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (pulses_us == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_rmt_tx_chan == NULL || s_rmt_copy_encoder == NULL) {
        ESP_LOGE(TAG, "RMT TX channel or encoder not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Phase 4: Convert pulses_us to RMT symbols
    // Allocate symbols: each pulse needs one symbol (high/low alternates)
    size_t symbol_count = count;
    rmt_symbol_word_t *rmt_symbols = malloc(symbol_count * sizeof(rmt_symbol_word_t));
    if (rmt_symbols == NULL) {
        ESP_LOGE(TAG, "Failed to allocate RMT symbols");
        return ESP_ERR_NO_MEM;
    }
    
    for (size_t i = 0; i < symbol_count; i++) {
        uint16_t pulse_us = pulses_us[i];
        rmt_symbols[i].level0 = (i % 2 == 0) ? 1 : 0; // Alternate high/low
        rmt_symbols[i].duration0 = pulse_us; // Duration in microseconds (1MHz = 1us per tick)
        rmt_symbols[i].level1 = 0;
        rmt_symbols[i].duration1 = 0;
    }
    
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags = {
            .eot_level = 0,
        },
    };
    
    // Transmit symbols using copy encoder (size in bytes)
    esp_err_t ret = rmt_transmit(s_rmt_tx_chan, s_rmt_copy_encoder, rmt_symbols, symbol_count * sizeof(rmt_symbol_word_t), &tx_cfg);
    free(rmt_symbols);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_transmit failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "IR send raw: %u pulses", (unsigned)count);
    return ESP_OK;
}
```

**Quy trình:**
1. Nhận mảng `pulses_us` (thời lượng từng xung tính bằng microsecond)
2. Chuyển đổi sang RMT symbols (high/low xen kẽ)
3. Gửi qua RMT channel
4. Giải phóng bộ nhớ

**⚠️ Vấn đề:** Code không thực hiện **carrier modulation** (điều chế tần số 38kHz). RMT chỉ gửi raw pulses, không có tần số sóng mang.

---

### 2. UI Layer (`screen_ir_control.c/h`)

#### 2.1 Cấu Trúc Giao Diện

```46:154:components/sx_ui/screens/screen_ir_control.c
static void on_create(void) {
    ESP_LOGI(TAG, "IR Control screen onCreate");
    
    if (!lvgl_port_lock(0)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return;
    }
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        lvgl_port_unlock();
        return;
    }
    
    s_container = container;
    
    // Set background
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "IR Control");
    
    // Create content area
    s_content = lv_obj_create(container);
    lv_obj_set_size(s_content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(s_content, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_content, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Status label
    s_status_label = lv_label_create(s_content);
    lv_label_set_text(s_status_label, "Select a device to control");
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x888888), 0);
    lv_obj_set_width(s_status_label, LV_PCT(100));
    
    // Device list (scrollable) - matching web demo
    s_device_list = lv_obj_create(s_content);
    lv_obj_set_size(s_device_list, LV_PCT(100), 150);
    lv_obj_set_style_bg_opa(s_device_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_device_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_device_list, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_device_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_device_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Device list
    const char* device_names[] = {"Air Conditioner", "TV", "Fan"};
    for (int i = 0; i < IR_DEVICE_MAX; i++) {
        lv_obj_t *device_item = lv_obj_create(s_device_list);
        lv_obj_set_size(device_item, LV_PCT(100), 50);
        lv_obj_set_style_bg_color(device_item, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(device_item, lv_color_hex(0x3a3a3a), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_width(device_item, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(device_item, 10, LV_PART_MAIN);
        lv_obj_set_style_radius(device_item, 5, LV_PART_MAIN);
        lv_obj_set_user_data(device_item, (void *)(intptr_t)i);
        lv_obj_add_event_cb(device_item, device_item_click_cb, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t *label = lv_label_create(device_item);
        lv_label_set_text(label, device_names[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
    }
    
    // Control panel (buttons for selected device) - matching web demo
    s_control_panel = lv_obj_create(s_content);
    lv_obj_set_size(s_control_panel, LV_PCT(100), LV_PCT(100) - 160);
    lv_obj_set_style_bg_color(s_control_panel, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_control_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_control_panel, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_control_panel, 15, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_control_panel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_control_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_control_panel, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_column(s_control_panel, 10, LV_PART_MAIN);
    
    // Control buttons (Power, Temp+, Temp-, Mode, etc.)
    const char* control_labels[] = {"Power", "Temp+", "Temp-", "Mode", "Fan", "Timer"};
    for (int i = 0; i < IR_CMD_MAX; i++) {
        lv_obj_t *btn = lv_btn_create(s_control_panel);
        lv_obj_set_size(btn, 80, 50);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x5b7fff), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 5, LV_PART_MAIN);
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, control_btn_cb, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, control_labels[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(label);
    }
    
    // Initially disable control panel (no device selected)
    lv_obj_add_flag(s_control_panel, LV_OBJ_FLAG_HIDDEN);
    
    lvgl_port_unlock();
    
    // Verification: Log screen creation
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_create(SCREEN_ID_IR_CONTROL, "IR Control", container, s_content);
    #endif
}
```

**Giao diện bao gồm:**
- **Device List:** Danh sách thiết bị (Air Conditioner, TV, Fan)
- **Control Panel:** Các nút điều khiển (Power, Temp+, Temp-, Mode, Fan, Timer)
- **Status Label:** Hiển thị trạng thái hiện tại

#### 2.2 IR Encoding (Mã Hóa IR)

```200:248:components/sx_ui/screens/screen_ir_control.c
static void send_ir_command(ir_device_type_t device, ir_command_type_t command) {
    // Note: IR service requires raw pulse patterns
    // For now, we'll create simple patterns based on device and command
    // In a real implementation, these would be learned or loaded from a database
    
    // Simple IR pattern: header + data
    // Format: [header_mark, header_space, data_mark, data_space, ...]
    // This is a placeholder - real IR patterns are device-specific
    
    uint16_t ir_pattern[10] = {0};
    size_t pattern_count = 0;
    
    // Generate a simple pattern based on device and command
    // Real implementation would use learned patterns or IR code database
    ir_pattern[0] = 9000;  // Header mark (9ms)
    ir_pattern[1] = 4500;  // Header space (4.5ms)
    ir_pattern[2] = 560;   // Data mark (0.56ms)
    
    // Encode device and command into pattern
    uint8_t data = (device << 4) | command;
    for (int i = 0; i < 8; i++) {
        ir_pattern[3 + i * 2] = 560;  // Mark
        if (data & (1 << i)) {
            ir_pattern[3 + i * 2 + 1] = 1690;  // Space for '1' (1.69ms)
        } else {
            ir_pattern[3 + i * 2 + 1] = 560;   // Space for '0' (0.56ms)
        }
    }
    pattern_count = 3 + 16;  // Header + 8 bits
    
    // Send IR command
    esp_err_t ret = sx_ir_send_raw(ir_pattern, pattern_count);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "IR command sent: device=%d, command=%d", device, command);
        if (s_status_label != NULL && lvgl_port_lock(0)) {
            const char* cmd_names[] = {"Power", "Temp+", "Temp-", "Mode", "Fan", "Timer"};
            char status[64];
            snprintf(status, sizeof(status), "Sent: %s", cmd_names[command]);
            lv_label_set_text(s_status_label, status);
            lvgl_port_unlock();
        }
    } else {
        ESP_LOGE(TAG, "Failed to send IR command: %s", esp_err_to_name(ret));
        if (s_status_label != NULL && lvgl_port_lock(0)) {
            lv_label_set_text(s_status_label, "Failed to send IR command");
            lvgl_port_unlock();
        }
    }
}
```

**Encoding Logic:**
- Sử dụng protocol tương tự **NEC protocol**
- **Header:** 9ms mark + 4.5ms space
- **Data:** 8 bits (device + command)
  - Bit '1': 560µs mark + 1690µs space
  - Bit '0': 560µs mark + 560µs space

**⚠️ Vấn đề:**
1. **Hardcoded pattern:** Không phải mã thật của thiết bị
2. **Không có carrier modulation:** RMT không điều chế 38kHz
3. **Không có learning:** Không thể học mã từ remote thật
4. **Không có database:** Không có thư viện mã IR

---

## ✅ Đánh Giá Hoàn Thiện

### Đã Hoàn Thành ✅

1. **Service Layer:**
   - ✅ RMT TX channel initialization
   - ✅ Raw pulse transmission
   - ✅ Error handling cơ bản
   - ✅ Memory management

2. **UI Layer:**
   - ✅ Giao diện người dùng đầy đủ
   - ✅ Device selection
   - ✅ Command buttons
   - ✅ Status feedback
   - ✅ Event handling

3. **Integration:**
   - ✅ Bootstrap integration
   - ✅ Screen registration
   - ✅ Service initialization

### Chưa Hoàn Thành ❌

1. **Carrier Modulation:**
   - ❌ Không có điều chế tần số 38kHz
   - ❌ RMT chỉ gửi raw pulses (DC signal)
   - ❌ Thiết bị IR không thể nhận tín hiệu

2. **IR Protocol Support:**
   - ❌ Chỉ có placeholder encoding (giống NEC)
   - ❌ Không hỗ trợ các protocol khác (RC5, Sony, Samsung, v.v.)
   - ❌ Không có protocol detection

3. **IR Learning:**
   - ❌ Không có chức năng học mã từ remote
   - ❌ Không có IR receiver support (rx_gpio = -1)
   - ❌ Không có signal capture và decode

4. **IR Code Database:**
   - ❌ Không có database mã IR
   - ❌ Không có support cho các thiết bị cụ thể
   - ❌ Hardcoded patterns không hoạt động với thiết bị thật

5. **Error Handling:**
   - ⚠️ Một số lỗi được bỏ qua (return ESP_OK khi fail)
   - ⚠️ Không có retry mechanism
   - ⚠️ Không có validation cho IR patterns

6. **Code Quality:**
   - ⚠️ Có compiler warning về array bounds (line 221)
   - ⚠️ Pattern count calculation có thể sai (line 228)

---

## 🚀 Đề Xuất Cải Thiện

### 1. Carrier Modulation (Ưu Tiên CAO) 🔴

**Vấn đề:** RMT không điều chế tần số 38kHz, thiết bị IR không nhận được tín hiệu.

**Giải pháp:** Sử dụng RMT carrier encoder hoặc LEDC để tạo carrier signal.

```c
// Option 1: Sử dụng RMT carrier encoder
rmt_carrier_config_t carrier_cfg = {
    .duty_cycle = 0.33,  // 33% duty cycle
    .frequency_hz = 38000,
    .flags = {
        .polarity_active_low = false,
    },
};
rmt_encoder_handle_t bytes_encoder = NULL;
rmt_bytes_encoder_config_t bytes_encoder_cfg = {
    .bit0 = {
        .level0 = 1,
        .duration0 = 560,
        .level1 = 0,
        .duration1 = 560,
    },
    .bit1 = {
        .level0 = 1,
        .duration0 = 560,
        .level1 = 0,
        .duration1 = 1690,
    },
    .flags = {
        .msb_first = 1,
    },
};
rmt_new_bytes_encoder(&bytes_encoder_cfg, &bytes_encoder);
rmt_apply_carrier(s_rmt_tx_chan, &carrier_cfg);
```

**Hoặc Option 2:** Sử dụng LEDC để tạo carrier, RMT để điều chế.

### 2. IR Protocol Library (Ưu Tiên CAO) 🔴

**Đề xuất:** Tích hợp thư viện IR protocol như `irremote` hoặc tự implement các protocol phổ biến:

- **NEC Protocol** (phổ biến nhất)
- **RC5 Protocol** (Philips)
- **Sony Protocol**
- **Samsung Protocol**
- **LG Protocol**

**Cấu trúc đề xuất:**
```c
typedef enum {
    IR_PROTOCOL_NEC,
    IR_PROTOCOL_RC5,
    IR_PROTOCOL_SONY,
    IR_PROTOCOL_SAMSUNG,
    IR_PROTOCOL_LG,
    IR_PROTOCOL_RAW,  // Raw pulses
} ir_protocol_t;

esp_err_t sx_ir_send_nec(uint8_t address, uint8_t command);
esp_err_t sx_ir_send_rc5(uint8_t address, uint8_t command);
esp_err_t sx_ir_send_sony(uint16_t command, uint8_t bits);
```

### 3. IR Learning Feature (Ưu Tiên TRUNG BÌNH) 🟡

**Chức năng:** Cho phép người dùng học mã từ remote thật.

**Yêu cầu:**
1. Implement IR receiver (RMT RX channel)
2. Signal capture và decode
3. Protocol detection
4. Lưu trữ mã vào NVS hoặc file
5. UI để học và quản lý mã

**API đề xuất:**
```c
esp_err_t sx_ir_start_learning(void);
esp_err_t sx_ir_stop_learning(void);
esp_err_t sx_ir_save_code(const char *device_name, const char *command_name, const uint16_t *pulses, size_t count);
esp_err_t sx_ir_load_code(const char *device_name, const char *command_name, uint16_t **pulses, size_t *count);
```

### 4. IR Code Database (Ưu Tiên TRUNG BÌNH) 🟡

**Chức năng:** Thư viện mã IR cho các thiết bị phổ biến.

**Cấu trúc đề xuất:**
```c
typedef struct {
    const char *brand;
    const char *model;
    ir_protocol_t protocol;
    uint32_t address;
    uint32_t power_code;
    uint32_t temp_up_code;
    uint32_t temp_down_code;
    // ... more codes
} ir_device_code_t;

const ir_device_code_t ir_code_database[] = {
    {"Samsung", "TV", IR_PROTOCOL_SAMSUNG, 0x707, 0x2, 0x10, 0x11, ...},
    {"LG", "AC", IR_PROTOCOL_LG, 0x88, 0x1, 0x20, 0x21, ...},
    // ...
};
```

**Lưu trữ:** Có thể lưu trong file JSON hoặc binary format trên SD card.

### 5. Cải Thiện UI (Ưu Tiên THẤP) 🟢

**Đề xuất:**
1. **Device Management Screen:**
   - Thêm/xóa thiết bị tùy chỉnh
   - Đặt tên thiết bị
   - Chọn protocol

2. **Learning UI:**
   - Nút "Learn" để học mã mới
   - Hiển thị mã đã học
   - Test mã trước khi lưu

3. **Advanced Controls:**
   - Temperature slider (thay vì chỉ + -)
   - Mode selector (Cool, Heat, Auto, Fan)
   - Fan speed selector
   - Timer settings

### 6. Fix Code Issues (Ưu Tiên CAO) 🔴

**Fix compiler warning:**
```c
// Line 221: Fix array bounds
uint16_t ir_pattern[19] = {0};  // 3 header + 16 data bits
// ...
for (int i = 0; i < 8; i++) {
    if (3 + i * 2 + 1 < 19) {  // Bounds check
        ir_pattern[3 + i * 2] = 560;
        if (data & (1 << i)) {
            ir_pattern[3 + i * 2 + 1] = 1690;
        } else {
            ir_pattern[3 + i * 2 + 1] = 560;
        }
    }
}
pattern_count = 3 + 16;  // Correct count
```

**Fix error handling:**
```c
// Don't return ESP_OK on failure
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "rmt_new_tx_channel failed: %s", esp_err_to_name(ret));
    return ret;  // Return actual error
}
```

### 7. Testing và Validation (Ưu Tiên TRUNG BÌNH) 🟡

**Đề xuất:**
1. **Unit Tests:**
   - Test IR encoding/decoding
   - Test protocol conversion
   - Test error handling

2. **Hardware Tests:**
   - Test với thiết bị IR thật
   - Đo tín hiệu bằng oscilloscope
   - Verify carrier frequency

3. **Integration Tests:**
   - Test end-to-end flow
   - Test UI interactions
   - Test learning feature

---

## 📊 Tổng Kết

### Trạng Thái Hiện Tại: **~40% Hoàn Thành**

**Đã có:**
- ✅ Cơ sở hạ tầng RMT
- ✅ UI đầy đủ
- ✅ Integration với hệ thống

**Còn thiếu:**
- ❌ Carrier modulation (quan trọng nhất)
- ❌ IR protocol support
- ❌ IR learning
- ❌ IR code database
- ❌ Error handling đầy đủ

### Lộ Trình Phát Triển Đề Xuất

1. **Phase 1 (Critical):**
   - Fix carrier modulation
   - Fix code issues
   - Implement NEC protocol

2. **Phase 2 (Important):**
   - IR learning feature
   - IR code database cơ bản
   - Protocol library (RC5, Sony)

3. **Phase 3 (Enhancement):**
   - Advanced UI
   - Device management
   - Testing framework

---

## 📚 Tài Liệu Tham Khảo

1. **ESP32 RMT Documentation:**
   - https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/rmt.html

2. **IR Remote Control Protocols:**
   - NEC Protocol: https://www.sbprojects.net/knowledge/ir/nec.php
   - RC5 Protocol: https://www.sbprojects.net/knowledge/ir/rc5.php
   - Sony Protocol: https://www.sbprojects.net/knowledge/ir/sirc.php

3. **IR Remote Libraries:**
   - Arduino IRremote: https://github.com/Arduino-IRremote/Arduino-IRremote
   - ESP32-IRremote: https://github.com/crankyoldgit/IRremoteESP8266

---

**Tác giả:** AI Assistant  
**Ngày:** 2024  
**Phiên bản:** 1.0


















