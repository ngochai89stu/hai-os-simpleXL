#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Navigation Service
// Route management, voice guidance, and turn-by-turn directions

// Navigation state
typedef enum {
    SX_NAV_STATE_IDLE = 0,
    SX_NAV_STATE_ROUTING,
    SX_NAV_STATE_NAVIGATING,
    SX_NAV_STATE_ARRIVED,
    SX_NAV_STATE_ERROR,
} sx_navigation_state_t;

// Navigation instruction type
typedef enum {
    SX_NAV_INSTRUCTION_NONE = 0,
    SX_NAV_INSTRUCTION_START,
    SX_NAV_INSTRUCTION_TURN_LEFT,
    SX_NAV_INSTRUCTION_TURN_RIGHT,
    SX_NAV_INSTRUCTION_GO_STRAIGHT,
    SX_NAV_INSTRUCTION_UTURN,
    SX_NAV_INSTRUCTION_ARRIVE,
} sx_navigation_instruction_t;

// GPS coordinate
typedef struct {
    double latitude;
    double longitude;
} sx_nav_coordinate_t;

// Navigation waypoint
typedef struct {
    sx_nav_coordinate_t coordinate;
    char name[64];
    uint32_t distance_m;  // Distance from previous waypoint in meters
} sx_nav_waypoint_t;

// Navigation route
typedef struct {
    sx_nav_waypoint_t *waypoints;
    size_t waypoint_count;
    uint32_t total_distance_m;
    uint32_t estimated_time_s;
} sx_nav_route_t;

// Navigation instruction
typedef struct {
    sx_navigation_instruction_t type;
    char text[128];
    uint32_t distance_m;      // Distance to next turn
    uint32_t time_s;          // Time to next turn (ETA)
    char icon_hash[16];       // Icon hash for turn-by-turn icon (from BLE)
    int speed_kmh;            // Current speed from GPS (from BLE, -1 if not available)
    char next_road[128];      // Next road name (from BLE)
    char next_road_desc[128]; // Next road description (from BLE, e.g., "Rẽ trái", "Rẽ phải")
    char total_dist[64];     // Total distance remaining (from BLE, e.g., "5.2 km")
    char ete[64];             // Estimated Time En route (from BLE, e.g., "15 min")
} sx_nav_instruction_t;

// Navigation callback
typedef void (*sx_nav_instruction_cb_t)(const sx_nav_instruction_t *instruction, void *user_data);
typedef void (*sx_nav_state_cb_t)(sx_navigation_state_t state, void *user_data);

// Initialize navigation service
// Returns: ESP_OK on success
esp_err_t sx_navigation_service_init(void);

// Deinitialize navigation service
void sx_navigation_service_deinit(void);

// Calculate route
// start: Start coordinate
// end: End coordinate
// route: Output route
// Returns: ESP_OK on success
esp_err_t sx_navigation_calculate_route(const sx_nav_coordinate_t *start,
                                       const sx_nav_coordinate_t *end,
                                       sx_nav_route_t *route);

// Start navigation
// route: Route to navigate
// Returns: ESP_OK on success
esp_err_t sx_navigation_start(const sx_nav_route_t *route);

// Stop navigation
// Returns: ESP_OK on success
esp_err_t sx_navigation_stop(void);

// Update current position
// position: Current GPS position
// Returns: ESP_OK on success
esp_err_t sx_navigation_update_position(const sx_nav_coordinate_t *position);

// Get current navigation state
sx_navigation_state_t sx_navigation_get_state(void);

// Get next instruction
// instruction: Output instruction
// Returns: ESP_OK on success
esp_err_t sx_navigation_get_next_instruction(sx_nav_instruction_t *instruction);

// Set instruction callback
void sx_navigation_set_instruction_callback(sx_nav_instruction_cb_t callback, void *user_data);

// Set state callback
void sx_navigation_set_state_callback(sx_nav_state_cb_t callback, void *user_data);

// Free route memory
void sx_navigation_free_route(sx_nav_route_t *route);

// Update instruction from external source (BLE, MCP, etc.)
// This allows external navigation data to override internal route
esp_err_t sx_navigation_update_instruction(const sx_nav_instruction_t *instruction);

// Set Google Maps Directions API key (optional, enables API routing)
// If set, route calculation will try API first, then fall back to simple route
void sx_navigation_set_api_key(const char *api_key);

#ifdef __cplusplus
}
#endif


