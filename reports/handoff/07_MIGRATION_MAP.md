# Migration Map (Old → New)

> Rule: old repo (`D:\NEWESP32\xiaozhi-esp32`) is **REFERENCE-ONLY**. Do not copy wholesale. Port by rewriting clean modules.

## Legend
- **KEEP**: Can be ported mostly as-is (small wrappers ok)
- **REWRITE**: Re-implement cleanly (no legacy dependencies)
- **REFERENCE-ONLY**: Read/compare only
- **DROP**: Not needed / do not bring over

## Mapping Table (initial)

| Old path / module (xiaozhi-esp32) | Classification | New target component/path (hai-os-simplexl) | Notes |
|---|---:|---|---|
| `display_test_standalone/` | REFERENCE-ONLY | `components/sx_platform/` | Use for pinout + init sequences (ST7796) only |
| `main/` app entry | REFERENCE-ONLY | `app/app_main.c` + `sx_core/sx_bootstrap.c` | New repo keeps bootstrap-only app_main |
| `display/ui_system/**` | DROP | (none) | **Forbidden** legacy UI |
| `components/*` (audio/wifi/ir/etc.) | REFERENCE-ONLY → REWRITE | `components/sx_services/*` | Port service logic via events/state |
| `assets/` or image pipelines | REFERENCE-ONLY | `components/sx_assets/` | Prefer RGB565 bins from SD (no PNG runtime) |

## Next expansions
- Add per-service rows after Phase 1 contracts are frozen.

























