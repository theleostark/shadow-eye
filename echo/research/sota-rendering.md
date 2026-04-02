# SOTA Research: E-Paper Display Rendering Techniques (2024-2026)

**Target hardware:** ESP32-C3 + SSD1677 controller, 800x480 panel, 4-gray capable  
**Researched:** 2026-04-02  
**Status:** Active reference document

---

## 1. Waveform Optimization & Custom LUT Generation

### 1.1 SSD1677 LUT Architecture

The SSD1677 uses a programmable Look-Up Table (LUT) to define voltage sequencing across refresh phases. The custom LUT is 111-112 bytes:

| Byte Range | Function | Detail |
|------------|----------|--------|
| 0-49 | VS waveforms | 5 groups x 10 bytes; voltage levels per phase |
| 50-99 | TP timing + repeat | 10 groups x 5 bytes; phase durations |
| 100-104 | Frame rate config | Controls update cadence |
| 105-109 | Voltage rails | VGH, VSH1, VSH2, VSL, VCOM |
| 110 | Reserved | -- |

Voltage levels encode particle movement direction:
- **VSH1** (medium positive): moderate lightening
- **VSH2** (strong positive): drives white
- **VSL** (strong negative): drives black
- **Hi-Z** (floating): no action

Key registers for LUT control:
- `0x32`: Write custom LUT (the core register)
- `0x22`: Display Update Control 2 -- orchestrates the refresh pipeline
- `0x20`: Master Activation -- triggers the actual update
- `0x18`: Temperature sensor selection (`0x80` = internal)

**Display Update Control 2 (0x22) patterns for different modes:**
- `0xF4` / `0xF7`: Initial full refresh (loads temp, LUT, full pipeline)
- `0x34`: Subsequent full refresh (skips temp reload)
- `0xFC`: Partial refresh (localized update)
- `0x0C`: Fast refresh with pre-loaded LUT (bypasses waveform reload)

Source: [papyrix-reader SSD1677 driver docs](https://github.com/bigbag/papyrix-reader/blob/main/docs/ssd1677-driver.md), [SSD1677 datasheet](https://cursedhardware.github.io/epd-driver-ic/SSD1677.pdf)

### 1.2 How reMarkable Achieves Low Latency

reMarkable tablets achieve ~55ms pen-to-ink latency through:

1. **Custom waveform tuning** -- aggressive DU (Direct Update) waveforms with minimal phases
2. **Region-localized updates** -- only the pen stroke bounding box refreshes
3. **No flash requirement** -- DU mode is inherently non-flashing for B/W transitions
4. **Hardware EPDC** -- dedicated e-paper display controller silicon, not software-driven SPI like SSD1677

The reMarkable Paper Pro uses "smart dithering" combined with partial refresh to avoid full black-flash cycles. Their CANVAS display technology likely includes panel-level waveform tuning co-developed with the panel manufacturer.

**Implication for SSD1677:** We cannot match reMarkable's latency (they have dedicated EPDC silicon), but we can adopt their approach of aggressive DU-mode waveforms for content that is purely B/W.

Source: [Stuff.tv reMarkable review](https://www.stuff.tv/news/remarkable-tablet-boosts-e-ink-speedy-new-heights/)

### 1.3 Open-Source Waveform Tools

**EPDiy (vroland/epdiy):**
- The leading open-source e-paper driver for parallel panels on ESP32/ESP32-S3
- Uses 2-bit per-pixel action codes: `00`=no-op, `01`=darken, `10`=lighten, `11`=no-op
- V7 uses static timing (all phases same duration), matching vendor waveform assumptions
- Waveform LUT is a lookup table mapping (source_gray, target_gray) to action sequences
- Timing is controlled by CKV (clock vertical) high-time in 10s of microseconds
- **Key limitation:** V7 currently ignores per-phase timing in waveforms, requiring vendor waveforms
- Waveform timings were "made up through experimentation" -- no formal derivation tool exists

Source: [epdiy GitHub](https://github.com/vroland/epdiy), [epdiy pixel driving wiki](https://github.com/vroland/epdiy/wiki/How-pixels-are-driven-in-a-parallel-epaper-with-epdiy), [epdiy waveform timings](https://github.com/vroland/epdiy/wiki/Waveform-timings-for-epdiy)

**LUT-Playground (nlimper/LUT-playground):**
- Web-based interface for experimenting with e-paper waveforms
- Currently supports SSD1619A (not SSD1677, but same LUT concepts)
- Allows recording slow-motion video of e-paper updates synchronized with LED display showing group/phase timing
- Useful for iterating on custom LUT values visually

Source: [LUT-playground GitHub](https://github.com/nlimper/LUT-playground)

**Modos Caster/Glider:**
- FPGA-based open-source EPDC (electrophoretic display controller)
- Defines an Interchangeable Waveform Format (IWF) -- human-readable INI + CSV
- Video processing pipeline: 5 stages at 4 pixels/clock
  - Stage 2: dither input to 1-bit and 4-bit
  - Stage 3: waveform lookup for 16-level grayscale
  - Stage 4: pixel state and voltage selection
  - Stage 5: state writeback and voltage output
- Duplicates waveform RAM and pixel logic 2-4x for throughput
- Error diffusion dithering unit is daisy-chained across pixel lanes

Source: [Modos Caster GitHub](https://github.com/Modos-Labs/Caster), [Modos Glider GitHub](https://github.com/Modos-Labs/Glider)

**FastEPD (bitbank2/FastEPD):**
- New library from bitbank2 (same author as bb_epaper used in TRMNL firmware)
- Provides standardized human-readable grayscale waveform matrix format
- Waveforms expressed as 38x16 matrices of `{0, 1, 2}` values (no-op, darken, lighten)
- Enables easy hardware switching between SPI and parallel panels
- Directly relevant to our SSD1677 implementation

Source: [fasani.de on driving eink displays](https://fasani.de/2025/01/03/on-driving-eink-displays/)

**h0rv/ssd1677 (Rust):**
- `no_std` Rust driver for SSD1677, embedded-hal v1.0 compatible
- Supports custom 112-byte LUT loading
- Full and partial refresh modes
- Rotation support (0/90/180/270)
- Good reference for clean SSD1677 register-level implementation

Source: [h0rv/ssd1677 GitHub](https://github.com/h0rv/ssd1677)

### 1.4 PineNote Kernel Waveform Work

The PineNote uses a Rockchip RK3566 with an `rockchip_ebc` kernel driver for its e-paper panel:
- Waveform data stored in `/usr/lib/firmware/rockchip/ebc.wbf`
- Individual panels get unique waveform data flashed per-device to account for manufacturing variation
- Multiple kernel forks improving display responsiveness:
  - Per-pixel scheduling for smarter update ordering
  - Automatic redraw of pixels that need correction
- WBF (Waveform Binary Format) is the proprietary E Ink format; the community reverse-engineers these

Source: [PineNote Development wiki](https://wiki.pine64.org/wiki/PineNote_Development), [m-weigand/linux PineNote fork](https://github.com/m-weigand/linux)

### 1.5 Practical LUT Strategy for SSD1677

Based on research, here is the recommended approach:

1. **Start with default OTP waveform** as baseline
2. **Read internal temperature** via register `0x18`/`0x1A` before each refresh
3. **Maintain 3-4 custom LUT tables** indexed by temperature band:
   - Cold (<15C): longer phase durations (+20-30%), conservative voltages
   - Normal (15-30C): balanced baseline
   - Warm (30-40C): shorter phases (-10-15%), aggressive voltages
   - Hot (>40C): minimum phase times
4. **Load LUT via `0x32`** command before each refresh, selected by temperature
5. **Use `0x22=0x0C`** (fast refresh with pre-loaded LUT) for subsequent updates to skip waveform reload latency
6. **Experimentally tune** using slow-motion camera + oscilloscope to measure actual particle response

---

## 2. Dithering Algorithms for 1-bit and 2-bit E-Paper

### 2.1 Algorithm Comparison

| Algorithm | Type | Quality | Speed | Memory | Artifacts | Best For |
|-----------|------|---------|-------|--------|-----------|----------|
| **Floyd-Steinberg** | Error diffusion | Good | O(n) | 2 rows | Directional worms | General purpose |
| **Atkinson** | Error diffusion | Very good | O(n) | 2 rows | Less spread, cleaner | E-paper B/W |
| **Blue Noise** | Threshold (precomputed) | Excellent | O(1)/pixel | Texture (4-64KB) | None visible | Best visual quality |
| **Ordered/Bayer** | Threshold (matrix) | Moderate | O(1)/pixel | Matrix (16-256B) | Grid patterns | Fast, deterministic |
| **Riemersma** | Error diffusion (Hilbert) | Very good | O(n) | Small buffer | No directional bias | Artifact-free dither |
| **Sierra** | Error diffusion | Good | O(n) | 3 rows | Similar to F-S | Alternative to F-S |

### 2.2 Floyd-Steinberg

The classic error diffusion algorithm. Distributes quantization error to 4 neighboring pixels:

```
        X   7/16
  3/16  5/16  1/16
```

**Pros:** Well-understood, good quality, widely implemented (bb_epaper uses it).
**Cons:** Produces directional "worm" artifacts. Propagates errors globally -- a single bad pixel affects everything downstream. Requires sequential processing (hard to parallelize).

### 2.3 Atkinson Dithering (Recommended for E-Paper)

Developed by Bill Atkinson for the original Macintosh. Diffuses only 3/4 of the quantization error (not 4/4 like F-S), distributing across 6 neighbors:

```
        X   1/8  1/8
  1/8  1/8  1/8
        1/8
```

**Why Atkinson is superior for e-paper:**
- **Less error propagation** = cleaner result with more contrast
- **Preserves highlights and shadows** better than F-S (intentionally "loses" 1/4 of error)
- **Faster** than Jarvis/Sierra (fewer neighbors)
- **Mac heritage** -- designed for 1-bit monochrome displays, which is exactly our use case
- NXP's own e-ink dithering documentation chose Atkinson "for its simplicity and better results especially for Eink black/white display cases"

**Implementation note for ESP32-C3:** Atkinson requires only a 2-row line buffer (same as F-S), fitting comfortably in the ~180KB free RAM.

Source: [NXP Dithering for E-Ink](https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/Dithering-Implementation-for-Eink-Display-Panel/ta-p/1100219)

### 2.4 Blue Noise Dithering (Best Visual Quality)

Blue noise dithering uses a precomputed noise texture where high-frequency components dominate, eliminating visible patterns. Implementation:

```
output[x][y] = (input[x][y] > blue_noise_texture[x % W][y % H]) ? WHITE : BLACK;
```

**Why blue noise is perceptually superior:**
- Removes all low-frequency "chunkiness" -- only high-frequency noise remains
- Human visual system is less sensitive to high-frequency noise
- No directional artifacts (unlike error diffusion)
- No grid patterns (unlike ordered dither)
- Result looks like natural film grain

**Practical implementation for ESP32-C3:**
- Precompute a 64x64 or 128x128 blue noise texture (4KB or 16KB)
- Store in flash (PROGMEM), tile across the 800x480 display
- O(1) per pixel -- just a comparison, no error tracking
- **Fully parallelizable** -- each pixel is independent
- Free textures available: [momentsingraphics.de/BlueNoise.html](https://momentsingraphics.de/BlueNoise.html)

**Tradeoff:** Slightly less sharp edges than error diffusion on text. Best for photographic/gradient content.

Source: [Bart Wronski on blue noise dithering](https://bartwronski.com/2016/10/30/dithering-part-two-golden-ratio-sequence-blue-noise-and-highpass-and-remap/), [Ulichney "Dithering with blue noise"](https://www.semanticscholar.org/paper/Dithering-with-blue-noise-Ulichney/6e7d576240cb6dfa3ab72504be99c9743eaa7612)

### 2.5 Ordered/Bayer Dithering

Uses a precomputed threshold matrix (typically 4x4 or 8x8) tiled across the image:

```
Bayer 4x4:
 0  8  2  10
12  4  14  6
 3  11  1  9
15  7  13  5
```

**Pros:**
- O(1) per pixel, O(n) total -- fastest possible
- Deterministic -- same input always produces same output
- Zero memory overhead beyond the tiny matrix (16-64 bytes)
- Can be computed with bitwise ops only (no division)
- **Excellent for 2-bit (4-gray) output** -- natural multi-level thresholding

**Cons:** Produces visible grid patterns at low DPI. Acceptable for UI elements but poor for photographs.

**Relevance to SSD1677 4-gray mode:** Bayer dithering maps naturally to 2-bit output. With 4 gray levels, a 4x4 Bayer matrix produces 16 effective tone levels. This is the most practical dithering for real-time 4-gray rendering on ESP32-C3.

### 2.6 Riemersma Dithering (Hilbert Curve)

Traverses the image along a Hilbert space-filling curve, applying error diffusion along the path:

**Key advantage:** The Hilbert curve visits every pixel exactly once while maintaining spatial locality -- neighbors on the curve are neighbors in the image. This eliminates the directional artifacts of row-by-row error diffusion.

**Pros:**
- No directional worm artifacts
- Good spatial error distribution
- Combines benefits of ordered and error diffusion
- Error stays localized (doesn't bleed to remote areas)

**Cons:**
- More complex to implement (Hilbert curve generation)
- Slightly slower than F-S due to curve traversal overhead
- Less common in embedded libraries

**For ESP32-C3:** Worth implementing if Atkinson artifacts are problematic. The Hilbert curve can be precomputed as a lookup table for 800x480 (~375KB, tight but possible in flash).

Source: [Compuphase Riemersma dither](https://www.compuphase.com/riemer.htm)

### 2.7 Recommended Dithering Strategy

For the TRMNL X4 with SSD1677:

| Content Type | Algorithm | Mode |
|-------------|-----------|------|
| Text/UI (1-bit) | None (threshold at 128) | Partial refresh |
| Text with gradients | Atkinson | Full refresh |
| Photographs (1-bit) | Blue noise (precomputed) | Full refresh |
| Photographs (4-gray) | Ordered Bayer 4x4 | Full refresh, 4-gray LUT |
| Dashboard/charts | Atkinson | Partial or full |
| Loading screens | None or threshold | Fast partial |

**The bb_epaper library** already implements dithered JPEG decoding (`ONE_BIT_DITHERED`, `FOUR_BIT_DITHERED`). The current firmware uses this. Enhancement opportunity: add blue noise as an alternative path for photographic content.

---

## 3. Partial Refresh Techniques

### 3.1 Standard Waveform Modes (E Ink Industry Standard)

| Mode | Gray Levels | Flash | Speed | Use Case |
|------|-------------|-------|-------|----------|
| **INIT** | 2 (B/W) | Yes | Slow | Display initialization, full clear |
| **DU** | 2 (B/W) | No | ~260ms | Direct update, pen input, fast B/W |
| **DU4** | 4 | No | Fast | Anti-aliased text in menus |
| **GC16** | 16 | Yes | ~450ms | Full grayscale with flash (highest quality) |
| **GL16** | 16 | No | ~450ms | Grayscale on white background (text pages) |
| **GLR16** | 16 | No | ~450ms | GL16 with reduced ghost (newer panels) |
| **A2** | 2 (B/W) | No | ~120ms | Fastest possible, significant ghosting |

**SSD1677 mapping:** The SSD1677 does not natively name these modes. Instead, we implement them by loading different custom LUTs:
- DU equivalent: aggressive 2-phase LUT, `0x22=0x0C`
- GC16 equivalent: full 40-phase LUT, `0x22=0xF7`
- Partial equivalent: windowed update via `0x44`/`0x45` + `0x22=0xFC`

Source: [Waveshare E-Paper mode declaration](https://www.waveshare.com/w/upload/c/c4/E-paper-mode-declaration.pdf), [rmkit eink-dev-notes](https://rmkit.dev/eink-dev-notes/)

### 3.2 TRMNL PLANE_FALSE_DIFF Mode

The current TRMNL firmware implements partial refresh through `PLANE_FALSE_DIFF`:

```cpp
// From display.cpp line 1258:
if (bbep.getPanelType() != EP75_800x480) {
    // need to write the inverted plane to do PLANE_FALSE_DIFF
}
// ...
bbep.writePlane(PLANE_FALSE_DIFF);
iRefreshMode = REFRESH_PARTIAL;
```

This works by:
1. Writing the new image to one RAM buffer
2. Writing the inverted previous image to the second RAM buffer
3. The SSD1677's dual-buffer architecture computes the diff internally
4. Only pixels that differ between buffers receive waveform voltage
5. Unchanged pixels receive Hi-Z (floating) -- no particle movement

**Key insight:** The SSD1677's dual RAM buffers (`0x24` for current, `0x26` for previous) enable hardware-accelerated differential updates. The LUT defines transitions between (old_state, new_state) pairs, not absolute pixel values.

### 3.3 Damage Tracking Algorithms

For intelligent partial refresh, track which regions changed:

**Tile-based damage tracking (recommended for ESP32-C3):**
```
Divide 800x480 into 32x32 tiles = 25x15 = 375 tiles
Each tile: 1 byte (dirty flag + change count)
Total overhead: 375 bytes
```

Per-tile comparison using `memcmp` on 128-byte tile data (32x32 / 8 bits per pixel) is fast enough for ESP32-C3 at 160MHz.

**Region merging:** When multiple adjacent tiles are dirty, merge into a single rectangular update region. The SSD1677 accepts arbitrary rectangular windows via `0x44`/`0x45` commands.

**SSD1677 coordinate quirk:** Y-coordinates are reversed due to gate orientation:
```
y_reversed = HEIGHT - y - height
```

### 3.4 Ghosting Management

Ghosting accumulates with partial refreshes. Mitigation strategies:

1. **Periodic full refresh** -- current firmware does every 16 partials (was 8). Research suggests 20-30 is safe with good waveforms.
2. **Ghost score tracking** -- compare expected vs actual pixel values using the dual buffer to estimate accumulated error.
3. **Compensation waveforms** -- before a full refresh, drive known-ghosted pixels with extra voltage phases.
4. **Content-aware scheduling** -- text-heavy content ghosts less than high-contrast graphics.

**Pervasive Displays' "Fast Update"** approach: uses optimized driving waveforms that maintain balanced pixel charges, preventing the charge imbalance that causes ghosting. Unlike legacy partial update, this guarantees no cumulative degradation.

Source: [Pervasive Displays Fast Update](https://www.pervasivedisplays.com/how-e-paper-works/fast-update-refresh/)

### 3.5 Recommended Partial Refresh Pipeline

```
1. Receive new image
2. Compare with previous frame (tile-based, 375 tiles)
3. If <0.1% changed -> skip refresh entirely
4. If <5% changed -> partial refresh (window update)
   a. Compute bounding rect of dirty tiles
   b. Set RAM window via 0x44/0x45
   c. Write only changed region to RAM
   d. Use PLANE_FALSE_DIFF for hardware diff
   e. Trigger with 0x22=0xFC (partial mode)
5. If >5% changed -> full refresh with appropriate LUT
6. Every 20-30 partials -> forced full refresh
7. Track ghost score; force full if score exceeds threshold
```

---

## 4. Temperature-Adaptive Rendering

### 4.1 Physics of Temperature Effects

E-ink particles are suspended in a dielectric fluid. Temperature directly affects:

| Parameter | Cold (<15C) | Normal (20-25C) | Hot (>35C) |
|-----------|-------------|------------------|------------|
| Fluid viscosity | High | Medium | Low |
| Particle mobility | Slow | Normal | Fast |
| Minimum refresh time | ~800ms | ~500ms | ~300ms |
| Required drive voltage | Higher | Standard | Lower |
| Ghosting risk | Higher | Normal | Lower |
| Phase duration needed | +20-30% | Baseline | -10-15% |

### 4.2 SSD1677 Temperature Sensor

The SSD1677 has a built-in temperature sensor with ~2C accuracy:

```
// Select internal sensor
write_command(0x18);
write_data(0x80);  // Internal sensor

// Read temperature (after display update)
// Temperature is encoded in the update sequence
// Use 0x1A to write external temp if internal is unreliable:
write_command(0x1A);
write_data(0x5A);  // Temperature encoding
```

The temperature sensor value influences waveform selection when OTP waveforms include temperature-indexed tables. For custom LUTs, we read the temperature and select our own LUT.

### 4.3 Temperature-Indexed Waveform Strategy

The recommended approach maps temperature bands to LUT parameters:

```
Temperature Band -> (Phase Duration Multiplier, Voltage Adjustment, Max Partials Before Full)

<5C    -> (1.5x, +1V VSH/VSL, 8 partials)   -- very conservative
5-15C  -> (1.2x, +0.5V, 12 partials)         -- conservative  
15-30C -> (1.0x, nominal, 20 partials)        -- baseline
30-40C -> (0.85x, -0.3V, 25 partials)        -- aggressive
>40C   -> (0.75x, -0.5V, 30 partials)        -- very aggressive
```

### 4.4 E Ink T2000 Timing Controller (Industry Reference)

E Ink and Himax jointly developed the T2000, an advanced color ePaper timing controller ASIC. Key features:
- Built-in temperature compensation algorithms
- Per-pixel waveform scheduling
- Hardware-accelerated dithering
- Designed for Spectra 6 (color) but architecture principles apply to monochrome

This represents the state of the art in commercial e-paper temperature compensation (2024).

Source: [E Ink T2000 announcement](https://www.eink.com/news/detail/E%20Ink-and-Himax-Unveil-Advanced-Color-ePaper-Timing-Controller-ASIC-T2000)

### 4.5 OTP vs External Temperature Compensation

The SSD1677 supports two approaches:

1. **OTP-based (iTC mode):** Waveforms pre-programmed in One-Time-Programmable memory, indexed by temperature. The controller auto-selects. Simple but inflexible.
2. **External (eTC mode):** MCU reads temperature, selects waveform, loads via `0x32`. Full control but requires firmware implementation.

**Recommendation:** Use eTC mode (external). The default OTP waveforms are generic and conservative. Custom temperature-indexed LUTs will outperform them.

Source: [GoodDisplay iTC vs eTC comparison](https://www.good-display.com/news/189.html), [GoodDisplay OTP and waveform files](https://www.good-display.com/news/205.html)

---

## 5. Font Rendering for E-Paper

### 5.1 Bitmap vs Vector at 220 PPI

At 220 PPI (the approximate density of 800x480 at 4.26"), the choice matters:

| Approach | Pros | Cons | Verdict |
|----------|------|------|---------|
| **Bitmap (BDF/PCF)** | Pixel-perfect, tiny footprint, fast render | Fixed sizes, no scaling | Best for fixed UI text |
| **Vector (TTF/OTF)** | Scalable, many fonts available | Needs rasterizer, hinting critical | Best for variable content |
| **Pre-rasterized sprites** | Fastest render, zero runtime cost | Inflexible, large flash use | Best for icons/logos |

At 220 PPI, vector fonts with good hinting produce excellent results. Below ~150 PPI, bitmap fonts are strongly preferred.

### 5.2 Hinting on Monochrome Displays

Font hinting is critical for 1-bit rendering:

- **Full hinting** (96-160 PPI range): Snaps stems/curves to pixel grid, dramatically improves legibility
- **Light/auto hinting** (160-300 PPI): Useful but less critical
- **No hinting** (>300 PPI): Fine -- resolution compensates

At 220 PPI on a 1-bit display, **auto-hinting produces acceptable results** but manual hinting is better. The Kindle uses unhinted TTFs with auto-hinting at render time.

For the ESP32-C3, the bb_epaper/bb_spi_lcd library handles font rendering. Using pre-hinted bitmap fonts (BDF format) avoids runtime hinting cost entirely.

### 5.3 Recommended Fonts for E-Paper

**Bitmap fonts (for UI at fixed sizes):**
- **Terminus** -- excellent fixed-width, designed for screens, available in BDF
- **UNSCII** -- designed for tiny displays, good at very small sizes
- **Tom Thumb** -- 3x5 pixel font, readable at minimum sizes
- **Tamzen** -- clean bitmap font derived from Terminus

**Vector fonts (for variable content):**
- **League Spartan Bold** -- reported as "particularly good" on e-ink at 16pt+
- **Roboto** -- clean geometry, good auto-hinting behavior
- **Inter** -- designed for screens, excellent at small sizes
- **IBM Plex Mono** -- excellent monospace for data/code display

**Workflow for ESP32-C3:**
1. Choose font and target size
2. Convert to BDF using FontForge or `otf2bdf`
3. Strip unused glyphs (reduce to ASCII + needed Unicode blocks)
4. Convert to Adafruit GFX format or bb_epaper's built-in format
5. Store in PROGMEM (flash)

Source: [MakerBlock smol fonts](https://makerblock.com/2025/06/smol-fonts-for-e-ink-displays/), [nicoverbruggen/ebook-fonts](https://github.com/nicoverbruggen/ebook-fonts), [TypeDrawers font rasterization discussion](https://typedrawers.com/discussion/3187/font-rasterization-on-e-readers)

### 5.4 Anti-Aliasing Considerations

On a 1-bit display, anti-aliasing is impossible. On the SSD1677's 4-gray mode:
- 2-bit anti-aliasing provides 4 levels (white, light gray, dark gray, black)
- Font edges benefit significantly from even this minimal anti-aliasing
- **DU4 waveform mode** is specifically designed for anti-aliased text (non-flashing, 4-level)
- Tradeoff: 4-gray mode requires full refresh, negating partial update benefits

**Recommendation:** Use 1-bit rendering with Atkinson dithering for general content. Reserve 4-gray mode for image-heavy screens where quality matters more than refresh speed.

---

## 6. Integration Recommendations for TRMNL X4

### 6.1 Priority Implementation Order

**Phase 1 -- Quick wins (firmware changes only):**
1. Implement temperature reading via `0x18` register (currently returns hardcoded 22.0C)
2. Add 3 custom LUT tables (cold/normal/warm) loaded via `0x32`
3. Increase SPI from 8MHz to 20MHz (validated by papyrix-reader at 40MHz)
4. Switch to Atkinson dithering for 1-bit JPEG decode path

**Phase 2 -- Partial refresh optimization:**
1. Implement tile-based damage tracking (375 bytes overhead)
2. Use bounding-rect partial windows via `0x44`/`0x45`
3. Implement ghost score tracking to schedule full refreshes adaptively
4. Increase partial-before-full threshold from 16 to 20+ with monitoring

**Phase 3 -- Advanced rendering:**
1. Add blue noise dithering option (store 64x64 texture in flash, 4KB)
2. Implement 4-gray mode with Bayer dithering for photographic content
3. Add content-type detection (text vs photo) to auto-select dithering algorithm
4. Explore Riemersma dithering if directional artifacts are reported

### 6.2 Existing Firmware Touchpoints

Key files to modify:
- `src/display.cpp` -- main display pipeline, dithering, refresh logic
- `src/epd_optimizations.cpp` -- temperature compensation, adaptive refresh (partially stubbed)
- `include/epd_optimizations.h` -- constants and thresholds
- `docs/EINK_OPTIMIZATION.md` -- existing optimization strategy document

The current firmware already:
- Uses `PLANE_FALSE_DIFF` for hardware-accelerated partial refresh
- Has dithered JPEG decoding (`ONE_BIT_DITHERED`, `FOUR_BIT_DITHERED`)
- Tracks partial refresh count (forces full every 16)
- Has a 4.26" workaround for SPI re-init after partial refresh
- Forces `REFRESH_FAST` (not partial) when refresh interval > 30min

### 6.3 Key Performance Targets

| Metric | Current | Target | How |
|--------|---------|--------|-----|
| SPI transfer (48KB frame) | 48ms @ 8MHz | 19ms @ 20MHz | `initIO()` freq param |
| Partial refresh | ~760ms | ~400ms | Custom fast LUT |
| Full refresh | ~4000ms | ~2500ms | Optimized LUT + temp comp |
| Temperature reading | Hardcoded 22C | Actual sensor | Register 0x18 read |
| Skip unchanged frames | Not implemented | <1ms check | `memcmp` on frame buffer |
| Dithering quality | Floyd-Steinberg (bb_epaper default) | Atkinson or blue noise | Custom dither path |

---

## 7. References

### Datasheets & Specifications
- [SSD1677 Datasheet (PDF)](https://cursedhardware.github.io/epd-driver-ic/SSD1677.pdf)
- [SSD1677 Solomon Systech Specification](https://www.e-paper-display.com/SSD1677Specification.pdf)
- [Waveshare E-Paper Mode Declaration (PDF)](https://www.waveshare.com/w/upload/c/c4/E-paper-mode-declaration.pdf)

### Open-Source Drivers & Tools
- [epdiy -- leading open-source e-paper driver](https://github.com/vroland/epdiy)
- [Modos Caster -- FPGA-based EPDC](https://github.com/Modos-Labs/Caster)
- [Modos Glider -- open-source e-ink monitor](https://github.com/Modos-Labs/Glider)
- [h0rv/ssd1677 -- Rust SSD1677 driver](https://github.com/h0rv/ssd1677)
- [papyrix-reader -- SSD1677 driver docs](https://github.com/bigbag/papyrix-reader/blob/main/docs/ssd1677-driver.md)
- [nlimper/LUT-playground -- waveform experimenter](https://github.com/nlimper/LUT-playground)
- [GxEPD2 -- Arduino e-paper library](https://github.com/ZinggJM/GxEPD2)
- [TRMNL firmware](https://github.com/usetrmnl/trmnl-firmware)

### Dithering Resources
- [Compuphase Riemersma dither](https://www.compuphase.com/riemer.htm)
- [Bart Wronski -- blue noise dithering](https://bartwronski.com/2016/10/30/dithering-part-two-golden-ratio-sequence-blue-noise-and-highpass-and-remap/)
- [Free blue noise textures](https://momentsingraphics.de/BlueNoise.html)
- [NXP Dithering for E-Ink panels](https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/Dithering-Implementation-for-Eink-Display-Panel/ta-p/1100219)
- [Albumentations dithering deep dive](https://albumentations.ai/blog/2025/02-the-art-and-science-of-dithering/)

### Font & Rendering
- [MakerBlock -- smol fonts for e-ink](https://makerblock.com/2025/06/smol-fonts-for-e-ink-displays/)
- [nicoverbruggen/ebook-fonts](https://github.com/nicoverbruggen/ebook-fonts)
- [TypeDrawers -- font rasterization on e-readers](https://typedrawers.com/discussion/3187/font-rasterization-on-e-readers)
- [Compuphase -- display typography](https://www.compuphase.com/scrnfont.htm)

### E-Paper Technology
- [fasani.de -- on driving eink displays](https://fasani.de/2025/01/03/on-driving-eink-displays/)
- [GoodDisplay OTP and waveform files](https://www.good-display.com/news/205.html)
- [GoodDisplay iTC vs eTC comparison](https://www.good-display.com/news/189.html)
- [E Ink T2000 timing controller](https://www.eink.com/news/detail/E%20Ink-and-Himax-Unveil-Advanced-Color-ePaper-Timing-Controller-ASIC-T2000)
- [Pervasive Displays Fast Update](https://www.pervasivedisplays.com/how-e-paper-works/fast-update-refresh/)
- [Viwoods e-ink refresh modes](https://viwoods.com/blogs/paper-tablet/viwoods-e-ink-refresh-modes-explained)
- [rmkit eink-dev-notes](https://rmkit.dev/eink-dev-notes/)
- [PineNote Development wiki](https://wiki.pine64.org/wiki/PineNote_Development)
- [Xteink X3 EPD analysis (CrazyCoder gist)](https://gist.github.com/CrazyCoder/82fec0bbd0e515dcc237d3db7451ec6f)
- [Ben Krasnow -- fast partial refresh](https://benkrasnow.blogspot.com/2017/10/fast-partial-refresh-on-42-e-paper.html)
