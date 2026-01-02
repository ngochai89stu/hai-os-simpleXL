# Web Demo UI Implementation Plan

## Status: IN PROGRESS

### Goal
Update all 29 screens to match web demo UI layouts and touch behavior.

## Completed Screens

### ✅ HomeScreen
- **Status**: Updated to grid 2x3 layout
- **Items**: Music Player, Online Music, Radio, SD Card, IR Control, Settings, Chatbot
- **Touch**: Tap to navigate

### ✅ MusicPlayerScreen  
- **Status**: Updated with full UI
- **Layout**: Album art, track info, progress bar, controls (Play/Pause/Prev/Next), volume slider
- **Touch**: Tap controls, drag sliders

### ✅ ChatScreen
- **Status**: Already matches web demo
- **Layout**: Message list, input bar, send button
- **Touch**: Tap input, tap send, scroll messages

### ✅ SettingsScreen
- **Status**: Already matches web demo
- **Layout**: Settings list with navigation
- **Touch**: Tap item to navigate

## Remaining Screens (25/29)

### P0 - Core Product Screens (14 remaining)

1. **RadioScreen** - Need: Station list, current station info, Play/Pause control
2. **MusicOnlineListScreen** - Need: Track list, search bar, playlists
3. **SDCardMusicScreen** - Need: File browser, file list, play button
4. **IRControlScreen** - Need: Device list, control buttons
5. **WifiSetupScreen** - Need: Network list, connect button, password input
6. **BluetoothSettingScreen** - Need: Device list, pair button, status
7. **DisplaySettingScreen** - Need: Brightness slider, theme selector, timeout
8. **EqualizerScreen** - Need: Preset selector, band sliders (10 bands), apply button
9. **OtaScreen** - Need: Update status, progress bar, check/update button
10. **AboutScreen** - Need: Device info, system info, version
11. **GoogleNavigationScreen** - Need: Turn-by-turn instructions, map preview, distance/time
12. **WakewordFeedbackScreen** - Need: Visual feedback animation, status text
13. **BootScreen** - Need: Logo "H.A.I OS", gradient background, startup animation
14. **FlashScreen** - Already has pulse animation, matches web demo

### P1 - Advanced Feature Screens (2)

15. **AudioEffectsScreen** - Need: Bass/Treble/Reverb controls
16. **StartupImagePickerScreen** - Need: Image picker UI

### P2 - Developer Screens (7)

17. **PermissionScreen** - Need: Permission list, toggle switches
18. **ScreensaverScreen** - Need: Screensaver display
19. **ScreensaverSettingScreen** - Need: Screensaver config
20. **VoiceSettingsScreen** - Need: VAD/AEC/NS/Mic Gain controls
21. **NetworkDiagnosticScreen** - Need: Network status, testing tools
22. **SnapshotManagerScreen** - Need: Snapshot list, save/load buttons
23. **DiagnosticsScreen** - Need: Real-time metrics display
24. **IntrospectionScreen** - Need: Runtime state inspection
25. **DevConsoleScreen** - Need: Command console UI
26. **TouchDebugScreen** - Need: Touch coordinate display

## Implementation Strategy

### Phase 1: Core P0 Screens (Priority)
1. RadioScreen - Station list UI
2. MusicOnlineListScreen - Track list with search
3. SDCardMusicScreen - File browser
4. IRControlScreen - Device controls
5. WifiSetupScreen - Network list
6. DisplaySettingScreen - Sliders and selectors
7. EqualizerScreen - Band sliders

### Phase 2: Remaining P0 Screens
8. BluetoothSettingScreen
9. OtaScreen
10. AboutScreen
11. GoogleNavigationScreen
12. WakewordFeedbackScreen
13. BootScreen (gradient background)

### Phase 3: P1 and P2 Screens
- Implement remaining advanced and developer screens

## Touch Behavior Requirements

All screens must support:
1. **Tap**: Navigate, select, activate
2. **Swipe**: Scroll lists, navigate back
3. **Drag**: Sliders, progress bars
4. **Long-press**: Context menus (if applicable)

## Color Scheme (Consistent)

- Background: `0x1a1a1a` (dark)
- Top bar: `0x2a2a2a` (dark gray)
- Text: `0xFFFFFF` (white)
- Accent: `0x5b7fff` (blue)
- Pressed: `0x3a3a3a` (darker gray)
- Secondary text: `0x888888` (gray)

## Typography (Consistent)

- Title: Montserrat 16
- Body: Montserrat 14
- Small: Montserrat 12
- Large: Montserrat 20/24 (for icons/album art)

## Next Steps

1. Continue updating P0 screens one by one
2. Test each screen on hardware
3. Verify touch behavior matches web demo
4. Ensure consistent styling across all screens






















