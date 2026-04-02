# ECHO Adaptive Firmware Architecture

**Version:** 0.0.1-alpha | **Status:** Design spec
**Principle:** The firmware adapts to context, not the user to the firmware.

## Core Idea

Instead of a fixed boot→connect→fetch→render→sleep cycle, ECHO evaluates its
**context** on every wake and selects the optimal **strategy**. Same hardware
primitives, different composition.

## Context Signals (read on wake, stored in RTC)

```c
typedef struct {
    // Power
    uint16_t battery_mv;          // ADC reading (GPIO 0)
    uint8_t  battery_pct;         // estimated 0-100
    bool     usb_powered;         // VBUS detection

    // Network
    uint8_t  wifi_channel;        // cached from last successful connect
    int8_t   wifi_rssi;           // last signal strength
    uint8_t  wifi_fail_count;     // consecutive failures (RTC persisted)
    bool     wifi_is_hotspot;     // detected phone hotspot (172.20.10.x range)

    // Display
    uint8_t  refresh_count;       // partials since last full refresh
    uint32_t last_image_hash;     // skip render if unchanged
    int8_t   temperature_c;       // SSD1677 temp sensor

    // Time
    uint32_t last_sync_epoch;     // last successful NTP sync
    uint32_t wake_count;          // total wakes since boot
    uint16_t sleep_duration_s;    // how long we slept

    // Mesh
    uint8_t  ota_check_counter;   // cycles until next OTA check
    bool     ota_pending;         // manifest indicated new version
} EchoContext;
```

## Strategies (composed from primitives)

### STRATEGY_FULL — Normal home WiFi cycle
**When:** battery > 30%, wifi_fail_count == 0, home WiFi available
```
wake → read_context → wifi_fast_connect(cached_channel)
     → api_fetch_image → delta_check(hash)
     → render_partial_or_skip → ota_check_periodic
     → wifi_off → deep_sleep(900s)
```
**Wake time:** ~4-8s | **Power:** medium

### STRATEGY_HOTSPOT — Phone hotspot (echo-field)
**When:** wifi_is_hotspot == true OR wifi_fail_count > 0 + hotspot detected
```
wake → read_context → wifi_connect(longer_timeout=30s, static_ip)
     → api_fetch_image → render
     → wifi_off → deep_sleep(900s)
```
**Wake time:** ~8-15s | **Power:** medium-high
**Key adaptation:** 30s timeout, static IP skips DHCP, skip DNS cache

### STRATEGY_OFFLINE — No WiFi available
**When:** wifi_fail_count >= 3 AND battery > 20%
```
wake → read_context → render_cached_frame(spiffs)
     → deep_sleep(3600s)  // sleep longer, conserve battery
```
**Wake time:** ~1s | **Power:** very low
**Key adaptation:** no network at all, use last cached image, extend sleep

### STRATEGY_ZEN — Ultra-low battery
**When:** battery < 15%
```
wake → read_context → render_low_battery_screen
     → deep_sleep(MAX)  // wake only on USB power detect
```
**Wake time:** ~0.5s | **Power:** minimal

### STRATEGY_PORTAL — WiFi setup needed
**When:** no saved credentials OR user triggered reset (GPIO 3 long press)
```
wake → start_captive_portal → wait_for_credentials
     → connect_and_verify → save → restart
```
**Wake time:** unlimited (user interaction) | **Power:** high (AP mode)

### STRATEGY_OTA — Firmware update available
**When:** ota_pending == true AND battery > 40% AND wifi connected
```
wake → wifi_connect → ota_download_stream → flash_and_verify
     → restart
```
**Wake time:** ~30-60s | **Power:** high
**Safety:** requires > 40% battery, dual-bank A/B rollback

### STRATEGY_FIRST_BOOT — Fresh device
**When:** wake_count == 0 OR factory reset
```
wake → show_echo_logo(full_refresh) → start_portal
     → guided_setup → first_image_fetch → render
     → deep_sleep(300s)  // shorter first cycle for quick feedback
```

## Strategy Selection (decision tree)

```c
EchoStrategy select_strategy(const EchoContext* ctx) {
    // Safety first
    if (ctx->battery_pct < 15 && !ctx->usb_powered)
        return STRATEGY_ZEN;

    // OTA takes priority when available and safe
    if (ctx->ota_pending && ctx->battery_pct > 40)
        return STRATEGY_OTA;

    // No credentials → portal
    if (!has_saved_wifi())
        return STRATEGY_PORTAL;

    // First boot
    if (ctx->wake_count == 0)
        return STRATEGY_FIRST_BOOT;

    // Network failures → try hotspot, then go offline
    if (ctx->wifi_fail_count >= 3)
        return STRATEGY_OFFLINE;

    // Detect hotspot by IP range or previous flag
    if (ctx->wifi_is_hotspot)
        return STRATEGY_HOTSPOT;

    // Default: full cycle
    return STRATEGY_FULL;
}
```

## Primitive Composition Table

| Primitive | FULL | HOTSPOT | OFFLINE | ZEN | PORTAL | OTA |
|-----------|------|---------|---------|-----|--------|-----|
| battery_read | x | x | x | x | | x |
| wifi_fast_connect | x | | | | | x |
| wifi_connect_long | | x | | | | |
| wifi_scan | | | | | x | |
| wifi_off | x | x | | | | |
| api_fetch_image | x | x | | | | |
| delta_check | x | x | | | | |
| render_partial | x | x | | | | |
| render_full | | | | | x | |
| render_cached | | | x | | | |
| render_status | | | | x | | |
| ota_manifest_check | x | | | | | |
| ota_download | | | | | | x |
| spiffs_cache_read | | | x | | | |
| spiffs_cache_write | x | x | | | | |
| rtc_save_context | x | x | x | x | | |
| deep_sleep | x | x | x | x | | |
| captive_portal | | | | | x | |
| ntp_sync | x | x | | | | |
| temperature_read | x | x | | | | |

## RTC-Persisted State (survives deep sleep)

```c
RTC_DATA_ATTR EchoContext echo_ctx;
RTC_DATA_ATTR uint32_t   cached_image_hash;
RTC_DATA_ATTR uint8_t    cached_wifi_channel;
RTC_DATA_ATTR int8_t     cached_wifi_rssi;
RTC_DATA_ATTR uint8_t    strategy_history[8];  // last 8 strategies for pattern detection
```

## Adaptive Behaviors

### WiFi Channel Caching
Save the WiFi channel on successful connect. On next wake, connect directly
to that channel (skips full scan, saves 1-3 seconds).

### Image Delta Skip
Hash the fetched image. If hash matches RTC-cached hash, skip render entirely.
Saves ~200-400ms display refresh + power.

### Sleep Duration Adaptation
```c
uint16_t adaptive_sleep(const EchoContext* ctx) {
    uint16_t base = 900;  // 15 minutes default

    // Extend sleep when offline (nothing new to show)
    if (ctx->wifi_fail_count > 0)
        base = 3600;  // 1 hour

    // Shorten when USB powered (assume user is watching)
    if (ctx->usb_powered)
        base = 300;  // 5 minutes

    // Extend when battery low
    if (ctx->battery_pct < 30)
        base = base * 2;

    return base;
}
```

### Hotspot Detection
iPhone hotspots assign IPs in 172.20.10.x/28 range.
Android hotspots typically use 192.168.43.x/24.
Detect on DHCP response and set `wifi_is_hotspot = true` for future wakes.

### Waveform Temperature Compensation
SSD1677 has a built-in temperature sensor (currently stubbed at 22°C).
Read actual temperature and select waveform LUT accordingly:
- < 10°C: slower waveforms (ink moves slowly in cold)
- 10-35°C: normal waveforms
- > 35°C: faster waveforms (ink is more fluid)

## Migration Path

### Phase 1: Context struct + strategy selector (no behavior change)
Add EchoContext and select_strategy() alongside existing code.
Log which strategy WOULD be selected. Zero risk.

### Phase 2: WiFi channel caching + image delta skip
Lowest risk optimizations. Measurable time/power savings.

### Phase 3: Adaptive sleep + hotspot detection
Changes sleep behavior. Needs testing with actual battery measurements.

### Phase 4: Full strategy engine replaces fixed boot cycle
The main bl.cpp loop becomes:
```c
void app_main() {
    echo_ctx_load();           // read RTC state
    echo_ctx_read_sensors();   // battery, temperature
    EchoStrategy s = select_strategy(&echo_ctx);
    execute_strategy(s, &echo_ctx);
    echo_ctx_save();           // persist to RTC
    deep_sleep(adaptive_sleep(&echo_ctx));
}
```

## File Map (proposed)

```
src/
├── echo_context.h      // EchoContext struct + RTC declarations
├── echo_context.cpp    // context load/save/sensor reads
├── echo_strategy.h     // strategy enum + select_strategy()
├── echo_strategy.cpp   // strategy execution (composes primitives)
├── echo_adaptive.h     // adaptive_sleep, hotspot_detect, channel_cache
└── echo_adaptive.cpp   // adaptive behavior implementations
```

These files wrap existing code — they don't replace bl.cpp, they call into it.
