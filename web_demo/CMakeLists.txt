cmake_minimum_required(VERSION 3.13)
project(simplexl_web_demo C)

set(CMAKE_C_STANDARD 11)

# When building with Emscripten, pass -DEMSCRIPTEN=1 to cmake automatically by using emcmake

# -----------------------------------------------------------------------------
# Add LVGL (vendored copy or download via FetchContent)
# For now expect lvgl source present under web_demo/third_party/lvgl
add_subdirectory(third_party/lvgl lvgl_build)

# -----------------------------------------------------------------------------
# SimpleXL UI sources (limit to required screens to avoid heavy deps)
# TODO: prune sources further if build fails
file(GLOB SX_UI_CORE
    ../components/sx_ui/screens/screen_boot.c
    ../components/sx_ui/screens/screen_home*.c
    ../components/sx_ui/screens/screen_music_player*.c
    ../components/sx_ui/screens/screen_settings*.c
    ../components/sx_ui/screens/register_all_screens.c
    ../components/sx_ui/ui_helpers/*.c
    )

add_library(simplexl_ui STATIC ${SX_UI_CORE})

target_include_directories(simplexl_ui PUBLIC
    ../components/sx_ui
    ../components/sx_assets/generated
    ${CMAKE_SOURCE_DIR}/stubs
)

target_link_libraries(simplexl_ui PUBLIC lvgl)

# -----------------------------------------------------------------------------
# Executable that glues UI with LVGL port to Emscripten/SDL
add_executable(ui_demo
    main.c
)

target_include_directories(ui_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/stubs
)

target_link_libraries(ui_demo PRIVATE simplexl_ui lvgl)

# -----------------------------------------------------------------------------
# Emscripten-specific flags
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    set_target_properties(ui_demo PROPERTIES LINK_FLAGS "
        -s USE_SDL=2 \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s EXPORT_ES6=1 \
        -s MODULARIZE=1 \
        -s EXPORTED_FUNCTIONS=['_ui_show_boot','_ui_show_home','_ui_show_music','_ui_show_settings'] \
        -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] \
        --shell-file ${CMAKE_SOURCE_DIR}/shell_minimal.html
    ")
endif()
