/**
 * @file sprite_config.h
 * @brief Centralized configuration for sprite and MQTT systems
 *
 * All configurable parameters in one place for easy modification.
 * Supports compile-time defaults and runtime overrides via preferences.
 */

#ifndef SPRITE_CONFIG_H
#define SPRITE_CONFIG_H

#include <Arduino.h>
#include <string.h>
#include "utils.h"

// ============================================================
// MQTT CONFIGURATION
// ============================================================

// Namespace for preferences storage
#define SPRITE_PREF_NAMESPACE "sprite"

// Preference keys
#define SPRITE_KEY_ENABLED "en"        // MQTT enabled
#define SPRITE_KEY_BROKER "br"         // Broker address
#define SPRITE_KEY_PORT "pt"           // Broker port
#define SPRITE_KEY_TOPIC "to"          // Subscription topic
#define SPRITE_KEY_USER "usr"          // Username (optional)
#define SPRITE_KEY_PASS "pwd"          // Password (optional)

// Default values
#define SPRITE_DEFAULT_ENABLED false
#define SPRITE_DEFAULT_BROKER "broker.hivemq.com"
#define SPRITE_DEFAULT_PORT 1883
#define SPRITE_DEFAULT_TOPIC "xteink/x4/display"
#define SPRITE_DEFAULT_CLIENT_ID "xteink-x4"

// Size limits
#define SPRITE_MAX_BROKER_LEN 128
#define SPRITE_MAX_TOPIC_LEN 128
#define SPRITE_MAX_MESSAGE_LEN 512
#define SPRITE_MAX_USER_LEN 64
#define SPRITE_MAX_PASS_LEN 64

// ============================================================
// SPRITE CONFIGURATION
// ============================================================

// Forward declaration - sprite_state_t is defined in mqtt_sprite.h
// This header should be included AFTER mqtt_sprite.h

// Sprite states count (must match mqtt_sprite.h enum)
#define SPRITE_STATE_COUNT 8

// Sprite state names in PROGMEM for efficiency
static const char SPRITE_STATE_NAMES_PGM[][12] PROGMEM = {
    "idle",
    "thinking",
    "talking",
    "excited",
    "sleeping",
    "error",
    "alert",
    "working"
};

// Default sprite position and size (can be overridden at runtime)
#define SPRITE_DEFAULT_X 290
#define SPRITE_DEFAULT_Y 140
#define SPRITE_DEFAULT_WIDTH 200
#define SPRITE_DEFAULT_HEIGHT 200

// Animation settings
#define SPRITE_MAX_FRAMES 4
#define SPRITE_DEFAULT_FRAME_DURATION 500  // ms

// ============================================================
// RUNTIME CONFIG STRUCTURE
// ============================================================

/**
 * @brief Complete sprite system configuration
 * Use this for batch configuration updates
 */
typedef struct {
    // MQTT settings
    struct {
        bool enabled;
        char broker[SPRITE_MAX_BROKER_LEN];
        uint16_t port;
        char topic[SPRITE_MAX_TOPIC_LEN];
        char username[SPRITE_MAX_USER_LEN];
        char password[SPRITE_MAX_PASS_LEN];
        uint16_t keepalive;          // MQTT keepalive in seconds
        uint16_t reconnect_interval;  // Reconnect interval in ms
    } mqtt;

    // Display settings
    struct {
        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;
        bool show_message;           // Show text message with sprite
    } display;

    // Animation settings
    struct {
        bool enabled;
        uint16_t frame_duration;
    } animation;

} sprite_config_t;

/**
 * @brief Initialize config with defaults
 */
inline void sprite_config_init_defaults(sprite_config_t* config) {
    memset(config, 0, sizeof(sprite_config_t));

    // MQTT defaults
    config->mqtt.enabled = SPRITE_DEFAULT_ENABLED;
    safe_strcpy(config->mqtt.broker, SPRITE_DEFAULT_BROKER, sizeof(config->mqtt.broker));
    config->mqtt.port = SPRITE_DEFAULT_PORT;
    safe_strcpy(config->mqtt.topic, SPRITE_DEFAULT_TOPIC, sizeof(config->mqtt.topic));
    config->mqtt.keepalive = 60;
    config->mqtt.reconnect_interval = 5000;

    // Display defaults
    config->display.x = SPRITE_DEFAULT_X;
    config->display.y = SPRITE_DEFAULT_Y;
    config->display.width = SPRITE_DEFAULT_WIDTH;
    config->display.height = SPRITE_DEFAULT_HEIGHT;
    config->display.show_message = true;

    // Animation defaults
    config->animation.enabled = true;
    config->animation.frame_duration = SPRITE_DEFAULT_FRAME_DURATION;
}

// ============================================================
// STATE LOOKUP HELPERS
// ============================================================

/**
 * @brief Get state name string from PROGMEM
 * @param state The sprite state
 * @return State name or "unknown" if invalid
 */
inline const char* sprite_state_name(sprite_state_t state) {
    if (state >= 0 && state < SPRITE_STATE_COUNT) {
        static char buf[12];
        return get_progmem_string(SPRITE_STATE_NAMES_PGM[state], buf, sizeof(buf));
    }
    return "unknown";
}

/**
 * @brief Parse state string to enum (optimized with PROGMEM)
 */
inline sprite_state_t sprite_state_from_string(const char* state_str) {
    if (!state_str) return SPRITE_STATE_UNKNOWN;

    char buf[12];
    for (int i = 0; i < SPRITE_STATE_COUNT; i++) {
        get_progmem_string(SPRITE_STATE_NAMES_PGM[i], buf, sizeof(buf));
        if (strcmp(state_str, buf) == 0) {
            return (sprite_state_t)i;
        }
    }
    return SPRITE_STATE_UNKNOWN;
}

#endif // SPRITE_CONFIG_H
