# ARCH9-Only Screens: Analysis and Porting Recommendations

> This report analyzes screens that exist in the legacy ARCH9 firmware UI but are absent from the `web-demo` UX reference. It provides recommendations on whether to port their functionality to the new `hai-os-simplexl` repo.

## 1. Summary of Findings

The ARCH9-only screens fall into three categories:
1.  **Advanced User Features**: Functionality that enhances the user experience but isn't part of the core UX defined by the webdemo (e.g., Screensaver, Audio Effects).
2.  **System & Privacy**: Features related to modern OS-level concerns like permissions.
3.  **Developer/Debug Tools**: A rich set of screens for diagnostics, debugging, and introspection.

## 2. Screen-by-Screen Analysis & Recommendation

| Screen | Feature / Purpose | Recommendation | Justification & Implementation Notes for SimpleXL |
|---|---|---|---|
| **Audio Effects** | Bass, Treble, Reverb controls. | **PORT (Low Priority)** | Combine with `EqualizerScreen` as an "Advanced" or "Effects" tab. Provides richer audio control for pro users. |
| **Voice Settings** | Configure VAD, NS, AEC, Mic Gain. | **PORT (as Hidden Dev Screen)** | Essential for audio pipeline tuning and debugging. Should be accessible via a developer options menu. |
| **Screensaver & Setting** | Configure and display a screensaver (Clock, Dim, Image). | **PORT (High Priority)** | This is a core UX feature for an always-on device to prevent burn-in and provide ambient information. |
| **Network Diagnostic** | Detailed WiFi info (RSSI, IP, MAC, etc.) and tests. | **PORT (as Hidden Dev Screen)** | Invaluable for customer support and debugging connectivity issues. Not a primary user-facing feature. |
| **Permission** | Manage permissions for Microphone, Network, Storage, Bluetooth. | **PORT (High Priority)** | A critical feature for user privacy and a sign of a mature OS. Should be part of the main `Settings` flow. |
| **Snapshot Manager** | Create, restore, and delete system configuration snapshots. | **PORT (as Hidden Dev Screen)** | Powerful developer/testing tool. Not a standard consumer feature. |
| **State Sync** | Monitor and trigger state synchronization with a backend. | **DEFER** | This feature is tightly coupled to a specific backend architecture. Re-evaluate only if a similar cloud-sync feature is planned for the new repo. |
| **Startup Image Picker** | Allow users to select a custom startup image. | **PORT (Low Priority)** | A good personalization feature. Can be an "Advanced" option within `Display Settings`. |
| **Diagnostics** | Real-time system metrics (CPU, Memory, WiFi, etc.). | **PORT (as Hidden Dev Screen)** | Essential for performance monitoring and debugging during development. |
| **Introspection** | Live runtime state view (service graph, UI state diffs). | **PORT (as Hidden Dev Screen)** | Advanced debugging tool for understanding system behavior in real-time. |
| **Dev Console** | On-device command console. | **PORT (as Hidden Dev Screen)** | Provides a powerful interface for developers to trigger actions and inspect state. |
| **Touch Debug** | Visualize touch coordinates and events. | **PORT (as Hidden Factory/Debug Screen)** | Necessary for hardware bring-up and factory testing. |

## 3. Conclusion for SimpleXL Architecture

- The **Core UX** will be defined by the webdemo screens + `Screensaver` + `Permission` screens.
- A significant number of ARCH9-only screens provide immense value for **development and diagnostics**. They should be ported but kept separate from the main user flow, accessible via a hidden "Developer Options" menu.
- Features like `State Sync` should be deferred to avoid pulling in unnecessary architectural complexity from the old repo.
