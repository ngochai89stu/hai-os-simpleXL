# UI Verification Plan

## Recon Results

### UI Framework Entry Points

#### Screen Registry
- **File**: `components/sx_ui/ui_screen_registry.c`
- **Function**: `ui_screen_registry_init()` - Initializes screen registry
- **Function**: `ui_screen_registry_register()` - Registers screen callbacks
- **Location**: Called from `sx_ui_task.c` line 97 (via `ui_router_init()`)

#### Screen Registration
- **File**: `components/sx_ui/screens/register_all_screens.c`
- **Function**: `register_all_screens()` - Registers all 29 screens
- **Location**: Called from `sx_ui_task.c` line 100

#### Router
- **File**: `components/sx_ui/ui_router.c`
- **Function**: `ui_router_init()` - Initializes router and screen container
- **Function**: `ui_router_navigate_to()` - Navigates to screen by ID
- **Location**: Called from `sx_ui_task.c` line 97

#### Screen Lifecycle
- **Pattern**: Each screen implements callbacks:
  - `on_create()` - Called when screen is created
  - `on_show()` - Called when screen becomes visible
  - `on_hide()` - Called when screen is hidden
  - `on_destroy()` - Called when screen is destroyed
- **Location**: Defined in `ui_screen_registry.h` as `ui_screen_callbacks_t`
- **Registration**: Each screen calls `screen_*_register()` which registers callbacks

#### Touch Input Device
- **File**: `components/sx_ui/sx_ui_task.c`
- **Function**: `lvgl_port_add_touch()` - Adds touch input device
- **Location**: Lines 103-116
- **Returns**: `lv_indev_t*` pointer to touch indev

### Screen Structure Pattern

Each screen file (`screen_*.c`) follows this pattern:
1. Static variables for UI objects
2. `on_create()` - Creates LVGL objects under container
3. `on_show()` - Updates UI from state
4. `on_hide()` - Stops animations/timers
5. `on_destroy()` - Cleans up objects
6. `screen_*_register()` - Registers callbacks

### Container Pattern

- **Container**: `ui_router_get_container()` returns the screen container
- **Usage**: All screen content is created as children of this container
- **Location**: `ui_router.c` line 116

## Verification Strategy

### Step 1: Creation Evidence
- Add tracer calls in each screen's `on_create()` to log:
  - screen_id, screen_name, container ptr, root ptr
- Store evidence in static table
- Track create_count per screen

### Step 2: Touch Proof
- Verify pointer indev exists after touch init
- Add touch probe button to each screen
- Log touch events per screen

### Step 3: Screen Walk Mode
- Auto-navigate through all screens
- Verify onCreate/onShow fires for each
- Generate summary report

### Step 4: Reports
- Document verification mode usage
- Expected PASS/FAIL patterns
- Troubleshooting guide







