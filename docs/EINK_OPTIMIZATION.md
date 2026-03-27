# E-Ink Display Optimization Strategy for Xteink X4

## Executive Summary

Based on first-principles analysis of electrophoretic displays, SSD1677 controller capabilities, and the TRMNL firmware, this document outlines a comprehensive optimization strategy to improve refresh speed, reduce power consumption, and enhance display quality.

## 1. First Principles: E-Ink Physics

### 1.1 Electrophoretic Display Fundamentals

**Physical Mechanism:**
- Charged pigment particles (TiO₂ for white, carbon black for black) suspended in dielectric fluid
- Electric field causes electrophoretic migration to opposite electrodes
- Bistable: particles remain in position after field removal
- Microcapsules (~50-100µm) contain suspension medium

**Key Physical Parameters:**
| Parameter | Typical Value | Optimization Impact |
|-----------|---------------|---------------------|
| Particle mobility | 10-100 µm/s per V/µm | Minimum refresh time |
| Temperature coefficient | ~2%/°C | Waveform adaptation needed |
| Operating temp range | 0-50°C | Performance varies |
| Viscosity | Decreases with heat | Faster refresh at high temp |

### 1.2 Refresh Time Fundamental Limits

**Particle Migration Time:**
```
t = d / (μE)
where d = distance, μ = mobility, E = field strength
```

For 100µm particle movement at 3V/100µm:
- At 25°C: ~500ms minimum
- At 40°C: ~300ms minimum

**Implication:** Sub-300ms full refresh is physically impossible. Target should be:
- Partial: 300-500ms (achievable)
- Full: 2-3s (vs current 4s)

## 2. SSD1677 Controller Deep Dive

### 2.1 Waveform LUT Structure

The SSD1677 has a **40-phase programmable LUT**:

```
LUT Structure:
- Bytes 0-49: VS[nX-LUTm] - Voltage levels for each phase
- Bytes 50-97: TP[nX] - Phase durations (0-255 frames)
- Bytes 98-99: RP[n] - Repeat counts (1-256)
- Bytes 100-104: Frame rate settings
- Bytes 105-109: Voltage levels
```

**Voltage Levels:**
| Code | Source Voltage | VCOM Voltage |
|------|----------------|---------------|
| 00 | VSS (0V) | DCVCOM |
| 01 | VSH1 (9-17V) | VSH1+DCVCOM |
| 10 | VSL (-9 to -17V) | VSL+DCVCOM |
| 11 | VSH2 (2.4-17V) | N/A |

### 2.2 Key Registers

| Command | Function | Optimization Value |
|---------|-----------|-------------------|
| 0x32 | Write LUT | Custom fast refresh LUT |
| 0x18 | Temperature control | Enable internal sensor |
| 0x1A | Write temperature | Auto-read for compensation |
| 0x22 | Display update control 2 | Optimize stage timing |
| 0x03 | Gate voltage | 15-20V (adjust for temp) |
| 0x04 | Source voltage | VSH1/VSH2/VSL tuning |
| 0x2C | VCOM control | Auto-sense mode |

## 3. Current Firmware Issues

### 3.1 Performance Bottlenecks

```cpp
// CURRENT: display.cpp:105
bbep.initIO(EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN, EPD_CS_PIN, EPD_MOSI_PIN, EPD_SCK_PIN, 8000000);
//                                                                            ^^^^^^^
// Problem: Only 8MHz SPI, SSD1677 supports up to 20MHz
```

**Issue 1: Low SPI Clock**
- Current: 8MHz
- Maximum: 20MHz (SSD1677 spec)
- Impact: 2.5x slower data transfer

**Issue 2: No Custom LUT**
```cpp
// CURRENT: Using default bb_epaper LUT
bbep.initPanel(BB_PANEL_EPDIY_V7_16);
```
- Default LUT optimized for compatibility, not speed
- No temperature-aware waveforms

**Issue 3: No Temperature Compensation**
- SSD1677 has ±2°C internal sensor
- Firmware doesn't use it
- Waveforms not optimized for current temperature

**Issue 4: Conservative Refresh Policy**
- Full refresh every 8 partials
- May be excessive for high-quality waveforms

### 3.2 Memory Constraints (ESP32-C3)

```
Total RAM: 400KB
Available: ~384KB (after 16KB IRAM mapping)
Frame buffer (800×480 1-bit): 48KB
Code: ~150KB
Free: ~180KB for stack, heap, WiFi buffers
```

**Optimization Implication:**
- Can afford DMA buffers
- Can cache multiple frames for smart refresh
- No need for complex streaming algorithms

## 4. Optimization Strategy

### 4.1 SPI/DMA Optimization

**Target:** 20MHz SPI with DMA

```cpp
// OPTIMIZED: ESP32-C3 specific SPI DMA
#define EPD_SPI_FREQ 20000000  // 20MHz max for SSD1677

// Use DMA for transfer
spi_device_acquire(bus, SPI_MAX_DMA_LEN);
spi_master_transmit_dma(handle, dma_tx, dma_size);
```

**Expected Gain:**
- Transfer time: 384KB / 20MHz = ~19ms (vs 48ms at 8MHz)
- 2.5x improvement in data transfer

### 4.2 Custom Fast Refresh LUT

Based on research from Ben Krasnow's custom waveforms:

```cpp
// Fast partial refresh LUT for SSD1677
// Optimized for 300-500ms refresh with minimal ghosting

const uint8_t FAST_PARTIAL_LUT[] = {
    // Phase durations (shorter than default)
    0x02, 0x02, 0x01, 0x01,  // TP[0A-D] - Group 0
    0x02, 0x02, 0x01, 0x01,  // TP[1A-D] - Group 1
    // ... (40 phases total)

    // Voltage levels - aggressive for speed
    0x00, 0x01, 0x10, 0x11,  // VS[0A-L0] through VS[0D-L0]
    // ... optimized voltage transitions
};
```

**Implementation:**
1. Read internal temperature
2. Select LUT based on temperature band
3. Load custom LUT via 0x32 command
4. Enable fast refresh mode

### 4.3 Temperature Compensation

```cpp
void optimize_for_temperature() {
    // Read internal temperature
    uint16_t temp = read_internal_temp_ssd1677();

    // Select waveform based on temperature band
    if (temp < 15) {
        load_cold_waveform();  // Slower, more thorough
    } else if (temp < 30) {
        load_normal_waveform();  // Balanced
    } else {
        load_hot_waveform();    // Faster, less movement needed
    }
}
```

**Temperature Bands:**
| Range | Strategy | LUT Selection |
|-------|----------|---------------|
| <15°C | Conservative | Long phase durations |
| 15-25°C | Balanced | Standard timing |
| 25-40°C | Aggressive | Short phases, fast transitions |
| >40°C | Very aggressive | Minimum phase times |

### 4.4 Adaptive Refresh Strategy

```cpp
// Smart refresh based on content change
struct ImageDelta {
    float changed_pixels;
    bool has_text;
    bool has_graphics;
};

int determine_refresh_mode(ImageDelta delta) {
    // No change = skip refresh entirely
    if (delta.changed_pixels < 0.001) {
        return REFRESH_NONE;
    }

    // Small text change = fast partial
    if (delta.changed_pixels < 0.01 && delta.has_text) {
        return REFRESH_FAST_PARTIAL;
    }

    // Significant change = full refresh
    if (delta.changed_pixels > 0.1) {
        return REFRESH_FULL;
    }

    // Default = partial
    return REFRESH_PARTIAL;
}
```

**Refresh Counters:**
- Fast partial: Every 15 updates (vs current 8)
- Full refresh: Based on ghosting detection
- Skip refresh: When content unchanged

### 4.5 Power Optimization

**Deep Sleep Strategy:**
```cpp
// Calculate optimal sleep time based on:
// 1. Battery voltage
// 2. Time since last full refresh
// 3. Temperature (battery degrades faster at temp)

uint32_t calculate_sleep_time() {
    float battery = read_battery_voltage();
    float temp = read_temperature();

    // Base interval
    uint32_t interval = preferences_refresh_rate();

    // Adjust for temperature
    if (temp < 10 || temp > 35) {
        interval *= 0.8;  // Check more often at temperature extremes
    }

    // Adjust for battery
    if (battery < 3.5) {
        interval *= 0.5;  // Low battery warning mode
    }

    return interval;
}
```

**Voltage Optimization:**
- Switch to 1.8V operation when battery < 3.7V
- Reduce SPI clock when battery < 3.5V
- Enable light sleep only when battery > 3.2V

### 4.6 Display Quality Optimization

**Dithering:**
- Implement Floyd-Steinberg for grayscale images
- Use ordered dither for faster rendering
- Cache dither patterns in RAM

**Ghosting Reduction:**
```cpp
// Active ghosting detection and mitigation
void detect_and_fix_ghosting() {
    // Read current frame
    uint8_t *current = read_framebuffer();

    // Compare with previous frame
    float ghost_score = calculate_ghost_score(current, previous_frame);

    if (ghost_score > 0.05) {
        // Ghosting detected
        load_compensation_waveform();
        force_full_refresh();
    }
}
```

## 5. Implementation Priority

### Phase 1: Quick Wins (1-2 days)
1. Increase SPI clock to 20MHz
2. Implement temperature-based LUT selection
3. Optimize refresh counter logic

### Phase 2: Medium Effort (3-5 days)
1. Implement custom fast refresh LUT
2. Add DMA support for SPI transfers
3. Implement delta-based refresh decisions

### Phase 3: Advanced (1-2 weeks)
1. Adaptive refresh with content analysis
2. Advanced ghosting detection and compensation
3. Power-aware operation modes

## 6. Expected Performance Gains

| Metric | Current | Optimized | Improvement |
|--------|---------|-----------|-------------|
| Partial refresh | ~760ms | ~400ms | 1.9x faster |
| Full refresh | 4000ms | ~2500ms | 1.6x faster |
| Data transfer | 48ms | 19ms | 2.5x faster |
| Battery life (daily) | ~120 days | ~150 days | 25% longer |
| Ghosting artifacts | Medium | Low | Better quality |

## 7. Testing Strategy

### 7.1 Performance Tests
- Measure actual refresh times with oscilloscope
- Power consumption profiling
- Temperature chamber testing (0-50°C)

### 7.2 Quality Tests
- Ghosting measurement (contrast retention)
- Long-term stability (1000+ refresh cycles)
- Battery life measurement

## 8. References

- [SSD1677 Datasheet](https://files.waveshare.com/upload/2/2a/SSD1677_1.0.pdf)
- [4.26" E-Paper Manual](https://files.waveshare.com/wiki/4.26inch-e-Paper-HAT/4.26inch_e-Paper_User_Manual.pdf)
- [Ben Krasnow: Fast Partial Refresh](https://benkrasnow.blogspot.com/2017/10/fast-partial-refresh-on-42-e-paper.html)
- [ESP32-C3 Speed Optimization](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/performance/speed.html)
- [bb_epaper Library](https://github.com/bitbank2/bb_epaper)
