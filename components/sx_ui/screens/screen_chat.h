#pragma once

#include "ui_screen_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// Register chat screen with the registry
void screen_chat_register(void);

// Public API to add messages to chat screen
void screen_chat_add_message(const char *role, const char *content);

#ifdef __cplusplus
}
#endif




