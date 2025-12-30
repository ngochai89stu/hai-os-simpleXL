# UI Direct Display Migration Notes

## Current Status

### Test Screen Location
- **File**: `components/sx_ui/screens/screen_home.c`
- **Content**: Displays "H.A.I OS" title and "Ready" status
- **File**: `components/sx_ui/screens/screen_flash.c`
- **Content**: Displays "Welcome" subtitle

### Boot Flow
- **File**: `components/sx_ui/sx_ui_task.c`
- **Sequence**: 
  1. `ui_router_init()` (line 97)
  2. `register_all_screens()` (line 100)
  3. `ui_router_navigate_to(SCREEN_ID_BOOT)` (line 119)
  4. Main loop: BOOT → FLASH (2s) → HOME (lines 156-174)

### Screen List/IDs
- **Registry**: `components/sx_ui/include/ui_screen_registry.h`
- **Total**: 29 screens (SCREEN_ID_BOOT to SCREEN_ID_TOUCH_DEBUG)
- **Registration**: `components/sx_ui/screens/register_all_screens.c`
- **All screens registered**: ✅

## What We Will Change

1. **Remove test content from screen_home.c**
   - Replace "H.A.I OS" + "Ready" with launcher menu
   - Show scrollable list/grid of all 29 screens
   - Tap item → navigate to that screen

2. **Update screen_flash.c**
   - Keep as splash screen but remove "Welcome" if not needed
   - Or keep it as is (it's just a flash screen)

3. **Ensure all 29 screens have real UI**
   - Each screen should have proper title + layout
   - No placeholder "test" text
   - Use router container pattern

4. **Add back button to all non-home screens**
   - Navigate back to HOME (SCREEN_ID_HOME)

5. **Verify navigation**
   - All screens navigable from launcher
   - Back button works
   - No test screen in boot flow














