/**
 * @file sprite_renderer.h
 * @brief Sprite rendering system for Xteink X4 e-ink display
 *
 * Renders desktop pet sprites based on MQTT state messages.
 * Supports 8 states with optional animation frames.
 */

#ifndef SPRITE_RENDERER_H
#define SPRITE_RENDERER_H

#include <Arduino.h>
#include "mqtt_sprite.h"

// Sprite configuration
#define SPRITE_MAX_SIZE 4096  // Maximum sprite PNG size in bytes
#define SPRITE_WIDTH 200      // Sprite width in pixels
#define SPRITE_HEIGHT 200     // Sprite height in pixels
#define SPRITE_X_OFFSET 290   // X position on 800x480 display (right-aligned)
#define SPRITE_Y_OFFSET 140   // Y position (centered vertically)

// Animation frames per state
#define SPRITE_MAX_FRAMES 4   // Maximum animation frames per state

// Sprite frame structure
typedef struct {
    const uint8_t* data;      // Pointer to PNG data
    size_t size;              // Size of PNG data
    uint16_t duration_ms;     // Duration to display this frame
} sprite_frame_t;

// Sprite animation structure
typedef struct {
    sprite_frame_t frames[SPRITE_MAX_FRAMES];
    uint8_t frame_count;
    uint8_t current_frame;
    unsigned long last_frame_time;
    bool is_animating;
} sprite_animation_t;

/**
 * @brief Initialize the sprite renderer
 *
 * Sets up the sprite system and prepares framebuffers.
 * Must be called before any sprite rendering.
 *
 * @return true on success
 */
bool sprite_renderer_init(void);

/**
 * @brief Display a sprite based on state
 *
 * Renders the appropriate sprite for the given state.
 * If the state has animation, the animation will be handled.
 *
 * @param state The sprite state to display
 * @return true on success
 */
bool sprite_display_state(sprite_state_t state);

/**
 * @brief Display a sprite with message
 *
 * Renders the sprite and adds a text message below it.
 *
 * @param state The sprite state to display
 * @param message Text message to display
 * @return true on success
 */
bool sprite_display_with_message(sprite_state_t state, const char* message);

/**
 * @brief Update animation frame
 *
 * Called regularly to advance animation frames.
 * Call this in the main loop.
 *
 * @return true if display was updated
 */
bool sprite_update_animation(void);

/**
 * @brief Clear the sprite area
 *
 * Clears the sprite display area to white.
 */
void sprite_clear(void);

/**
 * @brief Check if sprite system is ready
 *
 * @return true if sprite system is initialized
 */
bool sprite_is_ready(void);

/**
 * @brief Set sprite position
 *
 * @param x X coordinate
 * @param y Y coordinate
 */
void sprite_set_position(uint16_t x, uint16_t y);

/**
 * @brief Set sprite size
 *
 * @param width Width in pixels
 * @param height Height in pixels
 */
void sprite_set_size(uint16_t width, uint16_t height);

#endif // SPRITE_RENDERER_H
