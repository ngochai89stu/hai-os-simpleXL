# ALL SCREENS MIGRATION PLAN

> **Ngày:** 2026-01-01  
> **Mục tiêu:** Migrate TẤT CẢ screens sang dùng design tokens và shared components

---

## ✅ SCREENS ĐÃ MIGRATE (9 screens)

1. ✅ screen_settings.c
2. ✅ screen_radio.c
3. ✅ screen_ota.c
4. ✅ screen_wifi_setup.c
5. ✅ screen_about.c
6. ✅ screen_display_setting.c
7. ✅ screen_home.c (vừa migrate)

**Screens mới đã tạo (3 screens):**
8. ✅ screen_ac_control.c
9. ✅ screen_system_info.c
10. ✅ screen_quick_settings.c

---

## 📋 SCREENS CẦN MIGRATE (22 screens)

### Core Product Screens (13 screens)
1. ⏳ screen_chat.c
2. ⏳ screen_wakeword_feedback.c
3. ⏳ screen_music_online_list.c
4. ⏳ screen_music_player.c
5. ⏳ screen_sd_card_music.c
6. ⏳ screen_ir_control.c
7. ⏳ screen_bluetooth_setting.c
8. ⏳ screen_equalizer.c
9. ⏳ screen_google_navigation.c
10. ⏳ screen_permission.c
11. ⏳ screen_screensaver.c
12. ⏳ screen_screensaver_setting.c
13. ⏳ screen_boot.c
14. ⏳ screen_flash.c

### Advanced Feature Screens (1 screen)
15. ⏳ screen_startup_image_picker.c

### Developer & Diagnostic Screens (7 screens)
16. ⏳ screen_voice_settings.c
17. ⏳ screen_network_diagnostic.c
18. ⏳ screen_snapshot_manager.c
19. ⏳ screen_diagnostics.c
20. ⏳ screen_introspection.c
21. ⏳ screen_dev_console.c
22. ⏳ screen_touch_debug.c

---

## 🎯 MIGRATION STRATEGY

### Phase 1: Core Product Screens (Priority 1)
Migrate các screens quan trọng nhất trước:
- screen_chat.c
- screen_music_player.c
- screen_bluetooth_setting.c
- screen_equalizer.c
- screen_google_navigation.c

### Phase 2: Remaining Core Screens (Priority 2)
- screen_wakeword_feedback.c
- screen_music_online_list.c
- screen_sd_card_music.c
- screen_ir_control.c
- screen_permission.c
- screen_screensaver.c
- screen_screensaver_setting.c
- screen_boot.c
- screen_flash.c

### Phase 3: Advanced & Debug Screens (Priority 3)
- screen_startup_image_picker.c
- screen_voice_settings.c
- screen_network_diagnostic.c
- screen_snapshot_manager.c
- screen_diagnostics.c
- screen_introspection.c
- screen_dev_console.c
- screen_touch_debug.c

---

## 📝 MIGRATION CHECKLIST

Cho mỗi screen, cần:
- [ ] Include `ui_theme_tokens.h`
- [ ] Replace `lv_color_hex(0x1a1a1a)` → `UI_COLOR_BG_PRIMARY`
- [ ] Replace `lv_color_hex(0x2a2a2a)` → `UI_COLOR_BG_SECONDARY`
- [ ] Replace `lv_color_hex(0x3a3a3a)` → `UI_COLOR_BG_PRESSED`
- [ ] Replace `lv_color_hex(0x5b7fff)` → `UI_COLOR_PRIMARY`
- [ ] Replace `lv_color_hex(0xFFFFFF)` → `UI_COLOR_TEXT_PRIMARY`
- [ ] Replace `lv_color_hex(0x888888)` → `UI_COLOR_TEXT_SECONDARY`
- [ ] Replace `&lv_font_montserrat_14` → `UI_FONT_MEDIUM`
- [ ] Replace hardcoded padding → `UI_SPACE_*` tokens
- [ ] Replace manual list creation → `ui_scrollable_list_create()` (nếu có list)
- [ ] Replace manual list items → `ui_list_item_two_line_create()` (nếu có list items)
- [ ] Replace manual sliders → `ui_gradient_slider_create()` (nếu có slider)
- [ ] Build test
- [ ] Fix errors/warnings

---

## 🚀 EXECUTION PLAN

1. **Batch 1:** Migrate 5 core screens (chat, music_player, bluetooth_setting, equalizer, google_navigation)
2. **Batch 2:** Migrate 9 remaining core screens
3. **Batch 3:** Migrate 8 advanced/debug screens
4. **Final:** Build test tất cả, fix errors, tạo báo cáo tổng kết

---

**Status:** Đang thực hiện Batch 1








