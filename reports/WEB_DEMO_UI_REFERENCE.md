# Web Demo UI Reference

## Web Demo Screens (18 screens - Primary Reference)

Based on `web-demo/src/App.jsx` and analysis reports, here are the UI layouts:

### 1. HomeScreen
**Layout**: Main menu grid 2x3
- **Items**: 
  - Music Player
  - Online Music
  - Radio
  - SD Card
  - IR Control
  - Settings
  - Chatbot (may be separate or integrated)

**Touch**: Tap to navigate, swipe for scrolling

### 2. MusicPlayerScreen
**Layout**:
- Album art (large, centered)
- Track info (title, artist)
- Progress bar
- Controls: Play/Pause, Previous, Next
- Volume control

**Touch**: Tap controls, swipe progress bar

### 3. ChatScreen
**Layout**:
- Message list (scrollable)
- Input bar at bottom
- Send button

**Touch**: Tap input, tap send, scroll messages

### 4. RadioScreen
**Layout**:
- Station list (scrollable)
- Current station info
- Play/Pause control

**Touch**: Tap station to play, swipe list

### 5. MusicOnlineListScreen
**Layout**:
- Track list (scrollable)
- Search bar
- Playlists

**Touch**: Tap track to play, swipe list, tap search

### 6. SDCardMusicScreen
**Layout**:
- File browser (directory tree)
- File list
- Play button

**Touch**: Tap folder to open, tap file to play

### 7. IRControlScreen
**Layout**:
- Device list (AC, TV, etc.)
- Control buttons (power, temp, etc.)

**Touch**: Tap device, tap control buttons

### 8. SettingsScreen
**Layout**:
- Settings list (scrollable)
- Items: WiFi, Bluetooth, Display, Equalizer, About, OTA Update

**Touch**: Tap item to navigate

### 9. WifiSetupScreen
**Layout**:
- Network list (scrollable)
- Connect button
- Password input

**Touch**: Tap network, tap connect, input password

### 10. BluetoothSettingScreen
**Layout**:
- Device list (scrollable)
- Pair button
- Status

**Touch**: Tap device, tap pair

### 11. DisplaySettingScreen
**Layout**:
- Brightness slider
- Theme selector
- Timeout setting

**Touch**: Drag slider, tap selector

### 12. EqualizerScreen
**Layout**:
- Preset selector
- Band sliders (10 bands)
- Apply button

**Touch**: Drag sliders, tap preset

### 13. OtaScreen
**Layout**:
- Update status
- Progress bar
- Check/Update button

**Touch**: Tap check/update

### 14. AboutScreen
**Layout**:
- Device info
- System info
- Version

**Touch**: Scroll info

### 15. GoogleNavigationScreen
**Layout**:
- Turn-by-turn instructions
- Map preview
- Distance/time

**Touch**: Scroll instructions

### 16. WakewordFeedbackScreen
**Layout**:
- Visual feedback (animation)
- Status text

**Touch**: Double-tap trigger (system level)

### 17. BootScreen
**Layout**:
- Logo "H.A.I OS"
- Gradient background
- Startup animation

**Touch**: None (boot sequence)

### 18. FlashScreen
**Layout**:
- Welcome "SmartOS"
- Auto-navigate after 2s

**Touch**: None (splash)

## Touch Behavior Patterns

1. **Navigation**: Tap to navigate between screens
2. **Scrolling**: Swipe up/down for lists
3. **Controls**: Tap buttons, drag sliders
4. **Long-press**: Context menus (if applicable)
5. **Swipe gestures**: Back navigation (if applicable)

## Color Scheme

- Background: Dark theme (0x1a1a1a)
- Top bar: Dark gray (0x2a2a2a)
- Text: White (0xFFFFFF)
- Accent: Blue (0x5b7fff)
- Pressed: Darker shade

## Typography

- Title: Montserrat 16
- Body: Montserrat 14
- Small: Montserrat 12






















