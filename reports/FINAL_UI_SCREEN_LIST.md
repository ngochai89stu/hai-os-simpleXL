# FINAL_UI_SCREEN_LIST (for hai-os-simplexl)

> This is the definitive list of screens to be implemented in the new SimpleXL architecture. It is the result of merging and filtering screens from the legacy ARCH9 firmware and the web-demo UX reference.

## 1. Core Product Screens (P0 - High Priority)

These screens are required to achieve UX parity with the web-demo and cover all essential user flows.

1.  **BootScreen**: System startup animation and status.
2.  **FlashScreen**: Optional, brief welcome splash after boot (matches web-demo flow).
3.  **HomeScreen**: Main menu/dashboard.
4.  **ChatScreen**: Conversational UI for the assistant.
5.  **WakewordFeedbackScreen**: Instant visual feedback for voice activation.
6.  **MusicOnlineListScreen**: Browse online music tracks.
7.  **MusicPlayerScreen**: Main media player UI.
8.  **RadioScreen**: Browse and play radio stations.
9.  **SDCardMusicScreen**: File browser for local music on SD card.
10. **IRControlScreen**: Remote control for devices like air conditioners.
11. **SettingsScreen**: Root menu for all settings.
12. **WifiSetupScreen**: Scan and connect to WiFi networks.
13. **BluetoothSettingScreen**: Pair and manage Bluetooth devices.
14. **DisplaySettingScreen**: Configure brightness, theme, timeout, etc.
15. **EqualizerScreen**: Adjust audio equalizer presets and bands.
16. **OtaScreen**: Handle Over-The-Air firmware updates.
17. **AboutScreen**: Display device and system information.
18. **GoogleNavigationScreen**: Display turn-by-turn navigation instructions.
19. **PermissionScreen**: Manage permissions for microphone, storage, etc. (Ported from ARCH9).
20. **ScreensaverScreen** & **ScreensaverSettingScreen**: Configure and display the screensaver (Ported from ARCH9).

## 2. Advanced Feature Screens (P1 - Medium Priority)

These screens from ARCH9 provide valuable, user-facing functionality that enhances the product but can be implemented after the core set.

1.  **AudioEffectsScreen**: Advanced audio settings like Bass, Treble, Reverb. (To be merged into `EqualizerScreen`).
2.  **StartupImagePickerScreen**: Allow user to customize the boot image.

## 3. Developer & Diagnostic Screens (P2 - Hidden/Debug Only)

These screens are essential for development, testing, and support but should not be visible in a standard user build. They should be accessible via a hidden "Developer Options" menu.

1.  **VoiceSettingsScreen**: Tune low-level audio processing (VAD, AEC, NS, Mic Gain).
2.  **NetworkDiagnosticScreen**: Detailed network status and testing tools.
3.  **SnapshotManagerScreen**: Manage system configuration snapshots.
4.  **DiagnosticsScreen**: View real-time system metrics (CPU, memory, etc.).
5.  **IntrospectionScreen**: Inspect live runtime state (service graphs, UI state diffs).
6.  **DevConsoleScreen**: On-device command console.
7.  **TouchDebugScreen**: A simple screen to test touch input and coordinates.

## 4. Deferred or Deprecated Screens

-   **StateSyncScreen**: Deferred. This feature is tightly coupled to a specific backend architecture and will only be considered if a similar cloud-sync feature is designed for the new repo.

