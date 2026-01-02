#pragma once

/**
 * @file sx_event_ranges.h
 * @brief Event ID range definitions and validation (P1.2)
 * 
 * This file provides range definitions and validation helpers for event taxonomy.
 * Section 4.1 of SIMPLEXL_ARCH v1.3 requires range reservation to avoid ID collisions.
 */

#include <stdint.h>
#include <stdbool.h>
#include "sx_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// P1.2: Event ID range validation helpers

/**
 * @brief Check if event ID is within valid range
 * 
 * @param event_id Event ID to validate
 * @return true if valid, false if out of range
 */
static inline bool sx_event_id_is_valid(sx_event_type_t event_id) {
    // Valid range: 0x0000 to 0x0FFF (current implementation)
    return (event_id >= SX_EVT_NONE && event_id < 0x1000);
}

/**
 * @brief Get domain base for an event ID
 * 
 * @param event_id Event ID
 * @return Domain base (0xNN00) or 0 if invalid
 */
static inline uint32_t sx_event_get_domain_base(sx_event_type_t event_id) {
    if (!sx_event_id_is_valid(event_id)) {
        return 0;
    }
    // Extract domain base (mask lower 8 bits)
    return (event_id & 0xFF00);
}

/**
 * @brief Check if event ID belongs to a specific domain
 * 
 * @param event_id Event ID
 * @param domain_base Domain base (e.g., SX_EVT_AUDIO_BASE)
 * @return true if event belongs to domain
 */
static inline bool sx_event_belongs_to_domain(sx_event_type_t event_id, uint32_t domain_base) {
    return (sx_event_get_domain_base(event_id) == domain_base);
}

#ifdef __cplusplus
}
#endif






