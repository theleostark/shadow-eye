/**
 * @file sprite_renderer.cpp
 * @brief Sprite rendering system implementation
 */

#ifndef ECHO_DISABLE_MQTT_SPRITES

#include "sprite_renderer.h"
#include "sprite_data.h"
#include "display.h"
#include "DEV_Config.h"
#include <PNGdec.h>
#include <SPIFFS.h>

// Logger for this module
#define SPRITE_LOG(fmt, ...) log_i("[SPRITE] " fmt, ##__VA_ARGS__)

// Sprite configuration
static struct {
    uint16_t x_offset;
    uint16_t y_offset;
    uint16_t width;
    uint16_t height;
    bool initialized;
    sprite_state_t current_state;
    bool sprite_visible;
} sprite_config = {
    SPRITE_X_OFFSET,
    SPRITE_Y_OFFSET,
    SPRITE_WIDTH,
    SPRITE_HEIGHT,
    false,
    SPRITE_STATE_UNKNOWN,
    false
};

// Current animation state
static sprite_animation_t current_animation = {0};

// External references to bb_epaper display
#ifdef BB_EPAPER
extern bb_epaper bbep;
#endif

/**
 * @brief PNG draw callback for sprite rendering
 * Draws sprite at specific position on screen
 */
static int sprite_png_draw(PNGDRAW *pDraw) {
#ifdef BB_EPAPER
    int x;
    uint8_t uc = 0;
    uint8_t ucMask, src, *s, *d;
    int iPlane = *(int *)pDraw->pUser;

    // Get sprite position
    uint16_t sprite_x = sprite_config.x_offset;
    uint16_t sprite_y = sprite_config.y_offset;

    // Set drawing position for this line
    bbep.setXY(sprite_x, sprite_y + pDraw->y, sprite_x + sprite_config.width - 1, sprite_y + pDraw->y);

    if (iPlane == PNG_1_BIT || iPlane == PNG_1_BIT_INVERTED) {
        // 1-bit sprite rendering
        uint8_t ucInvert = (iPlane == PNG_1_BIT_INVERTED) ? 1 : 0;
        s = pDraw->pPixels;
        d = bbep.getCache();
        ucMask = 0x80;

        for (x = 0; x < pDraw->iWidth; x++) {
            if (ucMask == 0) {
                ucMask = 0x80;
                s++; // get next source byte
            }
            src = (*s & ucMask) ? 1 : 0;
            if (ucInvert) src ^= 1;
            uc <<= 1;
            uc |= src;
            if ((ucMask >>= 1) == 0) {
                ucMask = 0x80;
                s++;
            }
            if ((x & 7) == 7) {
                *d++ = uc;
                uc = 0;
            }
        }
        if ((x & 7) != 0) {
            uc <<= (7 - (x & 7));
            *d++ = uc;
        }
        bbep.writeData(d - (uint8_t *)bbep.getCache());
    } else {
        // 2-bit grayscale (not implemented for sprites - use 1-bit)
        SPRITE_LOG("2-bit sprites not supported, use 1-bit");
        return 0;
    }
#endif
    return 1;
}

/**
 * @brief Draw a sprite from PNG data
 */
static bool draw_sprite_from_data(const uint8_t* png_data, size_t png_size) {
    if (!png_data || png_size == 0) {
        SPRITE_LOG("Invalid PNG data");
        return false;
    }

#ifdef BB_EPAPER
    PNG *png = new PNG();

    if (!png) {
        SPRITE_LOG("Failed to allocate PNG decoder");
        return false;
    }

    // Check if PNG is valid
    int rc = png->openRAM((uint8_t*)png_data, png_size, sprite_png_draw);

    if (rc != PNG_SUCCESS) {
        SPRITE_LOG("Failed to open PNG: %d", rc);
        delete png;
        return false;
    }

    // Decode 1-bit sprite
    int iPlane = PNG_1_BIT;
    bbep.startWrite(PLANE_0);

    rc = png->decode(&iPlane, 0);
    png->close();
    delete png;

    if (rc != PNG_SUCCESS) {
        SPRITE_LOG("Failed to decode PNG: %d", rc);
        return false;
    }

    sprite_config.sprite_visible = true;
    return true;
#else
    SPRITE_LOG("BB_EPAPER not defined");
    return false;
#endif
}

// Simple 1-bit sprite data placeholders
// These will be replaced with actual PNG data
// For now, we draw simple shapes using the display functions

/**
 * @brief Draw a simple procedural sprite for testing
 * Draws basic shapes to represent each state
 */
static bool draw_procedural_sprite(sprite_state_t state) {
#ifdef BB_EPAPER
    uint16_t x = sprite_config.x_offset;
    uint16_t y = sprite_config.y_offset;
    uint16_t w = sprite_config.width;
    uint16_t h = sprite_config.height;

    // Clear sprite area first
    bbep.fillRect(x, y, w, h, 0); // White background

    // Draw different shapes based on state
    switch (state) {
        case SPRITE_STATE_IDLE:
            // Draw a simple sleeping face
            bbep.fillCircle(x + w/2, y + h/2, 40, 1); // Face outline
            bbep.fillCircle(x + w/2 - 15, y + h/2 - 10, 5, 1); // Left eye (closed)
            bbep.fillCircle(x + w/2 + 15, y + h/2 - 10, 5, 1); // Right eye (closed)
            bbep.fillCircle(x + w/2, y + h/2 + 15, 10, 0); // Mouth (white circle)
            break;

        case SPRITE_STATE_THINKING:
            // Face with thought bubble
            bbep.fillCircle(x + w/2, y + h/2, 40, 1); // Face
            bbep.fillCircle(x + w/2 - 15, y + h/2 - 10, 8, 1); // Left eye (open)
            bbep.fillCircle(x + w/2 + 15, y + h/2 - 10, 8, 1); // Right eye (open)
            bbep.fillRect(x + w/2 - 5, y + h/2 + 5, 10, 3, 1); // Mouth (flat)
            // Thought bubbles
            bbep.fillCircle(x + w/2 + 50, y + h/2 - 50, 8, 1);
            bbep.fillCircle(x + w/2 + 65, y + h/2 - 65, 12, 1);
            bbep.fillCircle(x + w/2 + 85, y + h/2 - 85, 18, 1);
            break;

        case SPRITE_STATE_TALKING:
            // Face with open mouth
            bbep.fillCircle(x + w/2, y + h/2, 40, 1); // Face
            bbep.fillCircle(x + w/2 - 15, y + h/2 - 10, 8, 1); // Left eye
            bbep.fillCircle(x + w/2 + 15, y + h/2 - 10, 8, 1); // Right eye
            bbep.fillCircle(x + w/2, y + h/2 + 15, 12, 0); // Mouth (open, white)
            bbep.fillCircle(x + w/2, y + h/2 + 15, 8, 1); // Tongue/inner mouth
            // Speech wave
            bbep.fillRect(x + w/2 + 45, y + h/2 - 5, 10, 5, 1);
            bbep.fillRect(x + w/2 + 60, y + h/2 - 10, 10, 10, 1);
            bbep.fillRect(x + w/2 + 75, y + h/2 - 15, 10, 15, 1);
            break;

        case SPRITE_STATE_EXCITED:
            // Happy excited face
            bbep.fillCircle(x + w/2, y + h/2, 40, 1); // Face
            // Star eyes
            for (int i = 0; i < 5; i++) {
                float angle = i * 72 * 0.01745;
                int sx = x + w/2 - 15 + cos(angle) * 12;
                int sy = y + h/2 - 10 + sin(angle) * 12;
                bbep.fillPixel(sx, sy, 1);
            }
            // Big smile
            bbep.fillCircle(x + w/2, y + h/2 + 10, 15, 0);
            bbep.fillCircle(x + w/2, y + h/2 + 5, 10, 1);
            // Excitement marks
            bbep.drawLine(x + w/2 - 50, y + h/2 - 50, x + w/2 - 40, y + h/2 - 60, 1);
            bbep.drawLine(x + w/2 - 50, y + h/2 - 60, x + w/2 - 40, y + h/2 - 50, 1);
            bbep.drawLine(x + w/2 + 50, y + h/2 - 50, x + w/2 + 40, y + h/2 - 60, 1);
            bbep.drawLine(x + w/2 + 50, y + h/2 - 60, x + w/2 + 40, y + h/2 - 50, 1);
            break;

        case SPRITE_STATE_SLEEPING:
            // Sleeping face with Z's
            bbep.fillCircle(x + w/2, y + h/2, 40, 1); // Face
            bbep.fillCircle(x + w/2 - 15, y + h/2 - 10, 8, 1); // Closed left eye
            bbep.fillCircle(x + w/2 + 15, y + h/2 - 10, 8, 1); // Closed right eye
            bbep.fillRect(x + w/2 - 8, y + h/2 + 10, 16, 3, 1); // Mouth
            // Z's
            bbep.drawChar(x + w/2 + 50, y + h/2 - 40, 'Z', 1, 0);
            bbep.drawChar(x + w/2 + 65, y + h/2 - 55, 'Z', 1, 0);
            bbep.drawChar(x + w/2 + 80, y + h/2 - 70, 'Z', 1, 0);
            break;

        case SPRITE_STATE_ERROR:
            // Error face with X eyes
            bbep.fillCircle(x + w/2, y + h/2, 40, 1); // Face
            // X eyes
            bbep.drawLine(x + w/2 - 20, y + h/2 - 15, x + w/2 - 10, y + h/2 - 5, 1);
            bbep.drawLine(x + w/2 - 20, y + h/2 - 5, x + w/2 - 10, y + h/2 - 15, 1);
            bbep.drawLine(x + w/2 + 10, y + h/2 - 15, x + w/2 + 20, y + h/2 - 5, 1);
            bbep.drawLine(x + w/2 + 10, y + h/2 - 5, x + w/2 + 20, y + h/2 - 15, 1);
            // Sad mouth
            bbep.drawArc(x + w/2, y + h/2 + 5, 15, 10, 0, 180, 1);
            // Sweat drop
            bbep.fillCircle(x + w/2 + 45, y + h/2 - 20, 8, 1);
            break;

        case SPRITE_STATE_ALERT:
            // Alert face with wide eyes
            bbep.fillCircle(x + w/2, y + h/2, 40, 1); // Face
            bbep.fillCircle(x + w/2 - 15, y + h/2 - 10, 12, 0); // Left eye (wide)
            bbep.fillCircle(x + w/2 - 15, y + h/2 - 10, 8, 1); // Pupil
            bbep.fillCircle(x + w/2 + 15, y + h/2 - 10, 12, 0); // Right eye (wide)
            bbep.fillCircle(x + w/2 + 15, y + h/2 - 10, 8, 1); // Pupil
            bbep.fillRect(x + w/2 - 8, y + h/2 + 10, 16, 3, 1); // Mouth (surprised)
            // Alert marks
            bbep.drawLine(x + w/2 - 55, y + h/2 - 40, x + w/2 - 45, y + h/2 - 55, 1);
            bbep.drawLine(x + w/2 - 55, y + h/2 - 55, x + w/2 - 45, y + h/2 - 40, 1);
            bbep.drawLine(x + w/2 + 45, y + h/2 - 55, x + w/2 + 55, y + h/2 - 55, 1);
            bbep.drawLine(x + w/2 + 45, y + h/2 - 55, x + w/2 + 45, y + h/2 - 40, 1);
            break;

        case SPRITE_STATE_WORKING:
            // Working face with gear
            bbep.fillCircle(x + w/2, y + h/2, 40, 1); // Face
            bbep.fillCircle(x + w/2 - 15, y + h/2 - 10, 5, 0); // Eye with focus
            bbep.fillCircle(x + w/2 + 15, y + h/2 - 10, 5, 0); // Eye with focus
            bbep.fillRect(x + w/2 - 5, y + h/2 + 5, 10, 3, 1); // Determined mouth
            // Gear icon
            int gx = x + w/2 + 60, gy = y + h/2 - 20;
            bbep.fillCircle(gx, gy, 15, 1);
            bbep.fillCircle(gx, gy, 8, 0);
            for (int i = 0; i < 8; i++) {
                float angle = i * 45 * 0.01745;
                bbep.fillCircle(gx + cos(angle) * 18, gy + sin(angle) * 18, 4, 1);
            }
            break;

        default:
            SPRITE_LOG("Unknown sprite state: %d", state);
            return false;
    }

    sprite_config.sprite_visible = true;
    return true;
#else
    SPRITE_LOG("BB_EPAPER not defined");
    return false;
#endif
}

// Public API implementation

bool sprite_renderer_init(void) {
    if (sprite_config.initialized) {
        return true;
    }

    SPRITE_LOG("Initializing sprite renderer...");
    SPRITE_LOG("Position: (%d, %d), Size: %dx%d",
               sprite_config.x_offset, sprite_config.y_offset,
               sprite_config.width, sprite_config.height);

    sprite_config.initialized = true;
    sprite_config.current_state = SPRITE_STATE_UNKNOWN;
    sprite_config.sprite_visible = false;

    return true;
}

bool sprite_display_state(sprite_state_t state) {
    if (!sprite_config.initialized) {
        SPRITE_LOG("Sprite renderer not initialized");
        return false;
    }

    if (state == sprite_config.current_state && sprite_config.sprite_visible) {
        // Already displaying this state
        return true;
    }

    SPRITE_LOG("Displaying sprite state: %d", state);
    sprite_config.current_state = state;

    // Get sprite data from embedded table
    const sprite_data_t* sprite_data = get_sprite_data(state);
    if (!sprite_data) {
        SPRITE_LOG("No sprite data for state: %d", state);
        // Fall back to procedural sprite
        return draw_procedural_sprite(state);
    }

    SPRITE_LOG("Using embedded PNG sprite: %d bytes", sprite_data->size);
    return draw_sprite_from_data(sprite_data->data, sprite_data->size);
}

bool sprite_display_with_message(sprite_state_t state, const char* message) {
    if (!sprite_config.initialized) {
        SPRITE_LOG("Sprite renderer not initialized");
        return false;
    }

    // Display sprite first
    if (!sprite_display_state(state)) {
        return false;
    }

    // TODO: Draw text message below sprite
    if (message && strlen(message) > 0) {
        SPRITE_LOG("Message: %s", message);
        // Text rendering would go here
    }

    return true;
}

bool sprite_update_animation(void) {
    if (!current_animation.is_animating) {
        return false;
    }

    unsigned long now = millis();
    if (now - current_animation.last_frame_time >=
        current_animation.frames[current_animation.current_frame].duration_ms) {

        current_animation.current_frame =
            (current_animation.current_frame + 1) % current_animation.frame_count;
        current_animation.last_frame_time = now;

        // TODO: Draw next frame
        return true;
    }

    return false;
}

void sprite_clear(void) {
#ifdef BB_EPAPER
    if (sprite_config.sprite_visible) {
        bbep.fillRect(sprite_config.x_offset, sprite_config.y_offset,
                      sprite_config.width, sprite_config.height, 0);
        sprite_config.sprite_visible = false;
        sprite_config.current_state = SPRITE_STATE_UNKNOWN;
    }
#endif
}

bool sprite_is_ready(void) {
    return sprite_config.initialized;
}

void sprite_set_position(uint16_t x, uint16_t y) {
    sprite_config.x_offset = x;
    sprite_config.y_offset = y;
}

void sprite_set_size(uint16_t width, uint16_t height) {
    sprite_config.width = width;
    sprite_config.height = height;
}

#endif // ECHO_DISABLE_MQTT_SPRITES
