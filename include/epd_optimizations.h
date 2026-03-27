/**
 * @file    epd_optimizations.h
 * @brief   E-Paper Display Optimizations for Xteink X4 (SSD1677)
 *
 * Based on first-principles analysis of electrophoretic displays,
 * SSD1677 controller capabilities, and custom waveform research.
 *
 * Key Optimizations:
 * - 20MHz SPI (vs 8MHz default)
 * - Temperature-adaptive waveforms
 * - Smart refresh strategy
 * - Reduced ghosting
 *
 * References:
 * - SSD1677 Datasheet: Solomon Systech
 * - Ben Krasnow's custom waveforms
 * - ESP32-C3 optimization guides
 */

#ifndef EPD_OPTIMIZATIONS_H
#define EPD_OPTIMIZATIONS_H

#include <Arduino.h>
#include <bb_epaper.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

// SPI Configuration
#ifndef EPD_SPI_FREQ
#define EPD_SPI_FREQ 20000000  // 20MHz max for SSD1677 (vs 8MHz default)
#endif

// Temperature Compensation
#define TEMP_COLD_THRESHOLD   15  // Below 15°C
#define TEMP_NORMAL_MIN       15  // 15-25°C normal range
#define TEMP_NORMAL_MAX       30  // Up to 30°C
#define TEMP_HOT_THRESHOLD    40  // Above 40°C

// Refresh Strategy
#define FAST_PARTIAL_MAX_COUNT  15  // Full refresh every 15 partials (vs 8)
#define DELTA_THRESHOLD         0.001  // Skip refresh if <0.1% change

// Power Management
#define LOW_BATTERY_THRESHOLD    3.5f  // Reduce clock when battery < 3.5V
#define LOW_VOLTAGE_SPI_FREQ    10000000  // 10MHz at low battery

// ============================================================================
// TEMPERATURE COMPENSATION
// ============================================================================

/**
 * @brief Temperature band for waveform selection
 */
typedef enum {
    TEMP_BAND_COLD,     // < 15°C
    TEMP_BAND_NORMAL,   // 15-30°C
    TEMP_BAND_WARM,     // 30-40°C
    TEMP_BAND_HOT       // > 40°C
} temp_band_t;

/**
 * @brief Get current temperature band
 * @return Temperature band based on internal or external sensor
 */
temp_band_t get_temperature_band();

/**
 * @brief Load optimized waveform for current temperature
 * @param band Current temperature band
 */
void load_temperature_optimized_waveform(temp_band_t band);

/**
 * @brief Read internal temperature from SSD1677
 * @return Temperature in Celsius, or 999 if read failed
 */
float read_ssd1677_temperature();

// ============================================================================
// REFRESH MODE OPTIMIZATION
// ============================================================================

// Define refresh mode constants (extending bb_epaper modes)
#define REFRESH_NONE -1  // Skip refresh entirely (content unchanged)

/**
 * @brief Calculate image delta between current and previous frame
 * @param current Current frame buffer
 * @param previous Previous frame buffer
 * @param width Frame width
 * @param height Frame height
 * @return Fraction of pixels that changed (0.0 to 1.0)
 */
float calculate_image_delta(uint8_t *current, uint8_t *previous,
                            uint16_t width, uint16_t height);

/**
 * @brief Determine optimal refresh mode based on content change
 * @param delta Fraction of pixels that changed
 * @param has_text True if content includes text
 * @param has_graphics True if content includes graphics
 * @return Recommended refresh mode
 */
int determine_optimal_refresh_mode(float delta, bool has_text, bool has_graphics);

/**
 * @brief Check if refresh is needed at all (content unchanged)
 * @param current Current frame buffer
 * @param previous Previous frame buffer
 * @param size Frame buffer size in bytes
 * @return True if content is unchanged
 */
bool is_content_unchanged(uint8_t *current, uint8_t *previous, size_t size);

// ============================================================================
// POWER OPTIMIZATION
// ============================================================================

/**
 * @brief Get optimal SPI frequency based on battery voltage
 * @return Recommended SPI frequency in Hz
 */
uint32_t get_power_optimized_spi_freq();

/**
 * @brief Calculate optimal sleep time based on battery and temperature
 * @param base_interval Base refresh interval from preferences
 * @return Optimized sleep time in seconds
 */
uint32_t calculate_optimal_sleep_time(uint32_t base_interval);

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize EPD optimizations
 * Call this after display initialization to enable all optimizations
 */
void epd_optimizations_init();

/**
 * @brief Apply SPI optimizations
 * Increases SPI clock to maximum supported frequency
 */
void apply_spi_optimizations();

/**
 * @brief Enable temperature compensation
 * Sets up temperature sensor and waveform selection
 */
void enable_temperature_compensation();

/**
 * @brief Configure adaptive refresh strategy
 * Sets up smart refresh based on content analysis
 */
void configure_adaptive_refresh();

// ============================================================================
// WAVEFORM DATA (Custom Fast Refresh LUT)
// ============================================================================

extern const uint8_t FAST_PARTIAL_WAVEFORM[];
extern const uint16_t FAST_PARTIAL_WAVEFORM_SIZE;

// ============================================================================
// DIAGNOSTICS
// ============================================================================

/**
 * @brief Print performance metrics
 * Call after refresh operation to show timing data
 */
void print_refresh_metrics();

/**
 * @brief Get current refresh statistics
 */
struct refresh_stats_t {
    uint32_t total_refreshes;
    uint32_t partial_refreshes;
    uint32_t full_refreshes;
    uint32_t skipped_refreshes;
    float average_refresh_time_ms;
    float ghosting_score;
};

extern refresh_stats_t g_refresh_stats;

#endif // EPD_OPTIMIZATIONS_H
