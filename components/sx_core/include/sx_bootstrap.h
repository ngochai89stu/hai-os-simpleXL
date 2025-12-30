#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// Phase 0: minimal bootstrap entry.
esp_err_t sx_bootstrap_start(void);

#ifdef __cplusplus
}
#endif

