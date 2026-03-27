/**
 * @file mqtt_sprite.cpp
 * @brief MQTT-based sprite display system implementation
 */

#include "mqtt_sprite.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "ArduinoJson.h"
#include "display.h"

// External references
extern Preferences preferences;

// Global MQTT client
static WiFiClient mqtt_wifi_client;
static PubSubClient mqtt_client(mqtt_wifi_client);

// Configuration
static char mqtt_broker[128] = MQTT_DEFAULT_BROKER;
static uint16_t mqtt_port = MQTT_DEFAULT_PORT;
static char mqtt_topic[MQTT_MAX_TOPIC_LEN] = MQTT_DEFAULT_TOPIC;
static char mqtt_username[64] = {0};
static char mqtt_password[64] = {0};

// State tracking
static bool mqtt_enabled = false;
static unsigned long last_mqtt_connect_attempt = 0;
static const unsigned long MQTT_RECONNECT_INTERVAL = 5000;  // 5 seconds

// Latest message
static sprite_message_t latest_message = {0};
static bool new_message_available = false;

// Logger for this module
#define MQTT_LOG(fmt, ...) log_i("[MQTT_SPRITE] " fmt, ##__VA_ARGS__)

// Callback for received MQTT messages
void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    MQTT_LOG("[MQTT_SPRITE] Received message on topic: %s", topic);

    // Null-terminate payload
    char payload_str[MQTT_MAX_MESSAGE_LEN];
    length = min(length, (unsigned int)(MQTT_MAX_MESSAGE_LEN - 1));
    memcpy(payload_str, payload, length);
    payload_str[length] = '\0';

    MQTT_LOG("[MQTT_SPRITE] Payload: %s", payload_str);

    // Parse JSON
    sprite_message_t msg;
    if (mqtt_sprite_parse_json(payload_str, &msg)) {
        latest_message = msg;
        new_message_available = true;
        MQTT_LOG("[MQTT_SPRITE] Parsed: state=%d, message=%s",
                 msg.state, msg.message);
    } else {
        MQTT_LOG("[MQTT_SPRITE] Failed to parse JSON");
    }
}

bool mqtt_sprite_init(void) {
    MQTT_LOG("[MQTT_SPRITE] Initializing...");

    // Load configuration from preferences
    char topic_buf[MQTT_MAX_TOPIC_LEN];
    char broker_buf[128];

    if (!mqtt_sprite_load_config(broker_buf, sizeof(broker_buf),
                                  &mqtt_port, topic_buf, sizeof(topic_buf))) {
        // Use defaults
        strncpy(mqtt_broker, MQTT_DEFAULT_BROKER, sizeof(mqtt_broker) - 1);
        mqtt_port = MQTT_DEFAULT_PORT;
        strncpy(mqtt_topic, MQTT_DEFAULT_TOPIC, sizeof(mqtt_topic) - 1);
    } else {
        strncpy(mqtt_broker, broker_buf, sizeof(mqtt_broker) - 1);
        strncpy(mqtt_topic, topic_buf, sizeof(mqtt_topic) - 1);
    }

    // Check if enabled
    preferences.begin(MQTT_SPRITE_PREF_NAMESPACE, true);
    mqtt_enabled = preferences.getBool(MQTT_ENABLED_KEY, false);
    preferences.end();

    if (!mqtt_enabled) {
        MQTT_LOG("[MQTT_SPRITE] MQTT sprites disabled");
        return false;
    }

    // Configure MQTT client
    mqtt_client.setServer(mqtt_broker, mqtt_port);
    mqtt_client.setCallback(mqtt_callback);

    // Note: PubSubClient credentials are set via connect() call
    // For HiveMQ public broker, no credentials needed

    MQTT_LOG("[MQTT_SPRITE] Configured: %s:%d, topic: %s",
             mqtt_broker, mqtt_port, mqtt_topic);

    return true;
}

bool mqtt_sprite_connect(void) {
    if (!mqtt_enabled) {
        return false;
    }

    if (mqtt_client.connected()) {
        return true;
    }

    // Don't reconnect too frequently
    unsigned long now = millis();
    if (now - last_mqtt_connect_attempt < MQTT_RECONNECT_INTERVAL) {
        return false;
    }
    last_mqtt_connect_attempt = now;

    MQTT_LOG("[MQTT_SPRITE] Connecting to %s:%d...", mqtt_broker, mqtt_port);

    // Attempt connection
    // Note: Public HiveMQ doesn't require authentication
    bool connected = mqtt_client.connect("xteink-x4");

    if (connected) {
        MQTT_LOG("[MQTT_SPRITE] Connected!");

        // Subscribe to topic
        if (mqtt_client.subscribe(mqtt_topic)) {
            MQTT_LOG("[MQTT_SPRITE] Subscribed to: %s", mqtt_topic);
        } else {
            MQTT_LOG("[MQTT_SPRITE] Failed to subscribe");
        }
    } else {
        MQTT_LOG("[MQTT_SPRITE] Connection failed: %d",
                 mqtt_client.state());
    }

    return connected;
}

bool mqtt_sprite_loop(void) {
    if (!mqtt_enabled) {
        return false;
    }

    // Maintain connection
    mqtt_sprite_connect();

    // Process MQTT messages
    mqtt_client.loop();

    // Return true if new message available
    return new_message_available;
}

bool mqtt_sprite_get_message(sprite_message_t *msg) {
    if (!new_message_available) {
        return false;
    }

    *msg = latest_message;
    new_message_available = false;
    return true;
}

bool mqtt_sprite_parse_json(const char *json, sprite_message_t *msg) {
    // Initialize output
    memset(msg, 0, sizeof(sprite_message_t));
    msg->state = SPRITE_STATE_UNKNOWN;
    msg->has_message = false;
    msg->has_state = false;

    // Parse JSON
    // Note: Using DynamicJsonDocument for ArduinoJson v7+
    DynamicJsonDocument doc(256);

    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        MQTT_LOG("JSON parse error: %s", error.c_str());
        return false;
    }

    // Extract "message" field
    if (doc.containsKey("message")) {
        const char *message_str = doc["message"];
        if (message_str) {
            strncpy(msg->message, message_str, sizeof(msg->message) - 1);
            msg->has_message = true;
        }
    }

    // Extract "state" field
    if (doc.containsKey("state")) {
        const char *state_str = doc["state"];
        if (state_str) {
            msg->state = mqtt_sprite_string_to_state(state_str);
            msg->has_state = true;
        }
    }

    // At least one field must be present
    return msg->has_message || msg->has_state;
}

sprite_state_t mqtt_sprite_string_to_state(const char *state_str) {
    for (int i = 0; i < (int)(sizeof(SPRITE_STATE_NAMES) / sizeof(SPRITE_STATE_NAMES[0])); i++) {
        if (strcmp(state_str, SPRITE_STATE_NAMES[i]) == 0) {
            return (sprite_state_t)i;
        }
    }
    return SPRITE_STATE_UNKNOWN;
}

const char* mqtt_sprite_state_to_string(sprite_state_t state) {
    if (state >= 0 && state < (int)(sizeof(SPRITE_STATE_NAMES) / sizeof(SPRITE_STATE_NAMES[0]))) {
        return SPRITE_STATE_NAMES[state];
    }
    return "unknown";
}

bool mqtt_sprite_is_connected(void) {
    return mqtt_client.connected();
}

void mqtt_sprite_stop(void) {
    mqtt_client.disconnect();
    MQTT_LOG("[MQTT_SPRITE] Stopped");
}

void mqtt_sprite_save_config(const char *broker, uint16_t port,
                              const char *topic, const char *username,
                              const char *password) {
    preferences.begin(MQTT_SPRITE_PREF_NAMESPACE, false);

    preferences.putString(MQTT_BROKER_KEY, broker);
    preferences.putUInt(MQTT_PORT_KEY, port);
    preferences.putString(MQTT_TOPIC_KEY, topic);

    if (username) {
        preferences.putString(MQTT_USERNAME_KEY, username);
    } else {
        preferences.remove(MQTT_USERNAME_KEY);
    }

    if (password) {
        preferences.putString(MQTT_PASSWORD_KEY, password);
    } else {
        preferences.remove(MQTT_PASSWORD_KEY);
    }

    preferences.end();
    MQTT_LOG("[MQTT_SPRITE] Configuration saved");
}

bool mqtt_sprite_load_config(char *broker, size_t broker_len,
                              uint16_t *port, char *topic, size_t topic_len) {
    preferences.begin(MQTT_SPRITE_PREF_NAMESPACE, true);

    String broker_str = preferences.getString(MQTT_BROKER_KEY, MQTT_DEFAULT_BROKER);
    String topic_str = preferences.getString(MQTT_TOPIC_KEY, MQTT_DEFAULT_TOPIC);
    uint16_t port_val = preferences.getUInt(MQTT_PORT_KEY, MQTT_DEFAULT_PORT);

    String username_str = preferences.getString(MQTT_USERNAME_KEY, "");
    String password_str = preferences.getString(MQTT_PASSWORD_KEY, "");

    preferences.end();

    strncpy(broker, broker_str.c_str(), broker_len - 1);
    strncpy(topic, topic_str.c_str(), topic_len - 1);
    *port = port_val;

    if (username_str.length() > 0) {
        strncpy(mqtt_username, username_str.c_str(), sizeof(mqtt_username) - 1);
    }

    if (password_str.length() > 0) {
        strncpy(mqtt_password, password_str.c_str(), sizeof(mqtt_password) - 1);
    }

    return true;
}

void mqtt_sprite_set_enabled(bool enabled) {
    preferences.begin(MQTT_SPRITE_PREF_NAMESPACE, false);
    preferences.putBool(MQTT_ENABLED_KEY, enabled);
    preferences.end();
    mqtt_enabled = enabled;

    if (enabled) {
        MQTT_LOG("[MQTT_SPRITE] MQTT sprites enabled");
    } else {
        MQTT_LOG("[MQTT_SPRITE] MQTT sprites disabled");
    }
}

bool mqtt_sprite_is_enabled(void) {
    return mqtt_enabled;
}
