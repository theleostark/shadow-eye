/**
 * @file mqtt_sprite.h
 * @brief MQTT-based sprite display system for Xteink X4
 *
 * Receives JSON messages via MQTT and displays animated sprites
 * based on the bot state (idle, thinking, talking, excited, sleeping, error, alert, working)
 *
 * Inspired by: https://www.reddit.com/r/xteinkereader/comments/1qpzhiy/
 */

#ifndef MQTT_SPRITE_H
#define MQTT_SPRITE_H

#include <Arduino.h>
#include <Preferences.h>

// MQTT Configuration
#define MQTT_SPRITE_PREF_NAMESPACE "mqtt_sprite"
#define MQTT_ENABLED_KEY "enabled"
#define MQTT_BROKER_KEY "broker"
#define MQTT_PORT_KEY "port"
#define MQTT_TOPIC_KEY "topic"
#define MQTT_USERNAME_KEY "username"
#define MQTT_PASSWORD_KEY "password"

// Default values
#define MQTT_DEFAULT_BROKER "broker.hivemq.com"
#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_TOPIC "xteink/x4/display"
#define MQTT_MAX_TOPIC_LEN 128
#define MQTT_MAX_MESSAGE_LEN 512

// Sprite states
typedef enum {
    SPRITE_STATE_IDLE = 0,
    SPRITE_STATE_THINKING,
    SPRITE_STATE_TALKING,
    SPRITE_STATE_EXCITED,
    SPRITE_STATE_SLEEPING,
    SPRITE_STATE_ERROR,
    SPRITE_STATE_ALERT,
    SPRITE_STATE_WORKING,
    SPRITE_STATE_UNKNOWN = 255
} sprite_state_t;

// Sprite state names (must match order above)
static const char* SPRITE_STATE_NAMES[] = {
    "idle",
    "thinking",
    "talking",
    "excited",
    "sleeping",
    "error",
    "alert",
    "working"
};

// Message structure from MQTT
typedef struct {
    char message[128];      // Text message to display
    sprite_state_t state;   // Current sprite state
    bool has_message;       // Whether message field is populated
    bool has_state;         // Whether state field is populated
} sprite_message_t;

/**
 * @brief Initialize MQTT sprite system
 *
 * Loads preferences and sets up MQTT client
 *
 * @return true on success
 */
bool mqtt_sprite_init(void);

/**
 * @brief Main loop handler - must be called regularly
 *
 * Handles MQTT connection, message receiving, and display updates
 * Call this in your main loop() function
 *
 * @return true if display should be updated
 */
bool mqtt_sprite_loop(void);

/**
 * @brief Get the latest sprite message
 *
 * @param msg Output buffer for the message
 * @return true if a new message is available
 */
bool mqtt_sprite_get_message(sprite_message_t *msg);

/**
 * @brief Parse JSON message into sprite_message_t
 *
 * Expected JSON format:
 * {"message": "text", "state": "thinking"}
 * or
 * {"state": "idle"}
 *
 * @param json JSON string to parse
 * @param msg Output message structure
 * @return true on success
 */
bool mqtt_sprite_parse_json(const char *json, sprite_message_t *msg);

/**
 * @brief Convert string to sprite_state_t
 *
 * @param state_str State string (e.g., "thinking")
 * @return Corresponding sprite_state_t
 */
sprite_state_t mqtt_sprite_string_to_state(const char *state_str);

/**
 * @brief Get current connection status
 *
 * @return true if connected to MQTT broker
 */
bool mqtt_sprite_is_connected(void);

/**
 * @brief Disconnect and cleanup
 */
void mqtt_sprite_stop(void);

/**
 * @brief Enable or disable MQTT sprites
 *
 * @param enabled true to enable
 */
void mqtt_sprite_set_enabled(bool enabled);

/**
 * @brief Check if MQTT sprites are enabled
 *
 * @return true if enabled
 */
bool mqtt_sprite_is_enabled(void);

/**
 * @brief Save MQTT configuration to preferences
 *
 * @param broker MQTT broker address
 * @param port MQTT port
 * @param topic MQTT topic
 * @param username Optional username (can be NULL)
 * @param password Optional password (can be NULL)
 */
void mqtt_sprite_save_config(const char *broker, uint16_t port,
                              const char *topic, const char *username,
                              const char *password);

/**
 * @brief Load MQTT configuration from preferences
 *
 * @param broker Output buffer for broker
 * @param broker_len Size of broker buffer
 * @param port Output port
 * @param topic Output buffer for topic
 * @param topic_len Size of topic buffer
 * @return true if config was loaded
 */
bool mqtt_sprite_load_config(char *broker, size_t broker_len,
                              uint16_t *port, char *topic, size_t topic_len);

#endif // MQTT_SPRITE_H
