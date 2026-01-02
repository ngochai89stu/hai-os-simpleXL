# UI Screen Creation and Touch Verification

## Overview

This document describes the verification system that proves all 29 screens are created at runtime and that touch input is active and bindable.

## Verification Mode

### Enable/Disable

Verification mode is controlled by compile-time flag `SX_UI_VERIFY_MODE`:

- **Location**: `components/sx_ui/ui_verify/sx_ui_verify.h`
- **Default**: `1` (enabled)
- **To disable**: Set `#define SX_UI_VERIFY_MODE 0` in `sx_ui_verify.h`

### What It Does

1. **Creation Evidence**: Logs every screen's onCreate/onShow/onHide/onDestroy with timestamps
2. **Touch Proof**: Verifies pointer indev exists and logs touch events per screen
3. **Screen Walk Mode**: Auto-navigates through all screens and generates summary report

## How to Run

### Step 1: Build and Flash

```bash
idf.py build
idf.py -p COMxx flash monitor
```

### Step 2: Wait for Boot

- Device boots: BOOT → FLASH → HOME
- HOME screen shows launcher menu with all 29 screens
- Green "Start Verify" button appears at bottom (verification mode only)

### Step 3: Start Verification

**Option A: Manual Navigation**
- Tap any screen from launcher
- Watch logs for `[CREATE]`, `[SHOW]` messages
- Tap touch probe button (blue bar at bottom) to test touch

**Option B: Automated Screen Walk**
- Tap "Start Verify" button on HOME screen
- System automatically navigates through all screens
- Summary report printed at end

## Expected PASS Patterns

### Screen Creation Logs

```
I (1234) sx_ui_verify: [CREATE] id=3 name=Chat container=0x3ffb1234 root=0x3ffb5678 count=1
I (1235) sx_ui_verify: [SHOW] id=3 name=Chat count=1
```

**PASS Criteria**:
- `[CREATE]` log exists for screen
- `root` pointer is non-NULL
- `container` pointer is non-NULL
- `count` increments on each create

### Touch Indev Verification

```
I (567) sx_ui: [VERIFY] Found indev type=0 ptr=0x3ffb9abc
I (568) sx_ui: [VERIFY] ✓ Pointer indev found: 0x3ffb9abc
```

**PASS Criteria**:
- At least one indev of type `LV_INDEV_TYPE_POINTER` (0) exists
- No `ERROR: TOUCH_NOT_READY` message

### Touch Event Logs

```
I (2345) screen_common: [TOUCH] screen=3 clicked obj=0x3ffbdef0
I (2346) sx_ui_verify: [TOUCH] screen=3 name=Chat clicked count=1
```

**PASS Criteria**:
- `[TOUCH]` log appears when touch probe is tapped
- `touch_ok=true` in evidence table

### Summary Report

```
I (5000) sx_ui_verify: ========================================
I (5001) sx_ui_verify: UI VERIFICATION SUMMARY REPORT
I (5002) sx_ui_verify: ========================================
I (5003) sx_ui_verify: Screen  0: Boot                      | PASS | create=1 show=1 hide=0 destroy=0 | root=0x3ffb1234 | NO_TOUCH
I (5004) sx_ui_verify: Screen  3: Chat                      | PASS | create=1 show=1 hide=0 destroy=0 | root=0x3ffb5678 | TOUCH_OK
...
I (5030) sx_ui_verify: Summary: 29/29 screens created, 10 screens with touch events
```

**PASS Criteria**:
- All screens show `PASS` status
- `create_count >= 1` for all screens
- `root` pointer is non-NULL for all screens

## Common FAIL Causes

### root==NULL

**Symptom**:
```
I (1234) sx_ui_verify: [CREATE] id=5 name=Music Online container=0x3ffb1234 root=(nil) count=1
I (1235) sx_ui_verify: [CREATE] WARNING: Screen 5 (Music Online) created with NULL root!
```

**Cause**: Screen's `onCreate()` doesn't create a root object as child of container

**Fix**: Ensure screen creates root object:
```c
lv_obj_t *root = lv_obj_create(container);
lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
// ... create content under root ...
sx_ui_verify_on_create(SCREEN_ID_*, "Name", container, root);
```

### container==NULL

**Symptom**:
```
I (1234) sx_ui_verify: [CREATE] ERROR: Screen 3 (Chat) created with NULL container!
```

**Cause**: `ui_router_get_container()` returned NULL (router not initialized)

**Fix**: Ensure router is initialized before screen registration

### No Pointer Indev

**Symptom**:
```
E (567) sx_ui: [VERIFY] ERROR: TOUCH_NOT_READY - No pointer indev found!
```

**Cause**: Touch input device not registered or registration failed

**Fix**: Check touch initialization in `sx_ui_task.c` (lines 103-116)

### No Touch Events

**Symptom**: No `[TOUCH]` logs when tapping touch probe

**Causes**:
1. Touch probe not added to screen
2. Touch probe covered by other object
3. Touch indev not active

**Fix**: 
- Ensure `screen_common_add_touch_probe()` is called in screen's `onCreate()`
- Check touch probe is visible and not covered
- Verify touch indev exists (see above)

## Verification API

### Core Functions

```c
// Initialize verification system
void sx_ui_verify_init(void);

// Log screen lifecycle events
void sx_ui_verify_on_create(ui_screen_id_t id, const char *name, void *container, void *root);
void sx_ui_verify_on_show(ui_screen_id_t id);
void sx_ui_verify_on_hide(ui_screen_id_t id);
void sx_ui_verify_on_destroy(ui_screen_id_t id);

// Mark touch event received
void sx_ui_verify_mark_touch_ok(ui_screen_id_t id);

// Generate report
void sx_ui_verify_dump_report(void);
```

### Screen Walk Mode

```c
// Start automated screen walk
void sx_ui_verify_start_screen_walk(void);

// Check if walk is in progress
bool sx_ui_verify_is_walking(void);
```

### Touch Probe Helper

```c
// Add touch probe button to screen (verification mode only)
lv_obj_t* screen_common_add_touch_probe(lv_obj_t *parent, const char *label, ui_screen_id_t screen_id);
```

## Implementation Status

### Screens with Verification Added

- ✅ Boot (SCREEN_ID_BOOT)
- ✅ Flash (SCREEN_ID_FLASH)
- ✅ Home (SCREEN_ID_HOME) - with verify button
- ✅ Chat (SCREEN_ID_CHAT) - with touch probe
- ✅ Settings (SCREEN_ID_SETTINGS) - with touch probe
- ✅ Radio (SCREEN_ID_RADIO) - with touch probe

### Remaining Screens

All other screens (22 screens) need:
1. Include `sx_ui_verify.h`
2. Call `sx_ui_verify_on_create()` in `onCreate()`
3. Call `sx_ui_verify_on_show/hide/destroy()` in respective callbacks
4. Add touch probe (optional but recommended)

**Note**: This can be done incrementally. Verification system works even if not all screens are instrumented.

## Files Modified/Added

### New Files
- `components/sx_ui/ui_verify/sx_ui_verify.h`
- `components/sx_ui/ui_verify/sx_ui_verify.c`

### Modified Files
- `components/sx_ui/sx_ui_task.c` - Added verify init and touch indev check
- `components/sx_ui/screens/screen_common.c` - Added touch probe helper
- `components/sx_ui/screens/screen_common.h` - Added touch probe declaration
- `components/sx_ui/screens/screen_home.c` - Added verify button and instrumentation
- `components/sx_ui/screens/screen_boot.c` - Added instrumentation
- `components/sx_ui/screens/screen_flash.c` - Added instrumentation
- `components/sx_ui/screens/screen_chat.c` - Added instrumentation and touch probe
- `components/sx_ui/screens/screen_settings.c` - Added instrumentation and touch probe
- `components/sx_ui/screens/screen_radio.c` - Added instrumentation and touch probe
- `components/sx_ui/CMakeLists.txt` - Added verify source file

## Success Criteria

✅ **For each screen ID 0..29**: Evidence that `onCreate` executed at runtime and root LVGL object exists

✅ **Global evidence**: LVGL pointer indev exists

✅ **For each screen**: Touch probe exists and can log on click (at least for tested screens)

✅ **Summary report**: Printed in logs with PASS/FAIL per screen

## Next Steps

1. **Add instrumentation to remaining screens** (22 screens)
2. **Test on hardware** - Flash and run screen walk
3. **Verify all screens** - Check summary report
4. **Disable verify mode** - Set `SX_UI_VERIFY_MODE 0` for production builds

---

**Status**: ✅ Core verification system implemented, ready for testing






















