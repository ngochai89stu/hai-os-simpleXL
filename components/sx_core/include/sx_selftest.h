/**
 * @file sx_selftest.h
 * @brief SimpleXL Self-Test Module - Smoke Test cho phần cứng và nền tảng
 * 
 * Module này cung cấp smoke test tối thiểu để xác minh:
 * - Boot stability
 * - LVGL init và vẽ cơ bản
 * - Touch input
 * - SD card (nếu có)
 * - Audio pipeline (nếu có)
 * - Heap/PSRAM sanity
 * 
 * @note Module này tuân thủ kiến trúc SimpleXL, không phá vỡ flow app hiện tại
 */

#ifndef SX_SELFTEST_H
#define SX_SELFTEST_H

#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Kết quả smoke test
 */
typedef struct {
    bool boot_stable;          ///< Boot ổn định (không reset loop)
    bool lvgl_init_ok;         ///< LVGL init thành công
    bool screen_draw_ok;       ///< Vẽ screen cơ bản OK
    bool touch_detected;        ///< Touch được phát hiện
    bool touch_5_points_ok;     ///< Touch 5 điểm (4 góc + giữa) OK
    bool sd_mount_ok;           ///< SD mount thành công (nếu có)
    bool sd_list_ok;            ///< SD list 3 file OK (nếu có)
    bool audio_init_ok;         ///< Audio init OK (nếu có)
    bool audio_play_ok;         ///< Audio play 5-10s OK (nếu có)
    bool heap_sanity_ok;        ///< Heap sanity check OK
    bool psram_sanity_ok;       ///< PSRAM sanity check OK (nếu có)
    
    // Thông tin chi tiết
    uint32_t boot_count;        ///< Số lần boot (để test stability)
    uint32_t free_heap_boot;    ///< Free heap tại boot
    uint32_t free_heap_ui;      ///< Free heap sau khi vào UI
    uint32_t free_psram_boot;    ///< Free PSRAM tại boot (nếu có)
    uint32_t free_psram_ui;     ///< Free PSRAM sau khi vào UI (nếu có)
    
    // Lỗi (nếu có)
    char error_msg[128];        ///< Thông báo lỗi (nếu có)
} sx_selftest_result_t;

/**
 * @brief Khởi tạo smoke test module
 * 
 * @return ESP_OK nếu thành công
 */
esp_err_t sx_selftest_init(void);

/**
 * @brief Chạy smoke test đầy đủ
 * 
 * @param result Kết quả test (output)
 * @return ESP_OK nếu test chạy thành công (không có nghĩa là tất cả test đều PASS)
 */
esp_err_t sx_selftest_run(sx_selftest_result_t *result);

/**
 * @brief In kết quả test ra log
 * 
 * @param result Kết quả test
 */
void sx_selftest_print_result(const sx_selftest_result_t *result);

/**
 * @brief Kiểm tra boot stability (3 lần reset nguồn liên tiếp)
 * 
 * @note Test này cần chạy thủ công bằng cách reset nguồn 3 lần
 * @return true nếu boot ổn định
 */
bool sx_selftest_check_boot_stability(void);

/**
 * @brief Test LVGL init và vẽ screen cơ bản
 * 
 * @return true nếu PASS
 */
bool sx_selftest_test_lvgl(void);

/**
 * @brief Test touch: chạm 5 điểm (4 góc + giữa)
 * 
 * @return true nếu PASS (có phản hồi từ 5 điểm)
 */
bool sx_selftest_test_touch(void);

/**
 * @brief Test SD card: mount + list 3 file
 * 
 * @return true nếu PASS (nếu SD có)
 */
bool sx_selftest_test_sd(void);

/**
 * @brief Test audio: init pipeline + play 5-10 giây
 * 
 * @return true nếu PASS (nếu audio có)
 */
bool sx_selftest_test_audio(void);

/**
 * @brief Kiểm tra heap/PSRAM sanity
 * 
 * @param result Kết quả test (output)
 * @return true nếu PASS
 */
bool sx_selftest_check_heap_sanity(sx_selftest_result_t *result);

#ifdef __cplusplus
}
#endif

#endif // SX_SELFTEST_H


