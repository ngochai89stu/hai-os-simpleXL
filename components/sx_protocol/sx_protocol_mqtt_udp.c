// MQTT UDP Audio Channel Implementation
// Based on xiaozhi-esp32_vietnam_ref implementation

#include "sx_protocol_audio.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/errno.h"
#include "mbedtls/aes.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <sys/ioctl.h>

static const char *TAG = "sx_mqtt_udp";

// UDP audio channel state
static int s_udp_socket = -1;
static struct sockaddr_in s_udp_server_addr = {0};
static bool s_udp_connected = false;
static mbedtls_aes_context s_aes_ctx;
static uint8_t s_aes_nonce[16] = {0};
static uint32_t s_local_sequence = 0;
static uint32_t s_remote_sequence = 0;
static sx_protocol_audio_callback_t s_audio_callback = NULL;
static SemaphoreHandle_t s_udp_mutex = NULL;
static TaskHandle_t s_udp_receive_task = NULL;
static bool s_udp_receive_task_running = false;

// Server audio parameters (from hello message)
static uint32_t s_server_sample_rate = 16000;
static uint32_t s_server_frame_duration = 60;

// UDP packet format (as per xiaozhi-esp32_vietnam_ref):
// |type 1u|flags 1u|payload_len 2u|ssrc 4u|timestamp 4u|sequence 4u|
// |payload payload_len|
typedef struct __attribute__((packed)) {
    uint8_t type;           // 0x01 = audio
    uint8_t flags;          // Reserved
    uint16_t payload_len;   // Payload size (network byte order)
    uint32_t ssrc;          // SSRC (network byte order)
    uint32_t timestamp;     // Timestamp (network byte order)
    uint32_t sequence;      // Sequence number (network byte order)
    uint8_t payload[];      // Encrypted Opus data
} mqtt_udp_audio_packet_t;

// Decode hex string to binary
static esp_err_t decode_hex_string(const char *hex_str, uint8_t *out, size_t out_size) {
    if (hex_str == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t hex_len = strlen(hex_str);
    if (hex_len % 2 != 0 || hex_len / 2 > out_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    for (size_t i = 0; i < hex_len; i += 2) {
        char c1 = hex_str[i];
        char c2 = hex_str[i + 1];
        
        uint8_t val = 0;
        if (c1 >= '0' && c1 <= '9') val = (c1 - '0') << 4;
        else if (c1 >= 'A' && c1 <= 'F') val = (c1 - 'A' + 10) << 4;
        else if (c1 >= 'a' && c1 <= 'f') val = (c1 - 'a' + 10) << 4;
        else return ESP_ERR_INVALID_ARG;
        
        if (c2 >= '0' && c2 <= '9') val |= (c2 - '0');
        else if (c2 >= 'A' && c2 <= 'F') val |= (c2 - 'A' + 10);
        else if (c2 >= 'a' && c2 <= 'f') val |= (c2 - 'a' + 10);
        else return ESP_ERR_INVALID_ARG;
        
        out[i / 2] = val;
    }
    
    return ESP_OK;
}

// UDP receive task
static void udp_receive_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "UDP receive task started");
    
    uint8_t *recv_buffer = (uint8_t *)malloc(2048);
    if (recv_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate receive buffer");
        s_udp_receive_task_running = false;
        vTaskDelete(NULL);
        return;
    }
    
    while (s_udp_receive_task_running && s_udp_connected) {
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        
        int len = lwip_recvfrom(s_udp_socket, recv_buffer, 2048, 0, 
                          (struct sockaddr *)&from_addr, &from_len);
        
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            ESP_LOGE(TAG, "UDP recvfrom error: %d", errno);
            break;
        }
        
        if (len < sizeof(s_aes_nonce)) {
            ESP_LOGE(TAG, "Invalid audio packet size: %d", len);
            continue;
        }
        
        // Parse packet header
        if (recv_buffer[0] != 0x01) {
            ESP_LOGE(TAG, "Invalid audio packet type: 0x%02x", recv_buffer[0]);
            continue;
        }
        
        uint32_t timestamp = ntohl(*(uint32_t *)&recv_buffer[8]);
        uint32_t sequence = ntohl(*(uint32_t *)&recv_buffer[12]);
        
        // Validate sequence
        if (sequence < s_remote_sequence) {
            ESP_LOGW(TAG, "Received audio packet with old sequence: %lu, expected: %lu", 
                    sequence, s_remote_sequence);
            continue;
        }
        if (sequence != s_remote_sequence + 1) {
            ESP_LOGW(TAG, "Received audio packet with wrong sequence: %lu, expected: %lu", 
                    sequence, s_remote_sequence + 1);
        }
        
        // Decrypt
        size_t decrypted_size = len - sizeof(s_aes_nonce);
        uint8_t *nonce = recv_buffer;
        uint8_t *encrypted = recv_buffer + sizeof(s_aes_nonce);
        
        uint8_t *decrypted = (uint8_t *)malloc(decrypted_size);
        if (decrypted == NULL) {
            ESP_LOGE(TAG, "Failed to allocate decrypted buffer");
            continue;
        }
        
        size_t nc_off = 0;
        uint8_t stream_block[16] = {0};
        int ret = mbedtls_aes_crypt_ctr(&s_aes_ctx, decrypted_size, &nc_off, 
                                       nonce, stream_block, encrypted, decrypted);
        
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to decrypt audio data, ret: %d", ret);
            free(decrypted);
            continue;
        }
        
        // Create audio packet
        sx_audio_stream_packet_t packet = {
            .sample_rate = s_server_sample_rate,
            .frame_duration = s_server_frame_duration,
            .timestamp = timestamp,
            .payload = decrypted,
            .payload_size = decrypted_size,
        };
        
        // Call callback
        if (s_audio_callback != NULL) {
            s_audio_callback(&packet);
        }
        
        // Note: Callback should copy payload if needed, we free it here
        free(decrypted);
        
        s_remote_sequence = sequence;
    }
    
    free(recv_buffer);
    ESP_LOGI(TAG, "UDP receive task stopped");
    s_udp_receive_task_running = false;
    vTaskDelete(NULL);
}

// Initialize UDP audio channel
esp_err_t sx_protocol_mqtt_udp_init(const char *udp_server, int udp_port, 
                                    const char *aes_key_hex, const char *aes_nonce_hex) {
    if (s_udp_connected) {
        return ESP_OK;  // Already initialized
    }
    
    if (udp_server == NULL || aes_key_hex == NULL || aes_nonce_hex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_udp_mutex == NULL) {
        s_udp_mutex = xSemaphoreCreateMutex();
        if (s_udp_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    
    // Decode AES key and nonce
    uint8_t aes_key[16] = {0};
    esp_err_t ret = decode_hex_string(aes_key_hex, aes_key, sizeof(aes_key));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode AES key");
        return ret;
    }
    
    ret = decode_hex_string(aes_nonce_hex, s_aes_nonce, sizeof(s_aes_nonce));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to decode AES nonce");
        return ret;
    }
    
    // Initialize AES context
    mbedtls_aes_init(&s_aes_ctx);
    mbedtls_aes_setkey_enc(&s_aes_ctx, aes_key, 128);
    
    // Create UDP socket
    s_udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket");
        mbedtls_aes_free(&s_aes_ctx);
        return ESP_FAIL;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(s_udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Set non-blocking using ioctl (ESP-IDF compatible)
    int flags = 1;
    ioctl(s_udp_socket, FIONBIO, &flags);
    
    // Setup server address
    memset(&s_udp_server_addr, 0, sizeof(s_udp_server_addr));
    s_udp_server_addr.sin_family = AF_INET;
    s_udp_server_addr.sin_port = htons(udp_port);
    
    struct hostent *he = gethostbyname(udp_server);
    if (he == NULL) {
        ESP_LOGE(TAG, "Failed to resolve hostname: %s", udp_server);
        lwip_close(s_udp_socket);
        s_udp_socket = -1;
        mbedtls_aes_free(&s_aes_ctx);
        return ESP_FAIL;
    }
    
    memcpy(&s_udp_server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    // Connect to server
    if (lwip_connect(s_udp_socket, (struct sockaddr *)&s_udp_server_addr, 
               sizeof(s_udp_server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to connect UDP socket");
        lwip_close(s_udp_socket);
        s_udp_socket = -1;
        mbedtls_aes_free(&s_aes_ctx);
        return ESP_FAIL;
    }
    
    s_local_sequence = 0;
    s_remote_sequence = 0;
    s_udp_connected = true;
    
    // Start receive task
    s_udp_receive_task_running = true;
    xTaskCreate(udp_receive_task, "mqtt_udp_rx", 4096, NULL, 5, &s_udp_receive_task);
    
    ESP_LOGI(TAG, "UDP audio channel initialized: %s:%d", udp_server, udp_port);
    return ESP_OK;
}

// Close UDP audio channel
void sx_protocol_mqtt_udp_close(void) {
    if (!s_udp_connected) {
        return;
    }
    
    // Stop receive task
    s_udp_receive_task_running = false;
    if (s_udp_receive_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (s_udp_receive_task != NULL) {
            vTaskDelete(s_udp_receive_task);
            s_udp_receive_task = NULL;
        }
    }
    
    // Close socket
    if (s_udp_socket >= 0) {
        lwip_close(s_udp_socket);
        s_udp_socket = -1;
    }
    
    // Free AES context
    mbedtls_aes_free(&s_aes_ctx);
    
    s_udp_connected = false;
    ESP_LOGI(TAG, "UDP audio channel closed");
}

// Send audio packet via UDP
esp_err_t sx_protocol_mqtt_udp_send_audio(const sx_audio_stream_packet_t *packet) {
    if (!s_udp_connected || packet == NULL || packet->payload == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_udp_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Prepare nonce (copy base nonce and update fields)
    uint8_t nonce[16];
    memcpy(nonce, s_aes_nonce, sizeof(nonce));
    *(uint16_t *)&nonce[2] = htons(packet->payload_size);
    *(uint32_t *)&nonce[8] = htonl(packet->timestamp);
    *(uint32_t *)&nonce[12] = htonl(++s_local_sequence);
    
    // Optimize: Single allocation for UDP packet (encrypted payload + UDP header)
    // Note: nonce is only used for encryption, not sent in UDP packet
    size_t udp_packet_size = sizeof(mqtt_udp_audio_packet_t) + packet->payload_size;
    uint8_t *udp_packet = (uint8_t *)malloc(udp_packet_size);
    if (udp_packet == NULL) {
        xSemaphoreGive(s_udp_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    mqtt_udp_audio_packet_t *pkt = (mqtt_udp_audio_packet_t *)udp_packet;
    
    // Encrypt payload directly into UDP packet payload (optimization: single allocation)
    size_t nc_off = 0;
    uint8_t stream_block[16] = {0};
    int ret = mbedtls_aes_crypt_ctr(&s_aes_ctx, packet->payload_size, &nc_off,
                                    nonce, stream_block, packet->payload,
                                    pkt->payload);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to encrypt audio data");
        free(udp_packet);
        xSemaphoreGive(s_udp_mutex);
        return ESP_FAIL;
    }
    
    // Fill UDP packet header
    pkt->type = 0x01;
    pkt->flags = 0;
    pkt->payload_len = htons(packet->payload_size);
    pkt->ssrc = 0;  // Not used
    pkt->timestamp = htonl(packet->timestamp);
    pkt->sequence = htonl(s_local_sequence);
    
    // Send packet
    int sent = lwip_send(s_udp_socket, udp_packet, udp_packet_size, 0);
    
    free(udp_packet);
    xSemaphoreGive(s_udp_mutex);
    
    if (sent < 0) {
        ESP_LOGE(TAG, "Failed to send UDP packet: %d", errno);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// Set audio callback
void sx_protocol_mqtt_udp_set_audio_callback(sx_protocol_audio_callback_t callback) {
    s_audio_callback = callback;
}

// Set server audio parameters
void sx_protocol_mqtt_udp_set_server_params(uint32_t sample_rate, uint32_t frame_duration) {
    s_server_sample_rate = sample_rate;
    s_server_frame_duration = frame_duration;
}

// Check if UDP channel is opened
bool sx_protocol_mqtt_udp_is_opened(void) {
    return s_udp_connected;
}

