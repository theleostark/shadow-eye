/**
 * @file utils.h
 * @brief Common utility functions and macros for Xteink X4
 *
 * Provides:
 * - Unified logging with module prefixes
 * - Configuration management helpers
 * - Memory-efficient string operations
 * - Constants and limits
 */

#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <Preferences.h>
#include <pgmspace.h>

// ============================================================
// LOGGING UTILITIES
// ============================================================

// Log levels
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE
} log_level_t;

// Current log level (can be changed at runtime)
extern log_level_t g_current_log_level;

// Module-based logging macros
#define LOG_ENABLED(level) ((level) <= g_current_log_level)

#define LOG_ERROR(module, fmt, ...) \
    do { if (LOG_ENABLED(LOG_LEVEL_ERROR)) log_e("[%s] " fmt, module, ##__VA_ARGS__); } while(0)

#define LOG_WARN(module, fmt, ...) \
    do { if (LOG_ENABLED(LOG_LEVEL_WARN)) log_w("[%s] " fmt, module, ##__VA_ARGS__); } while(0)

#define LOG_INFO(module, fmt, ...) \
    do { if (LOG_ENABLED(LOG_LEVEL_INFO)) log_i("[%s] " fmt, module, ##__VA_ARGS__); } while(0)

#define LOG_DEBUG(module, fmt, ...) \
    do { if (LOG_ENABLED(LOG_LEVEL_DEBUG)) log_d("[%s] " fmt, module, ##__VA_ARGS__); } while(0)

#define LOG_VERBOSE(module, fmt, ...) \
    do { if (LOG_ENABLED(LOG_LEVEL_VERBOSE)) log_v("[%s] " fmt, module, ##__VA_ARGS__); } while(0)

// Module name macros
#define LOG_MOD_MQTT "MQTT"
#define LOG_MOD_SPRITE "SPRITE"
#define LOG_MOD_DISPLAY "DISPLAY"
#define LOG_MOD_CONFIG "CONFIG"

// ============================================================
// MEMORY & STRING UTILITIES
// ============================================================

/**
 * @brief Safe string copy with null termination
 * @param dest Destination buffer
 * @param src Source string
 * @param dest_size Size of destination buffer
 * @return Length of copied string
 */
static inline size_t safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return 0;
    size_t i;
    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return i;
}

/**
 * @brief Safe string concatenation with bounds checking
 * @param dest Destination buffer (must contain null-terminated string)
 * @param src Source string to append
 * @param dest_size Total size of destination buffer
 * @return New length of destination string
 */
static inline size_t safe_strcat(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return 0;
    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size - 1) return dest_len;
    return dest_len + safe_strcpy(dest + dest_len, src, dest_size - dest_len);
}

/**
 * @brief Check if string is null or empty
 */
static inline bool str_is_null_or_empty(const char* str) {
    return !str || *str == '\0';
}

/**
 * @brief Compare strings with NULL safety
 */
static inline int safe_strcmp(const char* a, const char* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a, b);
}

// ============================================================
// ARRAY UTILITIES
// ============================================================

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

#define CLAMP(x, min, max) \
    ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// ============================================================
// CONFIGURATION HELPERS
// ============================================================

/**
 * @brief Scoped preferences helper - auto-closes on scope exit
 * Reduces code duplication and ensures preferences are always closed
 */
class ScopedPreferences {
private:
    Preferences& _prefs;
    bool _opened;

public:
    explicit ScopedPreferences(Preferences& prefs, const char* namespace_str, bool read_only = false)
        : _prefs(prefs), _opened(false) {
        _opened = _prefs.begin(namespace_str, read_only);
    }

    ~ScopedPreferences() {
        if (_opened) {
            _prefs.end();
        }
    }

    bool is_open() const { return _opened; }
    operator bool() const { return _opened; }

    // Delete copy operations
    ScopedPreferences(const ScopedPreferences&) = delete;
    ScopedPreferences& operator=(const ScopedPreferences&) = delete;
};

// ============================================================
// CONSTANTS & LIMITS
// ============================================================

// Network
#define MAX_HOSTNAME_LEN 128
#define MAX_TOPIC_LEN 128
#define MAX_MESSAGE_LEN 512
#define MAX_USERNAME_LEN 64
#define MAX_PASSWORD_LEN 64

// Timing
#define RECONNECT_INTERVAL_MS 5000
#define HEARTBEAT_INTERVAL_MS 30000

// Display
#define DISPLAY_WIDTH 800
#define DISPLAY_HEIGHT 480

// ============================================================
// PROGMEM HELPERS
// ============================================================

/**
 * @brief Get string from PROGMEM
 */
static inline const char* get_progmem_string(const char* progmem_str, char* buf, size_t buf_len) {
    if (!progmem_str || !buf || buf_len == 0) return nullptr;
    strncpy_P(buf, progmem_str, buf_len - 1);
    buf[buf_len - 1] = '\0';
    return buf;
}

#endif // UTILS_H
