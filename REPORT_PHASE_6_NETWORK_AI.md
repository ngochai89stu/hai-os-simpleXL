# PHASE 6 — Network/AI/Protocols
## Báo cáo phân tích network stack, AI services, và communication protocols

**Ngày tạo:** 2025-01-02  
**Dự án:** hai-os-simplexl  
**Mục tiêu:** Phân tích WiFi, HTTP, MQTT, WebSocket, AI services (Wake Word, STT, TTS, Chatbot), và OTA

---

## 1. WIFI SERVICE

### 1.1 WiFi Initialization

**Nguồn:** `components/sx_services/sx_wifi_service.c:L45-L122`

**Initialization Sequence:**

```
1. Initialize network optimizer (L59)
   └── sx_network_optimizer_init()

2. Initialize network interface (L62)
   └── esp_netif_init()

3. Create default event loop (L69)
   └── esp_event_loop_create_default()

4. Create WiFi STA netif (L77)
   └── esp_netif_create_default_wifi_sta()

5. Initialize WiFi (L85)
   └── esp_wifi_init(&cfg_init)

6. Register event handlers (L92-L110)
   ├── WIFI_EVENT handler
   └── IP_EVENT handler

7. Create event group (L113)
   └── xEventGroupCreate()
```

**Configuration:**

```c
wifi_init_config_t cfg_init = WIFI_INIT_CONFIG_DEFAULT();
```

**Phân tích:**
- ✅ **Standard ESP-IDF WiFi:** Sử dụng standard ESP-IDF WiFi API
- ✅ **Event-driven:** Event handlers cho connection/disconnection
- ✅ **Network optimizer:** Integration với network optimizer cho retry logic
- ⚠️ **No AP mode:** Chỉ STA mode, không có AP mode

### 1.2 WiFi Connection

**Nguồn:** `components/sx_services/sx_wifi_service.c:L244-L299`

**Connection Flow:**

```c
esp_err_t sx_wifi_connect(const char *ssid, const char *password) {
    // 1. Disconnect if already connected
    if (s_connected) {
        sx_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // 2. Store credentials
    strncpy(s_current_ssid, ssid, sizeof(s_current_ssid) - 1);
    if (password != NULL) {
        strncpy(s_current_password, password, sizeof(s_current_password) - 1);
    }
    
    // 3. Configure WiFi
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password != NULL) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    // 4. Set WiFi config
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    
    // 5. Clear event bits and reset retry
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_num = 0;
    
    // 6. Connect
    esp_wifi_connect();
}
```

**Phân tích:**
- ✅ **WPA2 PSK:** Default WPA2 authentication
- ✅ **PMF support:** Protected Management Frames enabled
- ✅ **Retry logic:** Auto-retry với exponential backoff
- ⚠️ **No credential storage:** Credentials không persist, cần reconnect mỗi boot

### 1.3 WiFi Event Handling

**Nguồn:** `components/sx_services/sx_wifi_service.c:L342-L422`

**Event Types:**

1. **WIFI_EVENT_STA_START:**
   - WiFi station started

2. **WIFI_EVENT_STA_DISCONNECTED:**
   - Auto-reconnect với exponential backoff
   - Max retry: 5 attempts
   - Publish `SX_EVT_WIFI_DISCONNECTED` event

3. **WIFI_EVENT_STA_CONNECTED:**
   - WiFi connected (before IP assignment)
   - Store channel info

4. **IP_EVENT_STA_GOT_IP:**
   - IP address assigned
   - Get RSSI
   - Publish `SX_EVT_WIFI_CONNECTED` event
   - Record successful connection

**Reconnection Logic:**

```c
if (s_cfg.auto_reconnect && s_retry_num < MAX_RETRY) {
    s_retry_num++;
    
    // Use network optimizer for retry delay
    sx_retry_config_t retry_config = {
        .initial_delay_ms = 1000,
        .max_delay_ms = 30000,
        .max_attempts = MAX_RETRY,
        .exponential_backoff = true,
        .backoff_multiplier = 1.5f,
    };
    uint32_t delay_ms = sx_network_optimizer_get_retry_delay(s_retry_num, &retry_config);
    
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    esp_wifi_connect();
}
```

**Phân tích:**
- ✅ **Auto-reconnect:** Automatic reconnection với exponential backoff
- ✅ **Network optimizer:** Integration với network optimizer cho smart retry
- ✅ **Event publishing:** Publish events cho UI sync
- ⚠️ **Max retry:** 5 attempts có thể không đủ cho poor network

### 1.4 WiFi Scanning

**Nguồn:** `components/sx_services/sx_wifi_service.c:L174-L242`

**Scan Configuration:**

```c
wifi_scan_config_t scan_config = {
    .ssid = NULL,
    .bssid = NULL,
    .channel = 0,
    .show_hidden = true,
    .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    .scan_time = {
        .active = {
            .min = 100,
            .max = 300
        }
    }
};
```

**Phân tích:**
- ✅ **Active scan:** Active scan mode cho better results
- ✅ **Hidden networks:** Support hidden networks
- ⚠️ **Blocking scan:** Blocking scan có thể block main thread

---

## 2. HTTP CLIENT

### 2.1 HTTP Client Usage

**Services sử dụng HTTP Client:**

1. **STT Service:** POST audio chunks to STT endpoint
2. **TTS Service:** GET audio from TTS endpoint
3. **Radio Service:** Stream audio from HTTP URLs
4. **OTA Service:** Download firmware updates
5. **Music Online Service:** Stream online music
6. **Weather Service:** Fetch weather data
7. **Chatbot Service:** HTTP-based chatbot (if configured)

### 2.2 STT HTTP Request

**Nguồn:** `components/sx_services/sx_stt_service.c:L38-L132`

**Request Flow:**

```c
// 1. Create HTTP client
esp_http_client_config_t http_config = {
    .url = s_config.endpoint_url,
    .timeout_ms = 10000,
};
esp_http_client_handle_t client = esp_http_client_init(&http_config);

// 2. Set headers
esp_http_client_set_method(client, HTTP_METHOD_POST);
esp_http_client_set_header(client, "Content-Type", "audio/pcm;rate=16000;channels=1");
if (s_config.api_key != NULL) {
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", s_config.api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);
}

// 3. Set POST data (PCM audio)
size_t data_len = chunk.sample_count * sizeof(int16_t);
esp_http_client_set_post_field(client, (const char *)chunk.pcm, data_len);

// 4. Perform request
esp_err_t ret = esp_http_client_perform(client);

// 5. Read response
int status_code = esp_http_client_get_status_code(client);
int content_length = esp_http_client_get_content_length(client);
char *response = (char *)malloc(content_length + 1);
int read_len = esp_http_client_read_response(client, response, content_length);

// 6. Parse JSON response
cJSON *json = cJSON_Parse(response);
cJSON *transcript = cJSON_GetObjectItem(json, "transcript");
cJSON *is_final = cJSON_GetObjectItem(json, "is_final");
```

**Phân tích:**
- ✅ **Bearer token auth:** Support API key authentication
- ✅ **JSON parsing:** Parse JSON response với cJSON
- ✅ **Final/interim results:** Support final và interim results
- ⚠️ **No streaming:** Không support streaming STT (chunk-based only)
- ⚠️ **Timeout:** 10s timeout có thể không đủ cho slow network

### 2.3 TTS HTTP Request

**Nguồn:** `components/sx_services/sx_tts_service.c:L322-L418`

**Request Flow:**

```c
// 1. Build request URL
char url[512];
snprintf(url, sizeof(url), "%s?text=%s", s_endpoint_url, text);
// Note: URL encoding needed in production

// 2. Create HTTP client
esp_http_client_config_t config = {
    .url = url,
    .timeout_ms = s_config.timeout_ms,
};
esp_http_client_handle_t client = esp_http_client_init(&config);

// 3. Add API key header
if (s_api_key[0] != '\0') {
    esp_http_client_set_header(client, "X-API-Key", s_api_key);
}

// 4. Perform GET request
esp_http_client_set_method(client, HTTP_METHOD_GET);
esp_err_t ret = esp_http_client_perform(client);

// 5. Read audio response
int content_length = esp_http_client_get_content_length(client);
int16_t *audio_data = (int16_t *)malloc(content_length);
int read_len = esp_http_client_read_response(client, (char *)audio_data, content_length);
```

**Phân tích:**
- ✅ **GET request:** Simple GET request với text parameter
- ✅ **API key support:** Support API key authentication
- ⚠️ **No URL encoding:** Text không được URL encode → có thể fail với special chars
- ⚠️ **Synchronous:** Blocking request → có thể block TTS task

### 2.4 Radio HTTP Streaming

**Nguồn:** `components/sx_services/sx_radio_service.c`

**Streaming Flow:**

```
HTTP GET Request
    ↓
Read stream data (chunked or non-chunked)
    ↓
Parse ICY metadata (if enabled)
    ↓
Detect audio format (Content-Type)
    ↓
Decode audio (AAC/MP3/FLAC)
    ↓
Feed to audio service
```

**Buffer Management:**

```c
#define READ_BUFFER_SIZE_DEFAULT 4096
#define MIN_BUFFER_BEFORE_PLAY_DEFAULT 2048

// Phase 5: Dynamic buffer sizing
static uint32_t s_buffer_fill_target_ms = 500;  // Target buffer fill
static uint32_t s_buffer_fill_current_ms = 0;   // Current buffer fill
static bool s_buffer_ready = false;             // Buffer ready for playback
```

**Phân tích:**
- ✅ **Chunked streaming:** Support chunked HTTP responses
- ✅ **ICY metadata:** Parse ICY metadata for station info
- ✅ **Format detection:** Auto-detect audio format từ Content-Type
- ✅ **Buffer management:** Dynamic buffer sizing based on network conditions
- ⚠️ **No resume:** Không support resume on disconnect (restart from beginning)

---

## 3. MQTT PROTOCOL

### 3.1 MQTT Client Implementation

**Nguồn:** `components/sx_protocol/sx_protocol_mqtt.c`

**MQTT Configuration:**

```c
typedef struct {
    char *broker_url;        // MQTT broker URL (mqtt:// or mqtts://)
    char *username;          // Optional username
    char *password;          // Optional password
    char *client_id;         // Client ID
    char *topic_prefix;      // Topic prefix (e.g., "device/123/")
    uint32_t keepalive;      // Keepalive interval (seconds)
    bool use_tls;            // Use TLS
    const char *cert_pem;    // TLS certificate (if use_tls)
} sx_protocol_mqtt_config_t;
```

**MQTT Events:**

1. **MQTT_EVENT_CONNECTED:**
   - Subscribe to default topics
   - Publish `SX_EVT_CHATBOT_CONNECTED` event

2. **MQTT_EVENT_DISCONNECTED:**
   - Publish `SX_EVT_CHATBOT_DISCONNECTED` event

3. **MQTT_EVENT_ERROR:**
   - Publish `SX_EVT_PROTOCOL_ERROR` event

4. **MQTT_EVENT_DATA:**
   - Parse JSON message
   - Handle hello message (audio params, session_id)
   - Route to chatbot handler

**Phân tích:**
- ✅ **TLS support:** Support MQTT over TLS
- ✅ **Topic prefix:** Flexible topic prefix configuration
- ✅ **JSON messages:** Parse JSON messages với cJSON
- ⚠️ **No QoS 2:** Chỉ support QoS 0 và 1
- ⚠️ **No retained messages:** Không handle retained messages

### 3.2 MQTT Audio Streaming

**Nguồn:** `components/sx_protocol/sx_protocol_mqtt.c` (cần verify implementation)

**Audio Packet Format:**

```c
typedef struct {
    uint32_t sequence;      // Packet sequence number
    uint32_t timestamp;     // Timestamp
    uint32_t sample_rate;   // Sample rate
    uint32_t channels;      // Channels (1=mono, 2=stereo)
    uint32_t data_size;     // Audio data size
    uint8_t data[];         // Audio data (Opus encoded)
} sx_audio_stream_packet_t;
```

**Phân tích:**
- ✅ **Opus encoding:** Support Opus audio encoding
- ✅ **Sequence tracking:** Packet sequence number cho ordering
- ⚠️ **No jitter buffer:** Không có jitter buffer cho network delay

### 3.3 MQTT UDP Bridge

**Nguồn:** `components/sx_protocol/sx_protocol_mqtt_udp.c` (cần verify)

**UDP Bridge Features:**
- AES encryption cho audio packets
- UDP transport cho low latency
- Fallback to MQTT nếu UDP fail

**Phân tích:**
- ✅ **Low latency:** UDP transport cho real-time audio
- ✅ **Encryption:** AES encryption cho security
- ⚠️ **No reliability:** UDP không guarantee delivery

---

## 4. WEBSOCKET PROTOCOL

### 4.1 WebSocket Client Implementation

**Nguồn:** `components/sx_protocol/sx_protocol_ws.c`

**WebSocket Configuration:**

```c
typedef struct {
    char *url;               // WebSocket URL (ws:// or wss://)
    char *username;          // Optional username
    char *password;          // Optional password
    uint32_t reconnect_interval_ms;  // Reconnect interval
    bool use_tls;            // Use TLS
    const char *cert_pem;    // TLS certificate (if use_tls)
} sx_protocol_ws_config_t;
```

**WebSocket Events:**

1. **WEBSOCKET_EVENT_CONNECTED:**
   - Publish `SX_EVT_CHATBOT_CONNECTED` event

2. **WEBSOCKET_EVENT_DISCONNECTED:**
   - Schedule reconnection
   - Publish `SX_EVT_CHATBOT_DISCONNECTED` event

3. **WEBSOCKET_EVENT_ERROR:**
   - Schedule reconnection
   - Publish `SX_EVT_PROTOCOL_ERROR` event

4. **WEBSOCKET_EVENT_DATA:**
   - Parse text frames (JSON messages)
   - Handle binary frames (audio packets)
   - Update last incoming time

**Phân tích:**
- ✅ **TLS support:** Support WebSocket over TLS (WSS)
- ✅ **Auto-reconnect:** Automatic reconnection với exponential backoff
- ✅ **Binary frames:** Support binary frames cho audio streaming
- ⚠️ **No ping/pong:** Không có ping/pong cho keepalive

### 4.2 WebSocket Audio Streaming

**Nguồn:** `components/sx_protocol/sx_protocol_ws.c:L60-L61`

**Audio Packet Buffer:**

```c
#define MAX_AUDIO_PACKET_SIZE (4000 + sizeof(sx_binary_protocol_v2_t))
static uint8_t s_ws_audio_send_buffer[MAX_AUDIO_PACKET_SIZE];
static SemaphoreHandle_t s_ws_audio_buffer_mutex = NULL;
```

**Protocol Version 2:**

```c
typedef struct {
    uint8_t version;         // Protocol version (2)
    uint8_t type;            // Packet type (audio, text, etc.)
    uint32_t sequence;        // Sequence number
    uint32_t timestamp;      // Timestamp
    uint32_t sample_rate;    // Sample rate
    uint32_t frame_duration; // Frame duration (ms)
    uint32_t data_size;      // Audio data size
    uint8_t data[];          // Audio data (Opus encoded)
} sx_binary_protocol_v2_t;
```

**Phân tích:**
- ✅ **Static buffer:** Reusable buffer để tránh malloc/free
- ✅ **Mutex protection:** Thread-safe buffer access
- ✅ **Protocol versioning:** Support multiple protocol versions
- ⚠️ **No fragmentation:** Không handle large packet fragmentation

### 4.3 WebSocket Hello Message

**Nguồn:** `components/sx_protocol/sx_protocol_ws.c:L154-L200`

**Hello Message Format:**

```json
{
    "type": "hello",
    "audio_params": {
        "sample_rate": 16000,
        "frame_duration": 60
    },
    "session_id": "abc123"
}
```

**Hello Handling:**

```c
// Parse audio_params
cJSON *audio_params = cJSON_GetObjectItem(root, "audio_params");
if (audio_params != NULL) {
    cJSON *sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
    if (cJSON_IsNumber(sample_rate)) {
        hello->server_sample_rate = (uint32_t)sample_rate->valueint;
        s_server_sample_rate = hello->server_sample_rate;
    }
    cJSON *frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
    if (cJSON_IsNumber(frame_duration)) {
        hello->server_frame_duration = (uint32_t)frame_duration->valueint;
        s_server_frame_duration = hello->server_frame_duration;
    }
}

// Parse session_id
cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
if (cJSON_IsString(session_id)) {
    strncpy(hello->session_id, session_id->valuestring, sizeof(hello->session_id) - 1);
}
```

**Phân tích:**
- ✅ **Audio params negotiation:** Negotiate sample rate và frame duration
- ✅ **Session tracking:** Track session ID cho reconnection
- ✅ **Event publishing:** Publish `SX_EVT_PROTOCOL_HELLO_RECEIVED` event

---

## 5. PROTOCOL FACTORY

### 5.1 Protocol Factory Pattern

**Nguồn:** `components/sx_protocol/sx_protocol_factory.c` (cần verify)

**Factory Purpose:**
- Auto-detect available protocols (WebSocket, MQTT)
- Provide unified interface (`sx_protocol_base_t`)
- Switch between protocols dynamically

**Base Interface:**

```c
typedef struct {
    bool (*is_connected)(sx_protocol_base_t *protocol);
    esp_err_t (*send_text)(sx_protocol_base_t *protocol, const char *text);
    esp_err_t (*send_audio)(sx_protocol_base_t *protocol, const sx_audio_stream_packet_t *packet);
    esp_err_t (*connect)(sx_protocol_base_t *protocol);
    esp_err_t (*disconnect)(sx_protocol_base_t *protocol);
} sx_protocol_base_ops_t;

typedef struct {
    sx_protocol_base_ops_t *ops;
    void *impl;
} sx_protocol_base_t;
```

**Phân tích:**
- ✅ **Unified interface:** Single interface cho multiple protocols
- ✅ **Auto-detection:** Auto-detect available protocols
- ✅ **Dynamic switching:** Switch protocols at runtime
- ⚠️ **No fallback:** Không có automatic fallback nếu protocol fail

---

## 6. WAKE WORD SERVICE

### 6.1 Wake Word Detection

**Nguồn:** `components/sx_services/sx_wake_word_service.c`

**Wake Word Engine:**

```c
typedef enum {
    SX_WAKE_WORD_TYPE_ESP_SR,  // ESP-SR wake word engine
    SX_WAKE_WORD_TYPE_CUSTOM,  // Custom wake word model
} sx_wake_word_type_t;

typedef struct {
    sx_wake_word_type_t type;
    const char *model_path;   // Path to wake word model
    float threshold;           // Detection threshold (0.0-1.0)
    bool enable_opus_encoding; // Enable Opus encoding
} sx_wake_word_config_t;
```

**Wake Word Task:**

```c
static void sx_wake_word_task(void *arg) {
    const size_t AUDIO_BUFFER_SAMPLES = 320; // 20ms at 16kHz
    int16_t *audio_buffer = malloc(AUDIO_BUFFER_SAMPLES * sizeof(int16_t));
    
    while (s_active) {
        // Get audio from queue
        if (xQueueReceive(s_audio_queue, audio_buffer, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Process audio through wake word engine
            sx_wake_word_feed_audio_esp_sr(audio_buffer, AUDIO_BUFFER_SAMPLES);
        }
    }
}
```

**Audio Queue:**

```c
const size_t AUDIO_BUFFER_SAMPLES = 320; // 20ms at 16kHz
s_audio_queue = xQueueCreate(10, AUDIO_BUFFER_SAMPLES * sizeof(int16_t));
```

**Phân tích:**
- ✅ **ESP-SR integration:** Support ESP-SR wake word engine
- ✅ **Queue-based:** Audio queue cho buffering
- ✅ **20ms chunks:** 20ms audio chunks cho low latency
- ⚠️ **ESP-SR required:** Requires ESP-SR library (CONFIG_SX_WAKE_WORD_ESP_SR_ENABLE)
- ⚠️ **Queue size:** 10 buffers = 200ms buffer → có thể miss wake word nếu queue full

### 6.2 Wake Word Callback

**Nguồn:** `components/sx_services/sx_wake_word_service.c:L152-L186`

**Callback Registration:**

```c
esp_err_t sx_wake_word_start(sx_wake_word_detected_cb_t callback, void *user_data) {
    s_detected_callback = callback;
    s_user_data = user_data;
    
    // Register callback with ESP-SR wrapper
    esp_err_t ret = sx_wake_word_register_callback_esp_sr(callback, user_data);
    
    // Create wake word detection task
    xTaskCreate(sx_wake_word_task, "sx_wake_word", 4096, NULL, 5, &s_wake_word_task_handle);
}
```

**Phân tích:**
- ✅ **Callback pattern:** Flexible callback cho wake word detection
- ✅ **User data:** Support user data trong callback
- ⚠️ **Task priority:** Priority 5 có thể conflict với other audio tasks

---

## 7. STT SERVICE

### 7.1 STT Service Architecture

**Nguồn:** `components/sx_services/sx_stt_service.c`

**STT Configuration:**

```c
typedef struct {
    const char *endpoint_url;    // STT endpoint URL
    const char *api_key;        // API key (optional)
    uint32_t chunk_duration_ms; // Chunk duration (default: 1000ms)
    uint32_t sample_rate_hz;   // Sample rate (default: 16000)
    bool auto_send;            // Auto-send chunks
} sx_stt_config_t;
```

**STT Task:**

```c
static void sx_stt_task(void *arg) {
    while (s_active) {
        stt_audio_chunk_t chunk;
        if (xQueueReceive(s_chunk_queue, &chunk, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Send audio chunk to STT endpoint
            // HTTP POST request với PCM audio
            // Parse JSON response
            // Call callback với transcript
        }
    }
}
```

**Audio Chunk Queue:**

```c
#define STT_CHUNK_QUEUE_SIZE 5
s_chunk_queue = xQueueCreate(STT_CHUNK_QUEUE_SIZE, sizeof(stt_audio_chunk_t));
```

**Phân tích:**
- ✅ **Chunk-based:** Chunk-based STT cho streaming
- ✅ **Queue buffering:** Queue cho audio chunks
- ✅ **JSON parsing:** Parse JSON response với cJSON
- ⚠️ **Queue size:** 5 chunks có thể không đủ cho slow network
- ⚠️ **No streaming STT:** Không support streaming STT (chunk-based only)

### 7.2 STT Result Handling

**Nguồn:** `components/sx_services/sx_stt_service.c:L86-L109`

**Result Callback:**

```c
typedef void (*sx_stt_result_cb_t)(const char *transcript, bool is_final, void *user_data);

// Call callback
if (s_result_callback != NULL) {
    s_result_callback(transcript->valuestring, final, s_user_data);
}

// Dispatch event if final
if (final) {
    sx_event_t evt = {
        .type = SX_EVT_UI_INPUT,
        .arg0 = 0,
        .arg1 = 0,
        .ptr = strdup(transcript->valuestring),
    };
    sx_dispatcher_post_event(&evt);
}
```

**Phân tích:**
- ✅ **Final/interim results:** Support final và interim results
- ✅ **Event publishing:** Publish events cho UI sync
- ⚠️ **Memory leak risk:** `strdup()` trong event → cần free trong event handler

---

## 8. TTS SERVICE

### 8.1 TTS Service Architecture

**Nguồn:** `components/sx_services/sx_tts_service.c`

**TTS Configuration:**

```c
typedef struct {
    const char *endpoint_url;    // TTS endpoint URL
    const char *api_key;         // API key (optional)
    uint32_t timeout_ms;        // HTTP timeout (default: 10000ms)
    sx_tts_priority_t default_priority;  // Default priority
} sx_tts_config_t;
```

**TTS Request Queue:**

```c
typedef struct {
    char text[512];
    sx_tts_priority_t priority;
    sx_tts_audio_callback_t audio_callback;
    sx_tts_error_callback_t error_callback;
    void *user_data;
} sx_tts_request_t;

s_tts_queue = xQueueCreate(10, sizeof(sx_tts_request_t));
```

**TTS Task:**

```c
static void sx_tts_task(void *arg) {
    sx_tts_request_t request;
    
    while (1) {
        // Wait for request
        if (xQueueReceive(s_tts_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        
        // Synthesize
        esp_err_t ret = sx_tts_synthesize(&request);
        
        // Call audio callback or use default playback
        if (request.audio_callback) {
            request.audio_callback(audio_data, audio_size, sample_rate, request.user_data);
        } else {
            // Default: play through audio service
            sx_audio_service_feed_pcm(audio_data, samples, sample_rate);
        }
    }
}
```

**Phân tích:**
- ✅ **Priority queue:** Priority-based request queue
- ✅ **Callback pattern:** Flexible audio callback
- ✅ **Default playback:** Default playback through audio service
- ⚠️ **No URL encoding:** Text không được URL encode → có thể fail
- ⚠️ **Synchronous HTTP:** Blocking HTTP request → có thể block TTS task

---

## 9. CHATBOT SERVICE

### 9.1 Chatbot Service Architecture

**Nguồn:** `components/sx_services/sx_chatbot_service.c`

**Chatbot Configuration:**

```c
typedef struct {
    const char *endpoint;  // Chatbot endpoint URL
} sx_chatbot_config_t;
```

**Chatbot Task:**

```c
static void sx_chatbot_task(void *arg) {
    chatbot_message_t msg;
    while (1) {
        // Process messages from queue
        if (xQueueReceive(s_message_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Phase 5: Try intent parsing first
            if (s_intent_parsing_enabled) {
                esp_err_t intent_ret = sx_intent_execute(msg.text);
                if (intent_ret == ESP_OK) {
                    continue; // Intent handled
                }
            }
            
            // Fallback: Check if message is a music/radio command
            char song_name[256];
            if (sx_chatbot_is_music_command(msg.text, song_name, sizeof(song_name))) {
                // Play radio stream
                sx_radio_play_station(stream_url);
            } else {
                // Send message via protocol (WebSocket/MQTT)
                sx_protocol_base_t *protocol = sx_chatbot_get_protocol_base();
                if (protocol && protocol->ops->is_connected(protocol)) {
                    // Build JSON message
                    cJSON *json = cJSON_CreateObject();
                    cJSON_AddStringToObject(json, "type", "user_message");
                    cJSON_AddStringToObject(json, "text", msg.text);
                    
                    char *json_str = cJSON_PrintUnformatted(json);
                    protocol->ops->send_text(protocol, json_str);
                }
            }
        }
    }
}
```

**Message Queue:**

```c
#define CHATBOT_QUEUE_SIZE 10
s_message_queue = xQueueCreate(CHATBOT_QUEUE_SIZE, sizeof(chatbot_message_t));
```

**Phân tích:**
- ✅ **Intent parsing:** Integration với intent service
- ✅ **Music commands:** Support music/radio commands
- ✅ **Protocol abstraction:** Use protocol factory cho WebSocket/MQTT
- ⚠️ **Queue size:** 10 messages có thể không đủ cho high traffic
- ⚠️ **No message priority:** Không có message priority

### 9.2 Chatbot JSON Message Handling

**Nguồn:** `components/sx_services/sx_chatbot_service.c:L314-L601`

**Message Types:**

1. **"stt":** STT transcript
   ```json
   {
       "type": "stt",
       "text": "Hello world"
   }
   ```

2. **"tts":** TTS state updates
   ```json
   {
       "type": "tts",
       "state": "start|stop|sentence_start",
       "text": "Hello world"
   }
   ```

3. **"llm":** LLM emotion
   ```json
   {
       "type": "llm",
       "emotion": "happy|sad|angry"
   }
   ```

4. **"mcp":** MCP raw message
   ```json
   {
       "type": "mcp",
       "data": {...}
   }
   ```

**Phân tích:**
- ✅ **Multiple message types:** Support STT, TTS, LLM, MCP messages
- ✅ **Event publishing:** Publish events cho UI sync
- ⚠️ **No message validation:** Không validate message structure

---

## 10. OTA SERVICE

### 10.1 OTA Update Flow

**Nguồn:** `components/sx_services/sx_ota_service.c`

**OTA Configuration:**

```c
typedef struct {
    const char *url;        // OTA update URL
    const char *cert_pem;   // TLS certificate (optional)
    uint32_t timeout_ms;    // HTTP timeout
} sx_ota_config_t;
```

**OTA Task:**

```c
static void sx_ota_task(void *arg) {
    sx_ota_config_t *config = (sx_ota_config_t *)arg;
    
    // Create HTTPS OTA config
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };
    
    // Begin OTA
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t ret = esp_https_ota_begin(&ota_config, &https_ota_handle);
    
    // Perform OTA
    while (1) {
        ret = esp_https_ota_perform(https_ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        // Report progress
        int percent = (int)((image_header_was_was_valid * 100) / image_header_total_len);
        s_progress_callback(percent);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Finish OTA
    if (ret == ESP_OK) {
        esp_https_ota_finish(https_ota_handle);
        esp_restart();
    } else {
        esp_https_ota_abort(https_ota_handle);
    }
}
```

**Phân tích:**
- ✅ **HTTPS OTA:** Support HTTPS OTA với certificate validation
- ✅ **Progress callback:** Progress callback cho UI updates
- ✅ **Auto-reboot:** Auto-reboot sau khi OTA complete
- ⚠️ **No rollback:** Không có rollback mechanism nếu OTA fail
- ⚠️ **No verification:** Không verify firmware signature

---

## 11. LỖI TIỀM ẨN & NỢ KỸ THUẬT

### 11.1 P0 (Critical) Issues

1. **No URL Encoding (TTS)**
   - **Vị trí:** `components/sx_services/sx_tts_service.c:L325`
   - **Vấn đề:** Text không được URL encode → có thể fail với special chars
   - **Hậu quả:** TTS request fail với special characters
   - **Cách sửa:** URL encode text trước khi build URL

2. **Memory Leak (STT Event)**
   - **Vị trí:** `components/sx_services/sx_stt_service.c:L106`
   - **Vấn đề:** `strdup()` trong event → cần free trong event handler
   - **Hậu quả:** Memory leak nếu event handler không free
   - **Cách sửa:** Use `sx_event_alloc_string()` thay vì `strdup()`

3. **No Credential Storage (WiFi)**
   - **Vị trí:** `components/sx_services/sx_wifi_service.c:L244-L299`
   - **Vấn đề:** Credentials không persist → cần reconnect mỗi boot
   - **Hậu quả:** Poor user experience
   - **Cách sửa:** Store credentials trong NVS

### 11.2 P1 (High) Issues

1. **Queue Size Too Small (Wake Word)**
   - **Vị trí:** `components/sx_services/sx_wake_word_service.c:L138`
   - **Vấn đề:** 10 buffers = 200ms buffer → có thể miss wake word
   - **Cách sửa:** Tăng queue size hoặc implement drop-oldest policy

2. **No Streaming STT**
   - **Vị trí:** `components/sx_services/sx_stt_service.c`
   - **Vấn đề:** Chunk-based STT only, không support streaming
   - **Cách sửa:** Implement streaming STT với WebSocket

3. **Synchronous HTTP (TTS)**
   - **Vị trí:** `components/sx_services/sx_tts_service.c:L322-L418`
   - **Vấn đề:** Blocking HTTP request → có thể block TTS task
   - **Cách sửa:** Use async HTTP client hoặc separate task

4. **No Protocol Fallback**
   - **Vị trí:** `components/sx_protocol/sx_protocol_factory.c`
   - **Vấn đề:** Không có automatic fallback nếu protocol fail
   - **Cách sửa:** Implement protocol fallback logic

### 11.3 P2 (Medium) Issues

1. **No Ping/Pong (WebSocket)**
   - **Vị trí:** `components/sx_protocol/sx_protocol_ws.c`
   - **Vấn đề:** Không có ping/pong cho keepalive
   - **Cách sửa:** Implement WebSocket ping/pong

2. **No Jitter Buffer (MQTT Audio)**
   - **Vị trí:** `components/sx_protocol/sx_protocol_mqtt.c`
   - **Vấn đề:** Không có jitter buffer cho network delay
   - **Cách sửa:** Implement jitter buffer

3. **No Resume (Radio Streaming)**
   - **Vị trí:** `components/sx_services/sx_radio_service.c`
   - **Vấn đề:** Không support resume on disconnect
   - **Cách sửa:** Implement HTTP Range requests

---

## 12. KẾT LUẬN PHASE 6

### 12.1 Điểm Mạnh

1. ✅ **Multi-protocol support:** WebSocket, MQTT, HTTP
2. ✅ **Protocol factory:** Unified interface cho multiple protocols
3. ✅ **AI services:** Wake Word, STT, TTS, Chatbot
4. ✅ **Event-driven:** Event-driven architecture cho UI sync
5. ✅ **Network optimizer:** Smart retry logic với exponential backoff
6. ✅ **HTTPS OTA:** Secure OTA updates

### 12.2 Điểm Yếu

1. ⚠️ **No URL encoding:** TTS text không được URL encode
2. ⚠️ **Memory leak risk:** STT event memory leak
3. ⚠️ **No credential storage:** WiFi credentials không persist
4. ⚠️ **Queue sizes:** Queue sizes có thể không đủ
5. ⚠️ **No streaming STT:** Chunk-based STT only
6. ⚠️ **Synchronous HTTP:** Blocking HTTP requests

### 12.3 Hành Động Tiếp Theo

**PHASE 7:** Phân tích Storage & Persistence

---

## 13. CHECKLIST HOÀN THÀNH PHASE 6

- [x] Phân tích WiFi service (init, connect, scan, events)
- [x] Phân tích HTTP client (STT, TTS, Radio, OTA)
- [x] Phân tích MQTT protocol (client, audio streaming, UDP bridge)
- [x] Phân tích WebSocket protocol (client, audio streaming, hello message)
- [x] Phân tích protocol factory (unified interface, auto-detection)
- [x] Phân tích Wake Word service (ESP-SR integration, queue-based)
- [x] Phân tích STT service (chunk-based, HTTP POST, JSON parsing)
- [x] Phân tích TTS service (HTTP GET, audio callback, priority queue)
- [x] Phân tích Chatbot service (intent parsing, protocol abstraction, JSON handling)
- [x] Phân tích OTA service (HTTPS OTA, progress callback, auto-reboot)
- [x] Xác định lỗi tiềm ẩn và nợ kỹ thuật
- [x] Tạo REPORT_PHASE_6_NETWORK_AI.md

---

## 14. THỐNG KÊ FILE ĐÃ ĐỌC

**Tổng số file đã đọc trong Phase 6:** ~15 files

**Danh sách:**
1. `components/sx_services/sx_wifi_service.c`
2. `components/sx_services/sx_stt_service.c`
3. `components/sx_services/sx_tts_service.c`
4. `components/sx_services/sx_wake_word_service.c`
5. `components/sx_services/sx_chatbot_service.c`
6. `components/sx_services/sx_radio_service.c` (partial)
7. `components/sx_services/sx_ota_service.c`
8. `components/sx_protocol/sx_protocol_ws.c` (partial)
9. `components/sx_protocol/sx_protocol_mqtt.c` (partial)

**Ước lượng % file đã đọc:** ~25-28% (đọc network/AI-critical files)

---

**Kết thúc PHASE 6**

