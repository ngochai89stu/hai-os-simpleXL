# Components Map

## Internal Components (in `components/`)

- **sx_app**
  - **Purpose**: Main application entry point (`app_main`).
  - **Requires**: `sx_core`

- **sx_core**
  - **Purpose**: Core architecture (bootstrap, dispatcher, orchestrator, events, state).
  - **Requires**: `nvs_flash`, `esp_event`, `sx_platform`, `sx_ui`, `sx_assets`

- **sx_ui**
  - **Purpose**: UI owner task, LVGL integration, screen management, touch input.
  - **Requires**: `sx_core`, `sx_platform`, `esp_lvgl_port` (vendored), `lvgl/lvgl` (managed)

- **sx_platform**
  - **Purpose**: Hardware Abstraction Layer (display drivers, touch driver).
  - **Requires**: `driver`, `esp_lcd`, `espressif__esp_lcd_touch`, `espressif/esp_lcd_st7796` (managed), `espressif/esp_lcd_touch_ft5x06` (managed)
  - **Phase 3**: Touch driver implemented (FT5x06 via I2C)

- **sx_assets**
  - **Purpose**: SD RGB565 asset loader (load images from SD card).
  - **Requires**: `driver`, `fatfs`, `sdmmc`
  - **Phase 3**: API implemented (SD mount stub - pending SPI bus sharing)

- **sx_common**
  - **Purpose**: (Placeholder) Shared utilities, logging, types.
  - **Requires**: TBD

- **sx_services**
  - **Purpose**: (Placeholder) Business logic services (audio, wifi, etc.).
  - **Requires**: TBD

## Vendored Components

- **esp_lvgl_port**
  - **Status**: To be vendored.
  - **Source Repo**: `https://github.com/espressif/esp-bsp`
  - **Pinned Commit**: `d15cf39696339b128eceb75665d806d8bd10959a`

## Managed Components (via `idf_component.yml`)

- **lvgl/lvgl**: `~9.1.0` (in `sx_ui`)
- **espressif/esp_lcd_st7796**: `*` (in `sx_platform`)
- **espressif/esp_lcd_touch_ft5x06**: `^1.0.0` (in `sx_platform`)
