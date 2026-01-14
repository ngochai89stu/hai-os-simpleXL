# API Catalog: sx_services – Storage Part 4: Playlist Manager & Media Metadata

## Tổng quan

**sx_playlist_manager** quản lý playlist với shuffle, repeat, gapless playback.
**sx_media_metadata** parse metadata từ audio files (ID3v2 cho MP3, Vorbis Comments cho OGG/FLAC).

---

## 1. sx_playlist_manager.h / sx_playlist_manager.c

### A) Vai Trò File

**sx_playlist_manager** là playlist management service. File này:
- Quản lý playlist structure (track paths, current index, shuffle, repeat)
- Cung cấp navigation (next, previous, play index)
- Hỗ trợ shuffle mode và repeat modes (all, one)
- Gapless playback support (preload next track)
- Metadata cache (LRU, size 32) để tránh parse lại

**Dependencies trực tiếp:**
```c
// sx_playlist_manager.c:1-11
#include "sx_playlist_manager.h"
#include "sx_audio_service.h"
#include "sx_dispatcher.h"
#include "sx_event.h"
#include "sx_media_metadata.h"
```

### B) Public API

```c
// sx_playlist_manager.h:25-81
esp_err_t sx_playlist_manager_init(void);
esp_err_t sx_playlist_create(const char **track_paths, size_t track_count, sx_playlist_t **out_playlist);
void sx_playlist_free(sx_playlist_t *playlist);
esp_err_t sx_playlist_set_current(sx_playlist_t *playlist);
sx_playlist_t* sx_playlist_get_current(void);
esp_err_t sx_playlist_next(void);
esp_err_t sx_playlist_previous(void);
esp_err_t sx_playlist_play_index(size_t index);
int sx_playlist_get_current_index(void);
const char* sx_playlist_get_current_track(void);
esp_err_t sx_playlist_set_shuffle(bool enabled);
esp_err_t sx_playlist_set_repeat(bool repeat_all, bool repeat_one);
bool sx_playlist_should_auto_play_next(void);
esp_err_t sx_playlist_preload_next(void);
bool sx_playlist_is_next_preloaded(void);
const char* sx_playlist_get_preloaded_track(void);
const char* sx_playlist_get_title(size_t track_index);
const char* sx_playlist_get_artist(size_t track_index);
const char* sx_playlist_get_genre(size_t track_index);
uint32_t sx_playlist_get_duration(size_t track_index);
size_t sx_playlist_get_count(void);
esp_err_t sx_playlist_get_cover_path(size_t track_index, char *path, size_t path_len);
```

**Contract:**

**`sx_playlist_create()`**
- **Input**: `track_paths` (array of track paths), `track_count`, `out_playlist` (output playlist)
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Playlist manager đã được init
- **Post-conditions**: Playlist đã được tạo với track paths copied
- **Error model**: 
  - `ESP_ERR_INVALID_ARG`: Chưa init, track_paths NULL, track_count == 0
  - `ESP_ERR_NO_MEM`: Memory allocation failed

**`sx_playlist_next()`**
- **Input**: Không có
- **Output**: `ESP_OK` nếu thành công
- **Pre-conditions**: Playlist manager đã được init, current playlist exists
- **Post-conditions**: Next track đã được play (gọi `sx_audio_play_file()`)
- **Error model**: 
  - `ESP_ERR_INVALID_STATE`: Chưa init hoặc no playlist
  - `ESP_ERR_NOT_FOUND`: End of playlist (no repeat)
  - `ESP_ERR_TIMEOUT`: Mutex timeout
  - `ESP_FAIL`: Audio play failed

### C) Data Model

**Static State** (```74:81:components/sx_services/sx_playlist_manager.c```):
- `s_initialized`: Init flag
- `s_current_playlist`: Current playlist pointer
- `s_playlist_mutex`: Mutex để protect playlist operations
- `s_next_preloaded`: Gapless preload flag
- `s_preloaded_index`: Preloaded track index
- `s_preloaded_track_path`: Preloaded track path

**Metadata Cache** (```15:24:components/sx_services/sx_playlist_manager.c```):
- `s_metadata_cache[METADATA_CACHE_SIZE]`: LRU cache (size 32)
- `s_cache_next_index`: Next cache index (round-robin)

**Data Structures** (```15:22:components/sx_services/sx_playlist_manager.h```):
```c
typedef struct {
    char **track_paths;      // Array of track file paths
    size_t track_count;      // Number of tracks
    size_t current_index;    // Current playing track index
    bool shuffle;            // Shuffle mode
    bool repeat_all;         // Repeat all tracks
    bool repeat_one;         // Repeat current track
} sx_playlist_t;
```

**Invariants:**
- Current index: 0 <= current_index < track_count
- Shuffle: Random index selection (```204:205:230:232:components/sx_services/sx_playlist_manager.c```)
- Repeat: repeat_all takes precedence over repeat_one

### D) Concurrency

- **Context**: 
  - **Init**: Chạy từ bootstrap (main task, single-threaded boot)
  - **Playlist operations**: Có thể được gọi từ bất kỳ task nào (UI, orchestrator, audio service)
- **Thread Safety**: 
  - **Playlist state**: Protected bởi `s_playlist_mutex` (```173:184:252:254:290:292:321:323:371:379:390:399:426:489:components/sx_services/sx_playlist_manager.c```)
  - **Metadata cache**: Không được protect bởi mutex (race condition risk)
  - **⚠️ RISK**: Metadata cache có thể race khi nhiều tasks access đồng thời

### E) Memory Ownership

- **Playlist structure**: 
  - **Owner**: Caller owns playlist (created bởi `sx_playlist_create()`)
  - **Lifetime**: Valid từ create đến free
  - **Cleanup**: Free trong `sx_playlist_free()` hoặc `sx_playlist_set_current()` (```178:180:151:166:components/sx_services/sx_playlist_manager.c```)

- **Track paths**: 
  - **Owner**: Playlist owns (malloc trong create)
  - **Lifetime**: Valid trong suốt lifetime của playlist
  - **Cleanup**: Free trong `sx_playlist_free()` (```156:163:components/sx_services/sx_playlist_manager.c```)

- **Preloaded track path**: 
  - **Owner**: Playlist manager owns (malloc trong preload)
  - **Lifetime**: Valid từ preload đến next preload hoặc clear
  - **Cleanup**: Free trong `sx_playlist_preload_next()` khi clear (```431:434:components/sx_services/sx_playlist_manager.c```)

### F) Side Effects

1. **Audio Service**: Gọi `sx_audio_play_file()` khi play next/previous/index (```275:306:336:components/sx_services/sx_playlist_manager.c```)
2. **Metadata Parser**: Gọi `sx_meta_parse_file()` để parse metadata (```48:48:components/sx_services/sx_playlist_manager.c```)
3. **Events**: Post events khi playlist state changes (nếu có)

### G) Call Sites

1. **sx_bootstrap_start()** - Init playlist manager (từ bootstrap)
2. **Audio service** - Gọi `sx_playlist_preload_next()` khi playback stopped (```500:500:components/sx_services/sx_audio_service.c```)
3. **UI screens** - Create playlist, navigate tracks, get metadata
4. **Orchestrator** - Handle playlist events

### H) Issues/Risks

1. **P1 - Metadata Cache Race**: Metadata cache không được protect bởi mutex → có thể race khi nhiều tasks access đồng thời.
   - **Điều kiện**: Nhiều tasks gọi `sx_playlist_get_title()` đồng thời
   - **Cách tái hiện**: Get metadata từ nhiều tasks
   - **Impact**: Cache corruption, invalid metadata

2. **P1 - Shuffle Not Persistent**: Shuffle dùng `rand()` không có seed persistence → mỗi lần boot shuffle order khác.
   - **Điều kiện**: Shuffle mode enabled, reboot
   - **Cách tái hiện**: Enable shuffle, reboot, shuffle order khác
   - **Impact**: Shuffle order không consistent

3. **P2 - Preload Not Implemented**: Gapless preload chỉ store track path, không preload audio data → không thực sự gapless.
   - **Điều kiện**: Preload next track
   - **Cách tái hiện**: `sx_playlist_preload_next()`
   - **Impact**: Gapless playback không smooth (vẫn có delay)

4. **P2 - No Playlist Persistence**: Playlist không được save vào NVS → mất khi reboot.
   - **Điều kiện**: Create playlist, reboot
   - **Cách tái hiện**: Create playlist, reboot
   - **Impact**: Playlist mất, phải tạo lại

### I) Đề Xuất Cải Thiện

1. **P1**: Thêm mutex để protect metadata cache
2. **P1**: Thêm shuffle seed persistence (save vào NVS)
3. **P2**: Implement actual audio data preload cho gapless playback
4. **P2**: Add playlist persistence (save vào NVS hoặc SD card)

---

## 2. sx_media_metadata.h / sx_media_metadata.c

### A) Vai Trò File

**sx_media_metadata** là media metadata parser. File này:
- Parse ID3v2 tags từ MP3 files (TIT2, TPE1, TCON, TLEN frames)
- Parse Vorbis Comments từ OGG/FLAC files (TITLE, ARTIST, GENRE tags)
- Estimate duration từ file size và bitrate (fallback)
- Find cover images trong cùng directory với track

**Dependencies trực tiếp:**
```c
// sx_media_metadata.c:1-12
#include "sx_media_metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
```

### B) Public API

```c
// sx_media_metadata.h:25-38
esp_err_t sx_meta_init(void);
esp_err_t sx_meta_parse_file(const char *file_path, sx_track_meta_t *out);
uint32_t sx_meta_estimate_duration(const char *file_path);
esp_err_t sx_meta_find_cover(const char *track_path, char *cover_path, size_t cover_path_len);
```

**Contract:**

**`sx_meta_parse_file()`**
- **Input**: `file_path` (audio file path), `out` (output metadata)
- **Output**: `ESP_OK` nếu metadata found, `ESP_ERR_NOT_FOUND` nếu no metadata
- **Pre-conditions**: File exists và readable
- **Post-conditions**: Metadata đã được parse và fill vào `out`
- **Error model**: 
  - `ESP_OK`: Metadata found và parsed
  - `ESP_ERR_NOT_FOUND`: No metadata hoặc parse failed
  - `ESP_ERR_INVALID_ARG`: file_path/out NULL
  - `ESP_ERR_NO_MEM`: Memory allocation failed

**`sx_meta_estimate_duration()`**
- **Input**: `file_path` (audio file path)
- **Output**: Duration in milliseconds (0 nếu cannot estimate)
- **Pre-conditions**: File exists
- **Post-conditions**: Return estimated duration
- **Error model**: 
  - `0`: Cannot estimate (file not found, unknown format)
  - `> 0`: Estimated duration (rough estimate based on file size và bitrate)

### C) Data Model

**Data Structures** (```15:22:components/sx_services/sx_media_metadata.h```):
```c
typedef struct {
    char title[256];           // Track title
    char artist[256];           // Artist name
    char genre[64];            // Genre
    uint32_t duration_ms;      // Duration in milliseconds (0 if unknown)
    char cover_hint[512];      // Optional: path to cover image or hint
    bool has_metadata;         // True if metadata was successfully parsed
} sx_track_meta_t;
```

**Invariants:**
- String fields: Null-terminated, max length enforced
- Duration: 0 nếu unknown
- Has metadata: True nếu parse thành công

### D) Concurrency

- **Context**: 
  - **Parse operations**: Có thể được gọi từ bất kỳ task nào (playlist manager, UI)
- **Thread Safety**: 
  - **File I/O**: Standard C file I/O (not thread-safe nếu cùng file)
  - **Static state**: Không có static state (pure functions)
  - **⚠️ RISK**: Nếu nhiều tasks parse cùng file đồng thời, có thể race (unlikely nhưng possible)

### E) Memory Ownership

- **File handles**: 
  - **Owner**: Parser owns (opened trong parse, closed sau khi parse)
  - **Lifetime**: Valid trong suốt parse operation
  - **Cleanup**: Close trong parse function (```413:440:components/sx_services/sx_media_metadata.c```)

- **Metadata output**: 
  - **Owner**: Caller owns output struct
  - **Lifetime**: Valid sau khi parse complete
  - **Usage**: Caller có thể use metadata sau khi parse

- **Temporary buffers**: 
  - **Owner**: Parser owns (malloc trong parse)
  - **Lifetime**: Valid trong suốt parse operation
  - **Cleanup**: Free trong parse function (```80:146:266:299:306:318:components/sx_services/sx_media_metadata.c```)

### F) Side Effects

1. **File I/O**: Open và read audio files để parse metadata
2. **Memory**: Allocate temporary buffers cho tag data (ID3v2, Vorbis Comments)
3. **Logging**: Log parsed metadata qua `ESP_LOGD`

### G) Call Sites

1. **Playlist manager** - Parse metadata khi get track info (```48:48:components/sx_services/sx_playlist_manager.c```)
2. **Audio service** - Parse metadata khi play file (```546:563:components/sx_services/sx_audio_service.c```)
3. **UI screens** - Display track metadata

### H) Issues/Risks

1. **P1 - UTF-16 Not Supported**: ID3v2 UTF-16 encoding không được support đầy đủ (```48:52:components/sx_services/sx_media_metadata.c```) → metadata có thể không hiển thị đúng.
   - **Điều kiện**: MP3 file với UTF-16 encoded tags
   - **Cách tái hiện**: Parse MP3 với UTF-16 title/artist
   - **Impact**: Metadata không được parse, hiển thị empty

2. **P1 - Duration Estimate Inaccurate**: Duration estimate dựa trên file size và fixed bitrate (```443:473:components/sx_services/sx_media_metadata.c```) → không chính xác.
   - **Điều kiện**: File với bitrate khác với assumed (128 kbps MP3, 1000 kbps FLAC)
   - **Cách tái hiện**: Estimate duration cho file với bitrate khác
   - **Impact**: Duration estimate không chính xác

3. **P2 - No Cover Image Parsing**: Không parse cover image từ ID3v2 APIC frame hoặc FLAC PICTURE block → chỉ tìm file trong directory.
   - **Điều kiện**: Track có embedded cover image
   - **Cách tái hiện**: Parse metadata cho track với embedded cover
   - **Impact**: Cover image không được extract

4. **P2 - OGG Parsing Fragile**: OGG parsing logic phức tạp (```212:301:components/sx_services/sx_media_metadata.c```) → có thể fail với non-standard OGG files.
   - **Điều kiện**: OGG file với non-standard structure
   - **Cách tái hiện**: Parse non-standard OGG file
   - **Impact**: Metadata không được parse

### I) Đề Xuất Cải Thiện

1. **P1**: Implement UTF-16 to UTF-8 conversion cho ID3v2
2. **P1**: Improve duration estimate (detect actual bitrate từ file)
3. **P2**: Parse embedded cover images (APIC, PICTURE blocks)
4. **P2**: Improve OGG parsing robustness

---

## Tổng Kết Phần 4

### Điểm Mạnh

1. **Playlist Features**: Shuffle, repeat, gapless playback support
2. **Metadata Cache**: LRU cache để tránh parse lại
3. **Multi-Format**: Hỗ trợ MP3 (ID3v2) và OGG/FLAC (Vorbis Comments)

### Điểm Yếu

1. **Metadata Cache Race**: Không được protect bởi mutex
2. **Shuffle Not Persistent**: Shuffle order không consistent
3. **Gapless Not Implemented**: Chỉ store path, không preload audio data
4. **UTF-16 Not Supported**: Metadata có thể không hiển thị đúng

### Đề Xuất Cải Thiện Tổng Thể

1. **P1**: Fix metadata cache race với mutex
2. **P1**: Add shuffle seed persistence
3. **P2**: Implement actual gapless playback
4. **P2**: Support UTF-16 encoding

---

**Hoàn thành Batch 6**: Đã phân tích đầy đủ 4 phần của storage services (settings, SD, assets, playlist + metadata).
