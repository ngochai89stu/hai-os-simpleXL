#include "screen_chat.h"

#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include "sx_lvgl.h"  // LVGL wrapper (Section 7.5 SIMPLEXL_ARCH v1.3)
#include "ui_router.h"
#include "sx_ui_verify.h"
#include "screen_common.h"
#include "sx_dispatcher.h"
#include "sx_state.h"
#include "sx_event.h"
#include "sx_stt_service.h"
#include "sx_tts_service.h"
#include "ui_theme_tokens.h"
#include "ui_list.h"

static const char *TAG = "screen_chat";

static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_message_list = NULL;
static lv_obj_t *s_input_bar = NULL;
static lv_obj_t *s_textarea = NULL;
static lv_obj_t *s_send_btn = NULL;
static lv_obj_t *s_container = NULL;
static lv_obj_t *s_status_label = NULL;  // Connection status indicator
static lv_obj_t *s_typing_indicator = NULL;  // Typing indicator
static lv_obj_t *s_stt_status_label = NULL;  // STT status label
static lv_obj_t *s_tts_status_label = NULL;  // TTS status label
static lv_obj_t *s_emotion_label = NULL;     // Emotion indicator label
static uint32_t s_last_state_seq = 0;  // Track state sequence to detect changes
static bool s_chatbot_connected = false;
static bool s_tts_speaking = false;
static bool s_stt_active = false;  // STT active state

// Forward declarations
static void add_message_to_list(const char *role, const char *content);

static void send_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (s_textarea == NULL) {
            return;
        }
        
        const char *text = lv_textarea_get_text(s_textarea);
        if (text == NULL || strlen(text) == 0) {
            return;
        }
        
        ESP_LOGI(TAG, "Send button clicked: %s", text);
        
        // Phase 4: Copy message text (event ptr must remain valid)
        char *message_copy = strdup(text);
        if (message_copy == NULL) {
            ESP_LOGE(TAG, "Failed to allocate message copy");
            return;
        }
        
        // Add user message to UI immediately (optimistic update)
        add_message_to_list("user", text);
        
        // Dispatch SX_EVT_UI_INPUT event with chat message
        sx_event_t evt = {
            .type = SX_EVT_UI_INPUT,
            .arg0 = 0,
            .arg1 = 0,
            .ptr = message_copy, // Orchestrator will free after processing
        };
        sx_dispatcher_post_event(&evt);
        
        // Clear textarea
        lv_textarea_set_text(s_textarea, "");
    }
}

static void on_create(void) {
    ESP_LOGI(TAG, "Chat screen onCreate");
    
    lv_obj_t *container = ui_router_get_container();
    if (container == NULL) {
        ESP_LOGE(TAG, "Screen container is NULL");
        return;
    }
    
    s_container = container;
    
    // Set background using token
    lv_obj_set_style_bg_color(container, UI_COLOR_BG_PRIMARY, LV_PART_MAIN);
    
    // Create top bar with back button
    s_top_bar = screen_common_create_top_bar_with_back(container, "Chat");
    
    // Create message list (scrollable) using shared component
    s_message_list = ui_scrollable_list_create(container);
    lv_obj_set_size(s_message_list, LV_PCT(100), LV_PCT(100) - 100); // Leave space for top bar and input
    lv_obj_align(s_message_list, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_set_flex_flow(s_message_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_message_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Add welcome message
    lv_obj_t *welcome_msg = lv_label_create(s_message_list);
    lv_label_set_text(welcome_msg, "Start a conversation with the chatbot...");
    lv_obj_set_style_text_font(welcome_msg, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(welcome_msg, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_pad_all(welcome_msg, UI_SPACE_XL, LV_PART_MAIN);
    
    // Create status label in top bar area (below title)
    s_status_label = lv_label_create(container);
    lv_label_set_text(s_status_label, "● Disconnected");
    lv_obj_set_style_text_font(s_status_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_status_label, UI_COLOR_TEXT_ERROR, 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_RIGHT, -10, 25);
    
    // Create STT/TTS status labels (below status label)
    s_stt_status_label = lv_label_create(container);
    lv_label_set_text(s_stt_status_label, "STT: Ready");
    lv_obj_set_style_text_font(s_stt_status_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_stt_status_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_stt_status_label, LV_ALIGN_TOP_LEFT, 10, 50);
    
    s_tts_status_label = lv_label_create(container);
    lv_label_set_text(s_tts_status_label, "TTS: Ready");
    lv_obj_set_style_text_font(s_tts_status_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_tts_status_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_tts_status_label, LV_ALIGN_TOP_LEFT, 10, 68);

    // Emotion indicator label (emoji/text based on chatbot emotion)
    s_emotion_label = lv_label_create(container);
    lv_label_set_text(s_emotion_label, "Emotion: 🙂");
    lv_obj_set_style_text_font(s_emotion_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_emotion_label, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(s_emotion_label, LV_ALIGN_TOP_LEFT, 10, 86);
    
    // Create typing indicator (hidden by default)
    s_typing_indicator = lv_label_create(s_message_list);
    lv_label_set_text(s_typing_indicator, "🤖 Typing...");
    lv_obj_set_style_text_font(s_typing_indicator, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(s_typing_indicator, UI_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_pad_all(s_typing_indicator, UI_SPACE_XL, LV_PART_MAIN);
    lv_obj_add_flag(s_typing_indicator, LV_OBJ_FLAG_HIDDEN);
    
    // Create input bar at bottom
    s_input_bar = lv_obj_create(container);
    lv_obj_set_size(s_input_bar, LV_PCT(100), 60);
    lv_obj_align(s_input_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_input_bar, UI_COLOR_BG_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_input_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_input_bar, UI_SPACE_SM, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_input_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_input_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Create textarea for input
    s_textarea = lv_textarea_create(s_input_bar);
    lv_obj_set_flex_grow(s_textarea, 1);
    lv_obj_set_size(s_textarea, LV_PCT(80), LV_PCT(100));
    lv_textarea_set_placeholder_text(s_textarea, "Type a message...");
    lv_obj_set_style_text_font(s_textarea, UI_FONT_MEDIUM, 0);
    
    // Create send button
    s_send_btn = lv_btn_create(s_input_bar);
    lv_obj_set_size(s_send_btn, 50, LV_PCT(100));
    lv_obj_t *send_label = lv_label_create(s_send_btn);
    lv_label_set_text(send_label, "Send");
    lv_obj_set_style_text_font(send_label, UI_FONT_MEDIUM, 0);
    lv_obj_center(send_label);
    lv_obj_add_event_cb(s_send_btn, send_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    #if SX_UI_VERIFY_MODE
    // Add touch probe for verification
    screen_common_add_touch_probe(container, "Chat Screen", SCREEN_ID_CHAT);
    // Verification: Log screen creation
    sx_ui_verify_on_create(SCREEN_ID_CHAT, "Chat", container, s_message_list);
    #endif
}

static void on_show(void) {
    ESP_LOGI(TAG, "Chat screen onShow");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_show(SCREEN_ID_CHAT);
    #endif
}

static void on_hide(void) {
    ESP_LOGI(TAG, "Chat screen onHide");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_hide(SCREEN_ID_CHAT);
    #endif
}

// Helper function without lock (for use when already holding lock)
static void add_message_to_list_unlocked(const char *role, const char *content) {
    if (s_message_list == NULL || content == NULL || strlen(content) == 0) {
        return;
    }
    
    // Hide typing indicator if visible
    if (s_typing_indicator != NULL) {
        lv_obj_add_flag(s_typing_indicator, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Remove welcome message if exists (check all children for welcome text)
    uint32_t child_cnt = lv_obj_get_child_cnt(s_message_list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(s_message_list, i);
        // Skip typing indicator
        if (child == s_typing_indicator) {
            continue;
        }
        if (child != NULL && lv_obj_check_type(child, &lv_label_class)) {
            const char *text = lv_label_get_text(child);
            if (text != NULL && strstr(text, "Start a conversation") != NULL) {
                lv_obj_del(child);
                break; // Only remove one welcome message
            }
        }
    }
    
    // Create message container for proper alignment
    lv_obj_t *msg_container = lv_obj_create(s_message_list);
    lv_obj_set_size(msg_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(msg_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(msg_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(msg_container, 5, LV_PART_MAIN);
    lv_obj_set_flex_flow(msg_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(msg_container, 
        (role && strcmp(role, "user") == 0) ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Create message bubble
    lv_obj_t *msg_bubble = lv_obj_create(msg_container);
    lv_obj_set_size(msg_bubble, LV_PCT(85), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(msg_bubble, 
        (role && strcmp(role, "user") == 0) ? UI_COLOR_PRIMARY : UI_COLOR_BG_SECONDARY,
        LV_PART_MAIN);
    lv_obj_set_style_border_width(msg_bubble, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(msg_bubble, UI_SPACE_MD, LV_PART_MAIN);
    lv_obj_set_style_pad_left(msg_bubble, UI_SPACE_LG, LV_PART_MAIN);
    lv_obj_set_style_pad_right(msg_bubble, UI_SPACE_LG, LV_PART_MAIN);
    lv_obj_set_style_radius(msg_bubble, 15, LV_PART_MAIN);
    
    // Message text
    lv_obj_t *msg_label = lv_label_create(msg_bubble);
    lv_label_set_text(msg_label, content);
    lv_obj_set_style_text_font(msg_label, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(msg_label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_label, LV_PCT(100));
    
    // Scroll to bottom with animation
    lv_obj_scroll_to_view(msg_bubble, LV_ANIM_ON);
    
    ESP_LOGI(TAG, "Message added: %s - %s", role ? role : "unknown", content);
}

// Public wrapper with lock (for use from event callbacks and public API)
static void add_message_to_list(const char *role, const char *content) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    add_message_to_list_unlocked(role, content);
    lvgl_port_unlock();
}

static void on_update(const sx_state_t *state) {
    if (state == NULL) {
        return;
    }
    
    // Check if state has changed
    if (state->seq == s_last_state_seq) {
        return;  // No change
    }
    s_last_state_seq = state->seq;
    
    // Update UI based on state
    // For chat screen, we mainly listen to events for new messages
    // State updates can be used for status changes
    if (strcmp(state->ui.status_text, "stt_result") == 0 && state->ui.last_user_message[0] != '\0') {
        add_message_to_list_unlocked("user", state->ui.last_user_message);
    } else if (strcmp(state->ui.status_text, "tts_sentence") == 0 && state->ui.last_assistant_message[0] != '\0') {
        add_message_to_list_unlocked("assistant", state->ui.last_assistant_message);
    }

    // Poll chatbot events and update UI
    sx_event_t evt;
    while (sx_dispatcher_poll_event(&evt)) {
        // Handle chatbot events
        if (evt.type == SX_EVT_CHATBOT_TTS_START) {
            // TTS started - show typing indicator
            ESP_LOGI(TAG, "TTS started");
            s_tts_speaking = true;
            if (s_typing_indicator != NULL) {
                lv_obj_clear_flag(s_typing_indicator, LV_OBJ_FLAG_HIDDEN);
                lv_obj_scroll_to_view(s_typing_indicator, LV_ANIM_ON);
            }
            // Update TTS status
            if (s_tts_status_label != NULL) {
                lv_label_set_text(s_tts_status_label, "TTS: Speaking");
                lv_obj_set_style_text_color(s_tts_status_label, lv_color_hex(0x5b7fff), 0);
            }
        } else if (evt.type == SX_EVT_CHATBOT_TTS_STOP) {
            // TTS stopped - hide typing indicator
            ESP_LOGI(TAG, "TTS stopped");
            s_tts_speaking = false;
            if (s_typing_indicator != NULL) {
                lv_obj_add_flag(s_typing_indicator, LV_OBJ_FLAG_HIDDEN);
            }
            // Update TTS status
            if (s_tts_status_label != NULL) {
                lv_label_set_text(s_tts_status_label, "TTS: Ready");
                lv_obj_set_style_text_color(s_tts_status_label, lv_color_hex(0x888888), 0);
            }
        } else if (evt.type == SX_EVT_CHATBOT_EMOTION) {
            const char *emotion = (const char *)evt.ptr;
            if (emotion != NULL) {
                ESP_LOGI(TAG, "Chatbot emotion: %s", emotion);
                // Update emotion indicator label
                if (s_emotion_label != NULL) {
                    // Simple mapping from emotion string to emoji/text
                    const char *emoji = "🙂";
                    if (strstr(emotion, "happy") || strstr(emotion, "joy")) {
                        emoji = "😄";
                    } else if (strstr(emotion, "sad")) {
                        emoji = "😢";
                    } else if (strstr(emotion, "angry")) {
                        emoji = "😠";
                    } else if (strstr(emotion, "surprise")) {
                        emoji = "😲";
                    } else if (strstr(emotion, "fear") || strstr(emotion, "scared")) {
                        emoji = "😨";
                    }

                    char buf[64];
                    snprintf(buf, sizeof(buf), "Emotion: %s", emoji);
                    lv_label_set_text(s_emotion_label, buf);
                }
                free((void *)evt.ptr);
            }
        } else if (evt.type == SX_EVT_CHATBOT_CONNECTED) {
            ESP_LOGI(TAG, "Chatbot connected");
            s_chatbot_connected = true;
            if (s_status_label != NULL) {
                lv_label_set_text(s_status_label, "● Connected");
                lv_obj_set_style_text_color(s_status_label, lv_color_hex(0x44ff44), 0);
            }
        } else if (evt.type == SX_EVT_CHATBOT_DISCONNECTED) {
            ESP_LOGI(TAG, "Chatbot disconnected");
            s_chatbot_connected = false;
            if (s_status_label != NULL) {
                lv_label_set_text(s_status_label, "● Disconnected");
                lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xff4444), 0);
            }
        }
        // Note: Other events are handled by orchestrator, we only handle UI-related ones here
    }
    
    // Update STT/TTS status from services
    // Update STT status
    bool stt_is_active = sx_stt_is_active();
    if (stt_is_active != s_stt_active && s_stt_status_label != NULL) {
        s_stt_active = stt_is_active;
        if (stt_is_active) {
            lv_label_set_text(s_stt_status_label, "STT: Listening");
            lv_obj_set_style_text_color(s_stt_status_label, lv_color_hex(0x5b7fff), 0);
        } else {
            lv_label_set_text(s_stt_status_label, "STT: Ready");
            lv_obj_set_style_text_color(s_stt_status_label, lv_color_hex(0x888888), 0);
        }
    }
    
    // Update TTS status
    bool tts_is_speaking = sx_tts_is_speaking();
    if (tts_is_speaking != s_tts_speaking && s_tts_status_label != NULL) {
        s_tts_speaking = tts_is_speaking;
        if (tts_is_speaking) {
            lv_label_set_text(s_tts_status_label, "TTS: Speaking");
            lv_obj_set_style_text_color(s_tts_status_label, lv_color_hex(0x5b7fff), 0);
        } else {
            lv_label_set_text(s_tts_status_label, "TTS: Ready");
            lv_obj_set_style_text_color(s_tts_status_label, lv_color_hex(0x888888), 0);
        }
    }
}

static void on_destroy(void) {
    ESP_LOGI(TAG, "Chat screen onDestroy");
    #if SX_UI_VERIFY_MODE
    sx_ui_verify_on_destroy(SCREEN_ID_CHAT);
    #endif
    
    if (s_top_bar != NULL) {
        lv_obj_del(s_top_bar);
        s_top_bar = NULL;
    }
    if (s_message_list != NULL) {
        lv_obj_del(s_message_list);
        s_message_list = NULL;
    }
    if (s_input_bar != NULL) {
        lv_obj_del(s_input_bar);
        s_input_bar = NULL;
    }
    if (s_status_label != NULL) {
        lv_obj_del(s_status_label);
        s_status_label = NULL;
    }
    if (s_stt_status_label != NULL) {
        lv_obj_del(s_stt_status_label);
        s_stt_status_label = NULL;
    }
    if (s_tts_status_label != NULL) {
        lv_obj_del(s_tts_status_label);
        s_tts_status_label = NULL;
    }
    if (s_emotion_label != NULL) {
        lv_obj_del(s_emotion_label);
        s_emotion_label = NULL;
    }
    if (s_typing_indicator != NULL) {
        lv_obj_del(s_typing_indicator);
        s_typing_indicator = NULL;
    }
    
    // Reset state
    s_last_state_seq = 0;
    s_chatbot_connected = false;
    s_tts_speaking = false;
    s_stt_active = false;
}

// Register this screen
void screen_chat_register(void) {
    ui_screen_callbacks_t callbacks = {
        .on_create = on_create,
        .on_show = on_show,
        .on_hide = on_hide,
        .on_destroy = on_destroy,
        .on_update = on_update,
    };
    ui_screen_registry_register(SCREEN_ID_CHAT, &callbacks);
}

// Public API to add messages to chat screen (can be called from other modules)
void screen_chat_add_message(const char *role, const char *content) {
    add_message_to_list(role, content);
}

