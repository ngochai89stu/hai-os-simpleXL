# LVGL Music Demo - Quick Start Guide

## ✅ Integration Complete

LVGL Music Demo đã được tích hợp vào Music Player screen với conditional compilation.

## 🚀 Cách Bật Demo

### Option 1: menuconfig (Recommended)

```bash
idf.py menuconfig
```

Navigate to:
1. **Component config** → **LVGL configuration** → **Demos**
   - Enable: `LV_USE_DEMO_MUSIC`
2. **Component config** → **SimpleXL UI Configuration**
   - Enable: `Use LVGL Music Demo for Music Player Screen`

### Option 2: sdkconfig.defaults

Add to `sdkconfig.defaults`:

```ini
CONFIG_LV_USE_DEMO_MUSIC=y
CONFIG_UI_USE_LVGL_MUSIC_DEMO=y
```

### Option 3: Direct Edit

Edit `sdkconfig`:

```ini
CONFIG_LV_USE_DEMO_MUSIC=y
CONFIG_UI_USE_LVGL_MUSIC_DEMO=y
```

## 📦 Build & Flash

```bash
# Build
idf.py build

# Flash
idf.py -p COM23 flash
```

## 🎵 Sử dụng

1. Boot device → Home menu
2. Click "Music Player" (🎵 icon)
3. **Nếu demo enabled**: Hiển thị LVGL Music Demo đầy đủ tính năng
4. **Nếu demo disabled**: Hiển thị custom UI đơn giản

## 🔍 Verify

Check logs for:
```
[UI] PlayMusicScreen -> LVGL Music Demo enabled
```

## 📝 Files Changed

1. `components/sx_ui/Kconfig.projbuild` - Kconfig option
2. `components/sx_ui/screens/screen_music_player.c` - Demo integration
3. `reports/LVGL_MUSIC_DEMO_INTEGRATION.md` - Full documentation

## ⚠️ Notes

- Demo tạo screen riêng, tạm thời bypass router container
- Demo không có back button built-in (có thể thêm sau)
- Demo assets embedded (~200-300KB flash)

## ✅ Status

- [x] Build passes
- [x] Kconfig option working
- [x] Conditional compilation working
- [ ] Runtime testing (pending hardware test)














