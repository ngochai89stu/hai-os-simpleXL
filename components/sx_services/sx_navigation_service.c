#include "sx_navigation_service.h"
#include "sx_tts_service.h"
#include "sx_wifi_service.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "cJSON.h"

static const char *TAG = "sx_navigation";

// Navigation service state
static bool s_initialized = false;
static sx_navigation_state_t s_state = SX_NAV_STATE_IDLE;
static sx_nav_route_t s_current_route = {0};
static sx_nav_coordinate_t s_current_position = {0};
static size_t s_current_waypoint_index = 0;
static SemaphoreHandle_t s_mutex = NULL;
static sx_nav_instruction_cb_t s_instruction_cb = NULL;
static sx_nav_state_cb_t s_state_cb = NULL;
static void *s_callback_user_data = NULL;

// External instruction data (from BLE or other sources)
static sx_nav_instruction_t s_external_instruction = {0};
static bool s_external_instruction_valid = false;

// Offline cache for routes and geocoding
#define MAX_CACHED_ROUTES 10
#define MAX_CACHED_GEOCODING 50
typedef struct {
    char key[128];  // "lat1,lon1-lat2,lon2" for routes, address for geocoding
    sx_nav_route_t route;
    bool route_valid;
    sx_nav_coordinate_t coord;
    bool coord_valid;
    uint32_t timestamp;  // Unix timestamp
} cached_data_t;

static cached_data_t s_route_cache[MAX_CACHED_ROUTES] = {0};
static size_t s_route_cache_count = 0;
static const uint32_t CACHE_EXPIRY_SECONDS = 86400; // 24 hours

// Forward declarations
static esp_err_t sx_nav_calculate_route_api(const sx_nav_coordinate_t *start,
                                           const sx_nav_coordinate_t *end,
                                           sx_nav_route_t *route);
static void sx_nav_generate_instruction(sx_nav_instruction_t *instruction,
                                        const sx_nav_waypoint_t *waypoint,
                                        uint32_t distance_to_next);

esp_err_t sx_navigation_service_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }
    
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }
    
    s_state = SX_NAV_STATE_IDLE;
    memset(&s_current_route, 0, sizeof(s_current_route));
    memset(&s_current_position, 0, sizeof(s_current_position));
    s_current_waypoint_index = 0;
    
    s_initialized = true;
    ESP_LOGI(TAG, "Navigation service initialized");
    return ESP_OK;
}

void sx_navigation_service_deinit(void) {
    if (!s_initialized) {
        return;
    }
    
    sx_navigation_stop();
    sx_navigation_free_route(&s_current_route);
    
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "Navigation service deinitialized");
}

esp_err_t sx_navigation_calculate_route(const sx_nav_coordinate_t *start,
                                       const sx_nav_coordinate_t *end,
                                       sx_nav_route_t *route) {
    if (!s_initialized || !start || !end || !route) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(route, 0, sizeof(*route));
    
    // Check cache first (offline support)
    char cache_key[128];
    snprintf(cache_key, sizeof(cache_key), "route:%.6f,%.6f-%.6f,%.6f",
             start->latitude, start->longitude, end->latitude, end->longitude);
    
    uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000000);
    bool found_in_cache = false;
    
    for (size_t i = 0; i < s_route_cache_count; i++) {
        if (strcmp(s_route_cache[i].key, cache_key) == 0 && s_route_cache[i].route_valid) {
            if (current_time - s_route_cache[i].timestamp < CACHE_EXPIRY_SECONDS) {
                // Copy cached route
                route->waypoint_count = s_route_cache[i].route.waypoint_count;
                route->waypoints = (sx_nav_waypoint_t *)malloc(s_route_cache[i].route.waypoint_count * sizeof(sx_nav_waypoint_t));
                if (route->waypoints != NULL) {
                    memcpy(route->waypoints, s_route_cache[i].route.waypoints,
                           s_route_cache[i].route.waypoint_count * sizeof(sx_nav_waypoint_t));
                    route->total_distance_m = s_route_cache[i].route.total_distance_m;
                    route->estimated_time_s = s_route_cache[i].route.estimated_time_s;
                    ESP_LOGI(TAG, "Route found in cache");
                    found_in_cache = true;
                    break;
                }
            }
        }
    }
    
    if (found_in_cache) {
        return ESP_OK;
    }
    
    // Try API-based routing first
    if (sx_wifi_is_connected()) {
        esp_err_t ret = sx_nav_calculate_route_api(start, end, route);
        if (ret == ESP_OK) {
            // Save to cache
            size_t cache_idx = s_route_cache_count;
            if (cache_idx >= MAX_CACHED_ROUTES) {
                // Find oldest entry
                cache_idx = 0;
                uint32_t oldest = s_route_cache[0].timestamp;
                for (size_t i = 1; i < MAX_CACHED_ROUTES; i++) {
                    if (s_route_cache[i].timestamp < oldest) {
                        oldest = s_route_cache[i].timestamp;
                        cache_idx = i;
                    }
                }
                sx_navigation_free_route(&s_route_cache[cache_idx].route);
            } else {
                s_route_cache_count++;
            }
            
            strncpy(s_route_cache[cache_idx].key, cache_key, sizeof(s_route_cache[cache_idx].key) - 1);
            s_route_cache[cache_idx].route.waypoint_count = route->waypoint_count;
            s_route_cache[cache_idx].route.waypoints = (sx_nav_waypoint_t *)malloc(route->waypoint_count * sizeof(sx_nav_waypoint_t));
            if (s_route_cache[cache_idx].route.waypoints != NULL) {
                memcpy(s_route_cache[cache_idx].route.waypoints, route->waypoints,
                       route->waypoint_count * sizeof(sx_nav_waypoint_t));
                s_route_cache[cache_idx].route.total_distance_m = route->total_distance_m;
                s_route_cache[cache_idx].route.estimated_time_s = route->estimated_time_s;
                s_route_cache[cache_idx].route_valid = true;
                s_route_cache[cache_idx].timestamp = current_time;
            }
            return ESP_OK;
        }
        ESP_LOGW(TAG, "API routing failed, using simple route");
    }
    
    // Fallback: Simple straight-line route
    route->waypoint_count = 2;
    route->waypoints = (sx_nav_waypoint_t *)malloc(2 * sizeof(sx_nav_waypoint_t));
    if (!route->waypoints) {
        return ESP_ERR_NO_MEM;
    }
    
    route->waypoints[0].coordinate = *start;
    strncpy(route->waypoints[0].name, "Start", sizeof(route->waypoints[0].name) - 1);
    route->waypoints[0].distance_m = 0;
    
    route->waypoints[1].coordinate = *end;
    strncpy(route->waypoints[1].name, "Destination", sizeof(route->waypoints[1].name) - 1);
    
    // Calculate distance (Haversine formula)
    double lat1 = start->latitude * M_PI / 180.0;
    double lat2 = end->latitude * M_PI / 180.0;
    double dlat = lat2 - lat1;
    double dlon = (end->longitude - start->longitude) * M_PI / 180.0;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    double distance_km = 6371.0 * c; // Earth radius in km
    route->waypoints[1].distance_m = (uint32_t)(distance_km * 1000);
    route->total_distance_m = route->waypoints[1].distance_m;
    route->estimated_time_s = route->total_distance_m / 10; // Assume 10 m/s average speed
    
    ESP_LOGI(TAG, "Route calculated: %.6f,%.6f -> %.6f,%.6f (%" PRIu32 " m)",
             start->latitude, start->longitude, end->latitude, end->longitude,
             route->total_distance_m);
    
    // Save simple route to cache too
    size_t cache_idx = s_route_cache_count;
    if (cache_idx >= MAX_CACHED_ROUTES) {
        cache_idx = 0;
        uint32_t oldest = s_route_cache[0].timestamp;
        for (size_t i = 1; i < MAX_CACHED_ROUTES; i++) {
            if (s_route_cache[i].timestamp < oldest) {
                oldest = s_route_cache[i].timestamp;
                cache_idx = i;
            }
        }
        sx_navigation_free_route(&s_route_cache[cache_idx].route);
    } else {
        s_route_cache_count++;
    }
    
    strncpy(s_route_cache[cache_idx].key, cache_key, sizeof(s_route_cache[cache_idx].key) - 1);
    s_route_cache[cache_idx].route.waypoint_count = route->waypoint_count;
    s_route_cache[cache_idx].route.waypoints = (sx_nav_waypoint_t *)malloc(route->waypoint_count * sizeof(sx_nav_waypoint_t));
    if (s_route_cache[cache_idx].route.waypoints != NULL) {
        memcpy(s_route_cache[cache_idx].route.waypoints, route->waypoints,
               route->waypoint_count * sizeof(sx_nav_waypoint_t));
        s_route_cache[cache_idx].route.total_distance_m = route->total_distance_m;
        s_route_cache[cache_idx].route.estimated_time_s = route->estimated_time_s;
        s_route_cache[cache_idx].route_valid = true;
        s_route_cache[cache_idx].timestamp = current_time;
    }
    
    return ESP_OK;
}

esp_err_t sx_navigation_start(const sx_nav_route_t *route) {
    if (!s_initialized || !route || route->waypoint_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Free old route
    sx_navigation_free_route(&s_current_route);
    
    // Copy route
    s_current_route.waypoint_count = route->waypoint_count;
    s_current_route.waypoints = (sx_nav_waypoint_t *)malloc(route->waypoint_count * sizeof(sx_nav_waypoint_t));
    if (!s_current_route.waypoints) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }
    memcpy(s_current_route.waypoints, route->waypoints, route->waypoint_count * sizeof(sx_nav_waypoint_t));
    s_current_route.total_distance_m = route->total_distance_m;
    s_current_route.estimated_time_s = route->estimated_time_s;
    
    s_current_waypoint_index = 0;
    s_state = SX_NAV_STATE_NAVIGATING;
    
    xSemaphoreGive(s_mutex);
    
    // Generate start instruction
    sx_nav_instruction_t instruction = {0};
    instruction.type = SX_NAV_INSTRUCTION_START;
    snprintf(instruction.text, sizeof(instruction.text), "Start navigation. Distance: %" PRIu32 " meters",
             s_current_route.total_distance_m);
    instruction.distance_m = s_current_route.total_distance_m;
    
    if (s_instruction_cb) {
        s_instruction_cb(&instruction, s_callback_user_data);
    }
    
    // Speak instruction via TTS
    sx_tts_speak_simple(instruction.text);
    
    if (s_state_cb) {
        s_state_cb(s_state, s_callback_user_data);
    }
    
    ESP_LOGI(TAG, "Navigation started");
    return ESP_OK;
}

esp_err_t sx_navigation_stop(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    sx_navigation_state_t old_state = s_state;
    s_state = SX_NAV_STATE_IDLE;
    s_current_waypoint_index = 0;
    
    xSemaphoreGive(s_mutex);
    
    if (s_state_cb && old_state != SX_NAV_STATE_IDLE) {
        s_state_cb(s_state, s_callback_user_data);
    }
    
    ESP_LOGI(TAG, "Navigation stopped");
    return ESP_OK;
}

esp_err_t sx_navigation_update_position(const sx_nav_coordinate_t *position) {
    if (!s_initialized || !position) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    s_current_position = *position;
    
    // Check if arrived at destination
    if (s_state == SX_NAV_STATE_NAVIGATING && s_current_waypoint_index < s_current_route.waypoint_count) {
        const sx_nav_waypoint_t *waypoint = &s_current_route.waypoints[s_current_waypoint_index];
        
        // Calculate distance to waypoint (simplified)
        double lat1 = position->latitude * M_PI / 180.0;
        double lat2 = waypoint->coordinate.latitude * M_PI / 180.0;
        double dlat = lat2 - lat1;
        double dlon = (waypoint->coordinate.longitude - position->longitude) * M_PI / 180.0;
        double a = sin(dlat / 2) * sin(dlat / 2) +
                   cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
        double c = 2 * atan2(sqrt(a), sqrt(1 - a));
        double distance_km = 6371.0 * c;
        uint32_t distance_m = (uint32_t)(distance_km * 1000);
        
        if (distance_m < 50) { // Within 50 meters
            s_current_waypoint_index++;
            if (s_current_waypoint_index >= s_current_route.waypoint_count) {
                s_state = SX_NAV_STATE_ARRIVED;
            }
        }
    }
    
    sx_navigation_state_t current_state = s_state;
    xSemaphoreGive(s_mutex);
    
    // Generate instruction if navigating
    if (current_state == SX_NAV_STATE_NAVIGATING) {
        sx_nav_instruction_t instruction = {0};
        if (sx_navigation_get_next_instruction(&instruction) == ESP_OK) {
            if (s_instruction_cb) {
                s_instruction_cb(&instruction, s_callback_user_data);
            }
            sx_tts_speak_simple(instruction.text);
        }
    } else if (current_state == SX_NAV_STATE_ARRIVED) {
        sx_nav_instruction_t instruction = {0};
        instruction.type = SX_NAV_INSTRUCTION_ARRIVE;
        strncpy(instruction.text, "You have arrived at your destination", sizeof(instruction.text) - 1);
        
        if (s_instruction_cb) {
            s_instruction_cb(&instruction, s_callback_user_data);
        }
        sx_tts_speak_simple(instruction.text);
        
        if (s_state_cb) {
            s_state_cb(current_state, s_callback_user_data);
        }
    }
    
    return ESP_OK;
}

sx_navigation_state_t sx_navigation_get_state(void) {
    if (!s_initialized) {
        return SX_NAV_STATE_IDLE;
    }
    
    sx_navigation_state_t state;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = s_state;
        xSemaphoreGive(s_mutex);
    } else {
        state = SX_NAV_STATE_ERROR;
    }
    
    return state;
}

esp_err_t sx_navigation_get_next_instruction(sx_nav_instruction_t *instruction) {
    if (!s_initialized || !instruction) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Priority 1: External instruction (from BLE, MCP, etc.)
    if (s_external_instruction_valid) {
        *instruction = s_external_instruction;
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }
    
    // Priority 2: Internal route instruction
    if (s_state != SX_NAV_STATE_NAVIGATING || s_current_waypoint_index >= s_current_route.waypoint_count) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    const sx_nav_waypoint_t *waypoint = &s_current_route.waypoints[s_current_waypoint_index];
    uint32_t distance_to_next = 0;
    
    if (s_current_waypoint_index + 1 < s_current_route.waypoint_count) {
        distance_to_next = s_current_route.waypoints[s_current_waypoint_index + 1].distance_m;
    }
    
    sx_nav_generate_instruction(instruction, waypoint, distance_to_next);
    
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

void sx_navigation_set_instruction_callback(sx_nav_instruction_cb_t callback, void *user_data) {
    s_instruction_cb = callback;
    s_callback_user_data = user_data;
}

void sx_navigation_set_state_callback(sx_nav_state_cb_t callback, void *user_data) {
    s_state_cb = callback;
    s_callback_user_data = user_data;
}

void sx_navigation_free_route(sx_nav_route_t *route) {
    if (route && route->waypoints) {
        free(route->waypoints);
        route->waypoints = NULL;
        route->waypoint_count = 0;
    }
}

esp_err_t sx_navigation_update_instruction(const sx_nav_instruction_t *instruction) {
    if (!s_initialized || !instruction) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    s_external_instruction = *instruction;
    s_external_instruction_valid = true;
    
    // Set state to navigating if not already
    if (s_state == SX_NAV_STATE_IDLE) {
        s_state = SX_NAV_STATE_NAVIGATING;
    }
    
    xSemaphoreGive(s_mutex);
    
    // Notify callbacks
    if (s_instruction_cb) {
        s_instruction_cb(instruction, s_callback_user_data);
    }
    
    if (s_state_cb) {
        s_state_cb(s_state, s_callback_user_data);
    }
    
    return ESP_OK;
}

// Google Maps Directions API key (set via settings or Kconfig)
static const char *s_google_maps_api_key = NULL;
static bool s_use_routing_api = false;

// Set Google Maps API key for routing (optional)
void sx_navigation_set_api_key(const char *api_key) {
    s_google_maps_api_key = api_key;
    s_use_routing_api = (api_key != NULL && strlen(api_key) > 0);
    ESP_LOGI(TAG, "Google Maps Directions API key %s", s_use_routing_api ? "set" : "cleared");
}

static esp_err_t sx_nav_calculate_route_api(const sx_nav_coordinate_t *start,
                                           const sx_nav_coordinate_t *end,
                                           sx_nav_route_t *route) {
    if (!s_use_routing_api || !s_google_maps_api_key) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Build API URL
    char url[512];
    snprintf(url, sizeof(url),
             "https://maps.googleapis.com/maps/api/directions/json?"
             "origin=%.6f,%.6f&destination=%.6f,%.6f&key=%s",
             start->latitude, start->longitude,
             end->latitude, end->longitude,
             s_google_maps_api_key);
    
    // Initialize HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 15000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    // Perform request
    esp_err_t ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        return ret;
    }
    
    // Check status code
    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "Directions API failed: HTTP %d", status_code);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    // Read response
    int content_length = esp_http_client_get_content_length(client);
    if (content_length <= 0 || content_length > 8192) {
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_SIZE;
    }
    
    char *response = (char *)malloc(content_length + 1);
    if (response == NULL) {
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    int read_len = esp_http_client_read_response(client, response, content_length);
    response[read_len] = '\0';
    
    // Parse JSON response
    cJSON *json = cJSON_Parse(response);
    if (json == NULL) {
        free(response);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Extract route from response
    cJSON *routes = cJSON_GetObjectItem(json, "routes");
    if (routes != NULL && cJSON_IsArray(routes) && cJSON_GetArraySize(routes) > 0) {
        cJSON *first_route = cJSON_GetArrayItem(routes, 0);
        cJSON *legs = cJSON_GetObjectItem(first_route, "legs");
        
        if (legs != NULL && cJSON_IsArray(legs) && cJSON_GetArraySize(legs) > 0) {
            cJSON *first_leg = cJSON_GetArrayItem(legs, 0);
            cJSON *distance = cJSON_GetObjectItem(first_leg, "distance");
            cJSON *duration = cJSON_GetObjectItem(first_leg, "duration");
            cJSON *steps = cJSON_GetObjectItem(first_leg, "steps");
            
            // Get total distance and duration
            if (distance != NULL) {
                cJSON *distance_value = cJSON_GetObjectItem(distance, "value");
                if (distance_value != NULL && cJSON_IsNumber(distance_value)) {
                    route->total_distance_m = (uint32_t)distance_value->valueint;
                }
            }
            
            if (duration != NULL) {
                cJSON *duration_value = cJSON_GetObjectItem(duration, "value");
                if (duration_value != NULL && cJSON_IsNumber(duration_value)) {
                    route->estimated_time_s = (uint32_t)duration_value->valueint;
                }
            }
            
            // Extract waypoints from steps
            if (steps != NULL && cJSON_IsArray(steps)) {
                int step_count = cJSON_GetArraySize(steps);
                route->waypoint_count = step_count + 2; // Start + steps + end
                route->waypoints = (sx_nav_waypoint_t *)malloc(route->waypoint_count * sizeof(sx_nav_waypoint_t));
                
                if (route->waypoints != NULL) {
                    // Start waypoint
                    route->waypoints[0].coordinate = *start;
                    strncpy(route->waypoints[0].name, "Start", sizeof(route->waypoints[0].name) - 1);
                    route->waypoints[0].distance_m = 0;
                    
                    // Step waypoints
                    uint32_t cumulative_distance = 0;
                    for (int i = 0; i < step_count && i < (int)route->waypoint_count - 2; i++) {
                        cJSON *step = cJSON_GetArrayItem(steps, i);
                        cJSON *end_location = cJSON_GetObjectItem(step, "end_location");
                        cJSON *step_distance = cJSON_GetObjectItem(step, "distance");
                        
                        if (end_location != NULL) {
                            cJSON *lat = cJSON_GetObjectItem(end_location, "lat");
                            cJSON *lng = cJSON_GetObjectItem(end_location, "lng");
                            if (lat != NULL && lng != NULL && cJSON_IsNumber(lat) && cJSON_IsNumber(lng)) {
                                route->waypoints[i + 1].coordinate.latitude = lat->valuedouble;
                                route->waypoints[i + 1].coordinate.longitude = lng->valuedouble;
                            }
                        }
                        
                        if (step_distance != NULL) {
                            cJSON *step_distance_value = cJSON_GetObjectItem(step_distance, "value");
                            if (step_distance_value != NULL && cJSON_IsNumber(step_distance_value)) {
                                cumulative_distance += (uint32_t)step_distance_value->valueint;
                                route->waypoints[i + 1].distance_m = cumulative_distance;
                            }
                        }
                        
                        snprintf(route->waypoints[i + 1].name, sizeof(route->waypoints[i + 1].name), 
                                "Step %d", i + 1);
                    }
                    
                    // End waypoint
                    route->waypoints[route->waypoint_count - 1].coordinate = *end;
                    strncpy(route->waypoints[route->waypoint_count - 1].name, "Destination", 
                           sizeof(route->waypoints[route->waypoint_count - 1].name) - 1);
                    route->waypoints[route->waypoint_count - 1].distance_m = route->total_distance_m;
                    
                    ESP_LOGI(TAG, "Route calculated via API: %" PRIu32 " waypoints, %" PRIu32 " m, %" PRIu32 " s",
                             route->waypoint_count, route->total_distance_m, route->estimated_time_s);
                    
                    cJSON_Delete(json);
                    free(response);
                    esp_http_client_cleanup(client);
                    return ESP_OK;
                }
            }
        }
    }
    
    cJSON_Delete(json);
    free(response);
    esp_http_client_cleanup(client);
    return ESP_ERR_NOT_FOUND;
}

static void sx_nav_generate_instruction(sx_nav_instruction_t *instruction,
                                        const sx_nav_waypoint_t *waypoint,
                                        uint32_t distance_to_next) {
    memset(instruction, 0, sizeof(*instruction));
    
    if (distance_to_next < 100) {
        instruction->type = SX_NAV_INSTRUCTION_ARRIVE;
        snprintf(instruction->text, sizeof(instruction->text), "Arrive at %s", waypoint->name);
    } else if (distance_to_next < 500) {
        instruction->type = SX_NAV_INSTRUCTION_GO_STRAIGHT;
        snprintf(instruction->text, sizeof(instruction->text), "Continue straight for %" PRIu32 " meters", distance_to_next);
    } else {
        instruction->type = SX_NAV_INSTRUCTION_GO_STRAIGHT;
        snprintf(instruction->text, sizeof(instruction->text), "Continue straight for %.1f kilometers",
                 distance_to_next / 1000.0f);
    }
    
    instruction->distance_m = distance_to_next;
    instruction->time_s = distance_to_next / 10; // Assume 10 m/s
}


