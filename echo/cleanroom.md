# ECHO Cleanroom Re-engineering Audit

Systematic inventory of all TRMNL-branded assets that need replacement
for ECHO to stand as an independent product.

## Audit Methodology

1. **String scan** — `rebrand.py --dry-run` finds text references
2. **Asset scan** — bitmap/sprite/HTML files with TRMNL branding
3. **API scan** — endpoints we depend on vs. ones we can self-host
4. **Legal scan** — license compliance, attribution requirements

## Layer 1: User-Visible Strings (display.cpp)

| Location | Current | Target | Status |
|----------|---------|--------|--------|
| display.cpp:1542 | "TRMNL firmware " | "ECHO " | pending |
| display.cpp:1550 | "Can't establish WiFi connection." | "Cannot connect to WiFi." | pending |
| display.cpp:1554 | Help text → TRMNL QR | Help text → shadowlab.cc/echo | pending |
| display.cpp:2014 | "TRMNL firmware " | "ECHO " | pending |

## Layer 2: Bitmap Assets (embedded C headers)

| File | Content | Action |
|------|---------|--------|
| src/logo_big.h | TRMNL logo 200x120 | Replace with ECHO wordmark |
| src/logo_medium.h | TRMNL logo 100x60 | Replace with ECHO wordmark |
| src/logo_small.h | TRMNL logo 50x30 | Replace with ECHO wordmark |
| src/loading.h | TRMNL loading spinner | Replace with ECHO animation frames |
| src/wifi_failed_qr.h | QR → help.trmnl.com | QR → shadowlab.cc/echo |
| src/wifi_connect_qr.h | QR → trmnl.com/start | QR → shadowlab.cc/echo/setup |
| sprites/ | Various TRMNL sprites | Audit and replace |

### Bitmap Generation Pipeline
```
SVG (design) → Pillow resize → Floyd-Steinberg 1-bit → C header array
```
Script needed: `echo/generate_bitmaps.py`

## Layer 3: Captive Portal HTML

| File | Content | Action |
|------|---------|--------|
| lib/wificaptive/src/WifiCaptivePortal.h | Portal SSID "ECHO" | Done |
| lib/wificaptive/src/*.cpp | HTML with TRMNL logo/branding | Replace HTML template |
| lib/wificaptive/src/*.h | CSS with TRMNL orange (#F96D1E) | Replace with ECHO palette |

### ECHO Captive Portal Design
- Background: #05050a (shadow dark)
- Accent: #8b5cf6 (violet)
- Text: #eaeaf2 (light)
- Font: system monospace
- Logo: ECHO wordmark (SVG inline)
- No external asset loads (ESP32 serves everything locally)

## Layer 4: API Dependencies

### MUST KEEP (we depend on TRMNL infrastructure)
| Endpoint | Purpose | Self-host? |
|----------|---------|------------|
| trmnl.com/api/display | Image fetch on wake | Future (needs CDN) |
| trmnl.com/api/setup | Device registration | Future (needs backend) |
| trmnl.com/api/log | Device logging | Future (optional) |
| trmnl.com/api/plugin_settings | Webhook image push | Future (needs backend) |

### CAN SELF-HOST NOW
| Endpoint | Purpose | ECHO equivalent |
|----------|---------|----------------|
| OTA firmware URL | Firmware updates | shadowlab.cc/firmware/echo.bin |
| Metadata JSON | Version check | shadowlab.cc/firmware/echo.json |
| Help/docs URLs | User support | shadowlab.cc/echo |

### FUTURE SELF-HOST (ECHO Backend)
When we build the ECHO backend (shadow-lab/apps/api), we can route:
- Device registration → our own API
- Image serving → our own CDN
- Logging → our own telemetry
- Plugin management → our own dashboard

## Layer 5: License Compliance

TRMNL firmware is **MIT licensed** (LICENSE file in repo).
MIT allows: modification, distribution, commercial use, private use.
Requirements: include original copyright notice + license text.

### Actions
- [x] Keep LICENSE file (MIT — original TRMNL copyright)
- [ ] Add ECHO-specific LICENSE.echo (Shadow Lab copyright for overlay)
- [ ] Add NOTICE file crediting TRMNL as upstream base
- [ ] Ensure all ECHO-added files have Shadow Lab copyright header

## Cleanroom Score

| Category | Total Items | Completed | Score |
|----------|-------------|-----------|-------|
| Strings | 5 | 5 | 100% |
| Bitmaps | 6 | 4 | 67% |
| Portal HTML | 3 | 3 | 100% |
| API separation | 6 | 2 | 33% |
| Legal | 4 | 1 | 25% |
| **Total** | **24** | **15** | **63%** |

Target: 100% before ECHO 0.1.0-beta
