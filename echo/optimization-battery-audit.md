# ECHO Battery & Deep Sleep Optimization Audit

**Date:** 2026-04-02
**Firmware:** v1.7.8 (BOARD_XTEINK_X4, ESP32-C3)
**Target:** 14+ day battery life on 15-min refresh cycle

---

## 1. Wake Cycle Timeline (Estimated)

| Phase | Estimated Duration | Notes |
|-------|-------------------|-------|
| Boot + Serial init | ~50 ms | `Serial.begin(115200)`, pins_init |
| Battery ADC read | ~5 ms | Done before WiFi (good) |
| Button handling | ~10 ms | Only on GPIO wake |
| Preferences init | ~20 ms | NVS flash read |
| Display init | ~100 ms | bb_epaper init + SPI setup |
| SPIFFS mount | ~50 ms | `filesystem_init()` |
| Logo display (non-timer wake) | ~2000 ms | Full EPD refresh on button/reset wake |
| File listing + NVS log | ~20 ms | `list_files()`, `log_nvs_usage()` |
| **WiFi connect** | **2000-15000 ms** | Full scan + association; 15s timeout per attempt, up to 3 attempts |
| NTP sync | 0-3000 ms | Skipped if <24h since last sync (good optimization) |
| submitStoredLogs (1st call) | 0-2000 ms | HTTP POST if logs exist |
| **DNS resolution** | **500-10000 ms** | Up to 5 attempts with 2s delay between each |
| **API /display fetch** | **1000-5000 ms** | New TLS handshake + HTTP GET |
| **Image download** | **2000-8000 ms** | New TLS handshake + HTTP GET for image |
| submitStoredLogs (2nd call) | 0-2000 ms | Called again after download |
| WiFi disconnect | ~10 ms | Done after image download |
| Image decode + display | 1000-3000 ms | PNG/JPEG decode + EPD refresh |
| SPIFFS write (cache) | ~200 ms | Write image to flash |
| OTA check | ~0 ms | Only if flagged |
| submitStoredLogs (3rd call) | 0-2000 ms | Called in goToSleep() |
| Display sleep | ~50 ms | `bbep.sleep(DEEP_SLEEP)` |
| SPIFFS unmount | ~10 ms | `filesystem_deinit()` |
| Preferences save + end | ~20 ms | Sleep time + epoch write |
| GPIO hold + deep sleep | ~1 ms | `gpio_hold_en(GPIO_NUM_13)` |
| **TOTAL (typical)** | **~8-15 seconds** | Happy path with cached NTP |
| **TOTAL (worst case)** | **~45+ seconds** | Failed DNS + retries + NTP sync |

---

## 2. Deep Sleep GPIO Audit

### XTEINK_X4 Pin Map

| GPIO | Function | Pre-Sleep State | Issue? |
|------|----------|----------------|--------|
| 3 | PIN_INTERRUPT (button) | INPUT + PULLUP (via `pins_init()`) | OK - configured at boot |
| 0 | PIN_BATTERY (ADC) | Analog input | **FLOATING** - not reconfigured before sleep |
| 8 | EPD_SCK_PIN (SPI CLK) | SPI output | **FLOATING** - SPI bus not de-initialized |
| 10 | EPD_MOSI_PIN (SPI MOSI) | SPI output | **FLOATING** - SPI bus not de-initialized |
| 21 | EPD_CS_PIN (SPI CS) | Digital output | **FLOATING** - not driven low or isolated |
| 5 | EPD_RST_PIN (EPD reset) | Digital output | **FLOATING** - not driven low |
| 4 | EPD_DC_PIN (EPD data/cmd) | Digital output | **FLOATING** - not driven low |
| 6 | EPD_BUSY_PIN (EPD busy) | Digital input | **FLOATING** - no pulldown configured |
| 13 | MOSFET (battery power) | `gpio_hold_en()` | **OK** - properly held during deep sleep |

### Findings

**CRITICAL: SPI bus GPIOs left floating before deep sleep.**

The `display_sleep()` function calls `bbep.sleep(DEEP_SLEEP)` which puts the EPD controller into sleep mode, but does **not** de-initialize the SPI peripheral or set the SPI GPIOs to a defined low-power state. The ESP32-C3 SPI peripheral remains configured, and when the chip enters deep sleep, those GPIOs revert to an undefined state.

On ESP32-C3, GPIOs that are not in the RTC domain and are left floating can draw 50-200 uA **each** during deep sleep. With 5-6 floating GPIOs, this could add **300-1200 uA** of parasitic current.

**GPIO 13 MOSFET: CORRECT.** The firmware properly calls `gpio_hold_en(GPIO_NUM_13)` and `gpio_deep_sleep_hold_en()` to keep the battery MOSFET enabled during deep sleep. This is essential and correctly implemented.

**PIN_BATTERY (GPIO 0):** The ADC pin is read once at boot but never reconfigured before sleep. On ESP32-C3, ADC GPIOs left as analog inputs can leak current.

---

## 3. WiFi Power Management Audit

### WiFi Shutdown Sequence in `goToSleep()`

```
Lines 2101-2105:
  submitStoredLogs();
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
  WiFi.mode(WIFI_OFF);
```

### Findings

**GOOD:** WiFi is properly shut down:
- `WiFi.disconnect()` is called if connected
- `WiFi.mode(WIFI_OFF)` is called unconditionally
- This internally calls `esp_wifi_stop()` which powers down the radio

**GOOD:** Early WiFi disconnect at line 908 -- after image download, WiFi is disconnected with `WiFi.disconnect(true)` to save power during image decode + display render.

**ISSUE: No `esp_wifi_stop()` explicit call.** While `WiFi.mode(WIFI_OFF)` should handle this, the Arduino WiFi wrapper does not always call `esp_wifi_deinit()`. The WiFi memory (about 40KB) remains allocated until `esp_wifi_deinit()` is called. This does not affect deep sleep power, but wastes RAM during the pre-sleep phase.

**ISSUE: `WiFi.setSleep(0)` in autoConnect.** Line 535 of WifiCaptive.cpp disables WiFi modem sleep during the connection process. This is correct for fast connection, but if the connection hangs for the full 15s timeout, the radio is at full power the entire time.

---

## 4. HTTP Connection Reuse Audit

### Current Architecture

Every HTTP request creates a **new TCP connection + TLS handshake**:

```
// http_client.h line 55:
https.setReuse(false);  // Disable keep-alive to ensure connection closes properly
```

The `withHttp()` template function:
1. Allocates a new `WiFiClientSecure` (or `WiFiClient`)
2. Calls `setInsecure()` (no cert validation)
3. Opens connection
4. Runs callback
5. Calls `https.end()` + `client->stop()` + `delete client`

### API Calls Per Wake Cycle

In a typical wake cycle, the firmware makes **3-5 separate HTTP connections**:

1. `submitStoredLogs()` -- POST /api/log (1st call, line 437)
2. `fetchApiDisplay()` -- GET /api/display (line 711)
3. Image download -- GET image URL (line 733, separate `withHttp()`)
4. `submitStoredLogs()` -- POST /api/log (2nd call, line 499)
5. `submitStoredLogs()` -- POST /api/log (3rd call, in `goToSleep()` line 2101)

Each TLS handshake costs approximately **1000-2000 ms** on ESP32-C3. That is **3-10 seconds** wasted on TLS handshakes alone.

### Findings

**CRITICAL: `setReuse(false)` explicitly disables HTTP keep-alive.** This forces a new TCP + TLS connection for every single request. The comment says "to ensure connection closes properly" suggesting this was a workaround for a bug.

**ISSUE: `submitStoredLogs()` called 3-4 times per wake cycle.** Lines 437, 499, 906, and 2101. Each call can trigger a separate HTTP POST. The first call (line 437) happens before the API display fetch, so it cannot share a connection. But calls at lines 499 and 906 could be consolidated.

**ISSUE: DNS resolution done separately from HTTP.** The `downloadAndShow()` function resolves the API hostname manually (line 691) with up to 5 retries + 2s delays, then the `withHttp()` function re-resolves the same hostname when creating the HTTP connection. This is double DNS resolution.

---

## 5. RTC Memory Usage Audit

### Currently Stored in RTC_DATA_ATTR

| Variable | Type | Purpose |
|----------|------|---------|
| `need_to_refresh_display` | uint8_t | Skip display update if content unchanged |
| `iUpdateCount` | int | Partial refresh counter (display.cpp) |
| `shadow_ota_check_counter` | uint8_t | OTA check frequency limiter |
| `lastCO2, lastSCDTemp, etc.` | int (x8) | Sensor data (only with SENSOR_SDA) |

### What is NOT Cached (Opportunities)

| Data | Estimated Savings | Feasibility |
|------|-------------------|-------------|
| **WiFi channel + BSSID** | 1-3 seconds per wake | High - standard ESP-IDF technique |
| **DNS IP for API server** | 500-2000 ms per wake | High - cache IP, validate occasionally |
| **Last image hash/filename** | Already partially done via SPIFFS | Medium - filename is in NVS Preferences |
| **NTP time offset** | 0-3000 ms per wake | Already done via Preferences (good) |
| **WiFi static IP config** | ~200 ms | Low - only matters for DHCP |
| **TLS session ticket** | ~1000 ms per TLS connection | Medium - ESP-IDF supports this |

### Findings

**WiFi channel + BSSID caching is the single biggest win.** Currently, `autoConnect()` does a full WiFi scan on every boot (when the last-used network is not found immediately). Caching the channel and BSSID in RTC memory lets `WiFi.begin()` skip the scan phase entirely, saving 1-3 seconds.

**DNS caching is the second biggest win.** The manual DNS resolution loop (lines 689-702) retries up to 5 times with 2000ms delays. Caching the resolved IP in RTC memory and using it directly would eliminate this entirely for 99% of wake cycles.

---

## 6. Optimization Recommendations

### P0 - High Impact, Safe

| # | Optimization | Est. Savings | Risk |
|---|-------------|-------------|------|
| 1 | **Cache WiFi channel + BSSID in RTC memory** | 1-3 sec/wake | Safe - fallback to full scan on failure |
| 2 | **Cache DNS result in RTC memory** | 0.5-10 sec/wake | Safe - re-resolve after N failures |
| 3 | **Enable HTTP keep-alive** (remove `setReuse(false)`) | 2-6 sec/wake | Needs testing - was disabled intentionally |
| 4 | **Set SPI GPIOs to INPUT_PULLDOWN before deep sleep** | 300-1200 uA saved | Safe - standard practice |
| 5 | **Consolidate submitStoredLogs calls** (call once before sleep) | 0-4 sec/wake | Safe - logs are stored until submitted |

### P1 - Medium Impact, Needs Testing

| # | Optimization | Est. Savings | Risk |
|---|-------------|-------------|------|
| 6 | **Set ADC GPIO to INPUT_PULLDOWN before sleep** | 50-100 uA saved | Needs testing - verify no effect on next boot |
| 7 | **TLS session ticket caching** | ~1 sec per TLS connection | Needs testing - server must support it |
| 8 | **Reduce DNS retry delay from 2000ms to 500ms** | Up to 7.5 sec on failures | Needs testing - may fail on slow networks |
| 9 | **Skip mDNS init on timer wake** | ~50 ms | Safe if mDNS not used for API |
| 10 | **Remove redundant file listing** (`list_files()` on every boot) | ~20 ms | Safe - diagnostic only |

### P2 - Lower Impact or Risky

| # | Optimization | Est. Savings | Risk |
|---|-------------|-------------|------|
| 11 | **Parallel NTP + DNS resolution** | 0.5-3 sec/wake | Risky - complex async code |
| 12 | **Conditional image download (hash check)** | 2-8 sec when image unchanged | Needs server support (ETag/304) |
| 13 | **Reduce WiFi connection timeout from 15s to 8s** | Faster failure path | Risky - may fail on slow APs |
| 14 | **Use `esp_wifi_deinit()` after WIFI_OFF** | ~40KB RAM freed, marginal power | Needs testing |

---

## 7. Estimated Impact Summary

### Current Power Budget (estimated)

- Deep sleep current: **~20 uA** (ESP32-C3 spec) + **300-1200 uA floating GPIOs** = **320-1220 uA**
- Wake duration: **8-15 seconds** typical
- Wake current: **~80-160 mA** (WiFi active)
- Sleep duration: **900 seconds** (15 min default)

### With P0 Optimizations Applied

- Deep sleep current: **~20-50 uA** (GPIOs properly configured)
- Wake duration: **3-6 seconds** (cached WiFi/DNS, keep-alive, consolidated logs)
- Wake current: **~80-160 mA** (unchanged)
- Sleep duration: **900 seconds** (unchanged)

### Battery Life Projection

Assuming 1000 mAh battery:

| Scenario | Avg Current | Battery Life |
|----------|-------------|-------------|
| **Current (with floating GPIOs)** | ~0.8-1.5 mA average | 28-52 days |
| **Current (if GPIOs not floating)** | ~0.16-0.25 mA average | 166-260 days |
| **With P0 optimizations** | ~0.08-0.15 mA average | 277-520 days |

> Note: The floating GPIO estimate is the most uncertain. If the bb_epaper library internally sets GPIOs to a safe state during `sleep(DEEP_SLEEP)`, the actual leakage may be lower. **Measure actual deep sleep current to confirm.**

---

## 8. Code References

| File | Key Lines | Concern |
|------|-----------|---------|
| `src/pins.cpp:6-14` | `pins_init()` -- only configures PIN_INTERRUPT | Missing pre-sleep GPIO config |
| `src/bl.cpp:2099-2131` | `goToSleep()` -- no SPI/GPIO cleanup | Floating GPIOs |
| `src/bl.cpp:2102-2105` | WiFi disconnect + WIFI_OFF | Correct |
| `src/bl.cpp:2127-2129` | GPIO 13 MOSFET hold | Correct |
| `lib/trmnl/include/http_client.h:55` | `https.setReuse(false)` | Disables keep-alive |
| `src/bl.cpp:689-702` | DNS retry loop (5x, 2s delay) | No caching |
| `src/bl.cpp:437,499,906,2101` | 4x `submitStoredLogs()` calls | Redundant HTTP connections |
| `src/display.cpp:2068-2077` | `display_sleep()` -- no SPI de-init | SPI GPIOs left configured |
| `lib/wificaptive/src/WifiCaptive.cpp:535` | `WiFi.setSleep(0)` | Modem sleep disabled during connect |
| `include/config.h:68` | `SLEEP_TIME_TO_SLEEP 900` | 15-min default refresh |
| `src/bl.cpp:74` | `RTC_DATA_ATTR need_to_refresh_display` | Only RTC var for main logic |

---

## 9. Recommended Next Steps

1. **Measure actual deep sleep current** with a uA-capable meter (e.g., PPK2). This is the single most important diagnostic step -- it will confirm or disprove the floating GPIO theory.
2. **Implement P0 #4** (GPIO cleanup before sleep) as the lowest-risk, highest-certainty win.
3. **Implement P0 #1** (WiFi channel/BSSID caching) for the biggest wake-time reduction.
4. **Test P0 #3** (HTTP keep-alive) in isolation to verify it does not cause connection leak issues.
5. **Implement P0 #2** (DNS caching) alongside the WiFi caching for compounding benefit.
