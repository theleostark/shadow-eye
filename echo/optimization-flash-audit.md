# ECHO Firmware Flash Size Audit

**Date:** 2026-04-02
**Firmware:** xteink_x4_release (ESP32-C3)
**Binary:** 1,277 KB (firmware.bin = 1,307,312 bytes)
**OTA bank:** 1,856 KB (0x1D0000 = 1,900,544 bytes)
**Usage:** 68.8% of OTA bank (579 KB free)
**Target:** reduce by 200+ KB to free space for new features

## Current Size Breakdown

### Section Summary (from linker map)

| Section | Size | Notes |
|---------|------|-------|
| `.flash.text` | 987.4 KB | Code in flash |
| `.flash.rodata` | 199.6 KB | Read-only data (strings, const arrays) |
| `.iram0.text` | 75.7 KB | Code in IRAM (hot paths) |
| **Total** | **1,262.7 KB** | Binary overhead brings it to 1,277 KB |

### Top Library Contributors (linked size from map)

| Library | Size | Notes |
|---------|------|-------|
| ESPAsyncWebServer | 1,883 KB* | Captive portal only |
| framework-arduinoespressif32 | 1,353 KB* | Arduino core |
| libc | 1,169 KB* | C standard library |
| wpa_supplicant | 1,120 KB* | WiFi security (required) |
| lwip | 1,025 KB* | TCP/IP stack (required) |
| mbedcrypto | 989 KB* | TLS (required for HTTPS) |
| **wificaptive** | **947 KB*** | Custom captive portal lib |
| libstdc++ | 663 KB* | C++ stdlib |
| libdriver | 641 KB* | ESP-IDF drivers |
| libtrmnl | 635 KB* | TRMNL core library |
| bl.cpp | 477 KB* | Business logic |
| nvs_flash | 474 KB* | NVS (required) |
| WiFi | 455 KB* | WiFi library (required) |
| **mqtt_sprite.cpp** | **282 KB*** | MQTT sprite system |
| wifi_provisioning | 225 KB* | WiFi provisioning (possibly unused) |
| WiFiClientSecure | 211 KB* | HTTPS client (required) |
| mdns | 245 KB* | mDNS library |
| PNGdec | 175 KB* | PNG decoder (required) |
| HTTPClient | 171 KB* | HTTP client (required) |
| JPEGDEC | 156 KB* | JPEG decoder |
| display.cpp | 136 KB* | Display rendering |
| AsyncTCP | 124 KB* | TCP backend for ESPAsyncWebServer |
| PubSubClient | 98 KB* | MQTT client library |
| sprite_renderer.cpp | 53 KB* | Sprite rendering engine |
| mdns_discovery.cpp | 63 KB* | mDNS browser |

*\* Object file section totals from linker map. The linker strips unused symbols, so actual linked contribution to the binary is smaller. However, relative sizes indicate optimization priority.*

---

## Optimization Opportunities

### 1. Remove MQTT Sprite System (mqtt_sprite + sprite_renderer + PubSubClient)

**Estimated savings: 40-80 KB linked**
**Risk: LOW** (feature is opt-in, disabled by default)

**Findings:**
- `mqtt_sprite.cpp` is initialized in `main.cpp` on every boot
- `mqtt_sprite_init()` checks `preferences` for an `mqtt_enabled` flag (defaults to `false`)
- When disabled, the code still links: PubSubClient (98 KB obj), mqtt_sprite (282 KB obj), sprite_renderer (53 KB obj)
- `sprite_data.h` embeds 8 PNG sprites as byte arrays (~34 KB source, ~20 KB binary data)
- `PubSubClient` dependency in `platformio.ini` exists solely for this feature

**What breaks:** MQTT-based sprite push to the display. This is a custom ECHO feature not part of upstream TRMNL. It can be gated behind a build flag or removed entirely.

**Recommended action:**
- Wrap in `#ifdef ECHO_MQTT_SPRITES` / remove from default build
- Remove `knolleary/PubSubClient@^2.8.0` from `[deps_app]` (it pulls in for ALL board targets)
- Conservative estimate: **50-80 KB saved** (PubSubClient + MQTT client code + sprite data + renderer)

---

### 2. ESPAsyncWebServer: Keep but Constrain

**Estimated savings: 0 KB (cannot easily replace) / 5-15 KB with config trimming**
**Risk: HIGH to replace, LOW to trim**

**Findings:**
- ESPAsyncWebServer is used exclusively by `lib/wificaptive/src/WebServer.cpp` for the captive portal
- Features actually used: `server.on()` route handlers, `AsyncWebServerResponse`, `AsyncCallbackJsonWebHandler`
- Features NOT used: WebSockets (`AsyncWebSocket`), Server-Sent Events (`AsyncEventSource`), MessagePack (`AsyncMessagePack`), Middleware
- The library is v3.7.7 and compiles ALL modules even if unused (no `#ifdef` gating)
- Replacing with ESP-IDF `httpd` would require rewriting the entire captive portal and breaking the `AsyncCallbackJsonWebHandler` JSON parsing -- HIGH risk

**Why not replace:**
- The captive portal is critical for WiFi setup (cannot be broken)
- `httpd` from ESP-IDF is already linked (the HTTPD config keys appear in sdkconfig) but would need manual JSON parsing, response building, and DNS captive portal logic
- The wificaptive library (947 KB obj) is tightly coupled to ESPAsyncWebServer

**Recommended action:**
- Do NOT replace ESPAsyncWebServer at this time
- Investigate if PlatformIO `lib_ignore` can exclude unused ESPAsyncWebServer modules (WebSocket, EventSource, MessagePack) -- this may save 5-15 KB

---

### 3. Build Flags: Already Good, Minor Improvements Available

**Estimated savings: 10-30 KB**
**Risk: LOW-MEDIUM**

**Current state (xteink_x4_release):**
- `-Os` (optimize for size): YES, already enabled
- `build_type = release`: YES
- `CORE_DEBUG_LEVEL=0`: YES (no debug logging in release)
- BLE: `CONFIG_BT_ENABLED is not set` -- BLE is disabled in sdkconfig. However, **29 BLE .o files are compiled and present in the build directory** (all ~1.2 KB stubs). The linker should strip them, but this is suspicious.

**Issues found:**
- `CONFIG_LWIP_IPV6=y` -- IPv6 is enabled but ECHO only needs IPv4. Disabling saves ~10-20 KB
- `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y` -- SoftAP is needed for captive portal, keep it
- `CONFIG_LWIP_DHCPS=y` -- DHCP server is needed for captive portal, keep it
- `wifi_provisioning` library (225 KB obj) appears linked but no code in src/ or lib/ references it. It may be pulled in transitively by the Arduino framework. If it can be excluded via `lib_ignore`, that could save significant flash.
- The `build_type = debug` in `[env]` base means ALL non-release environments build with debug info. This is correct for development but verify the release env truly overrides it (it does: `build_type = release`).

**Recommended action:**
- Add `CONFIG_LWIP_IPV6=n` to `sdkconfig.xteink_x4_release` (saves ~10-20 KB)
- Investigate excluding `wifi_provisioning` via sdkconfig: `CONFIG_WIFI_PROV_ENABLED=n`
- Add `-ffunction-sections -fdata-sections` if not already present (enables dead code stripping -- verify with `-Wl,--print-gc-sections`)

---

### 4. Partition Table: Optimal for OTA

**No savings available**

**Current `min_spiffs.csv`:**
| Partition | Offset | Size |
|-----------|--------|------|
| nvs | 0x9000 | 20 KB |
| otadata | 0xE000 | 8 KB |
| app0 (OTA_0) | 0x10000 | 1,856 KB |
| app1 (OTA_1) | 0x1E0000 | 1,856 KB |
| spiffs | 0x3B0000 | 256 KB |
| coredump | 0x3F0000 | 64 KB |

This is the standard ESP32 4MB min_spiffs partition table. Both OTA banks must be equal size. The only way to increase app space is to shrink SPIFFS or coredump, but that changes the OTA bank boundary which breaks rollback.

---

### 5. Unused Fonts: All Three Are Used

**Estimated savings: 0 KB (all referenced)**
**Risk: N/A**

All three fonts are actively used in `src/display.cpp`:
- `nicoclean_8.h` (10 KB source) -- used in multiple places
- `Inter_18.h` (39 KB source) -- used for standard text
- `Roboto_Black_24.h` (18 KB source) -- used for headers

These compile to const arrays in `.rodata`. Combined source is ~67 KB, binary contribution is roughly 20-30 KB after compilation. All are needed.

---

### 6. BOARD_TRMNL_X Dead Code

**Estimated savings: 0-5 KB**
**Risk: LOW**

Eight `#ifdef BOARD_TRMNL_X` blocks exist across `display.cpp`, `bl.cpp`, `config.h`, and `DEV_Config.h`. Since ECHO builds with `BOARD_XTEINK_X4`, these blocks are excluded by the preprocessor. They compile to zero bytes -- the preprocessor handles this correctly.

However, the `#else` fallback branches that DO compile for XTEINK_X4 may contain unnecessary code paths. This requires per-block review but savings are negligible.

---

## Summary Table

| Optimization | Est. Savings | Risk | Effort | Status |
|-------------|-------------|------|--------|--------|
| Remove MQTT sprite system | 50-80 KB | LOW | Low | Ready |
| Remove PubSubClient dep | (included above) | LOW | Trivial | Ready |
| Disable IPv6 in sdkconfig | 10-20 KB | LOW | Trivial | Ready |
| Disable wifi_provisioning | 10-30 KB | MEDIUM | Low | Needs testing |
| Trim ESPAsyncWebServer unused modules | 5-15 KB | MEDIUM | Medium | Needs investigation |
| Add -ffunction-sections/-fdata-sections | 5-20 KB | LOW | Trivial | Needs verification |
| **Total estimated** | **80-165 KB** | | | |

## Recommended Execution Order

### Phase 1: Safe wins (do now)
1. Gate MQTT sprite system behind `#ifdef ECHO_MQTT_SPRITES` (do not define it)
2. Move `PubSubClient` from `[deps_app]` to board-specific deps (or remove entirely)
3. Add `CONFIG_LWIP_IPV6=n` to sdkconfig defaults for xteink_x4

### Phase 2: Verify and apply
4. Test with `CONFIG_WIFI_PROV_ENABLED=n` or equivalent sdkconfig flag
5. Verify `-ffunction-sections -fdata-sections -Wl,--gc-sections` are active (ESP-IDF may already use them)
6. Build and measure delta

### Phase 3: Deeper optimization (if needed)
7. Evaluate slimming ESPAsyncWebServer (fork with unused modules removed)
8. Profile mbedTLS cipher suite -- disable unused ciphers in sdkconfig
9. Consider replacing JPEGDEC if JPEG support is not needed for ECHO

## Notes

- The firmware binary (1,277 KB) is well within the 1,856 KB OTA bank
- The 200 KB target is aggressive but achievable with Phase 1 + Phase 2
- PubSubClient being in `[deps_app]` means it compiles for ALL board targets -- removing it from common deps benefits every build, not just ECHO
- The BLE .o stubs in the build dir are ~1.2 KB each (empty objects) and do not contribute to the binary -- BLE is correctly disabled
