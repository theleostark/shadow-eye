# ECHO Firmware Changelog

All versions follow ECHO's independent versioning (not TRMNL).

## ECHO 0.0.1-alpha (unreleased)

**Base:** TRMNL 1.7.8 | **Target:** xteink X4 Frost White

### Identity
- [ ] Captive portal SSID: "ECHO" (done in WifiCaptive.h)
- [ ] mDNS: "echo.local" with `_shadowlab._tcp` service
- [ ] OTA endpoint: shadowlab.cc/firmware/echo.bin
- [ ] Preferences namespace: "shadow_echo"

### Rebrand (5 critical findings)
- [ ] `display.cpp:1542` — "TRMNL firmware" → "ECHO firmware"
- [ ] `display.cpp:1550` — WiFi error message cleanup
- [ ] `display.cpp:1554` — Help text → shadowlab.cc/echo
- [ ] `display.cpp:2014` — Second firmware label instance
- [ ] `mdns_discovery.cpp:105` — "trmnl" hostname match → "echo"

### Cleanroom TODO
- [ ] Replace TRMNL logo bitmaps (logo_big.h, logo_medium.h, logo_small.h)
- [ ] Replace WiFi QR code (wifi_failed_qr.h → shadowlab.cc/echo)
- [ ] Replace WiFi connect QR (wifi_connect_qr.h)
- [ ] Replace loading animation sprites (loading.h)
- [ ] Captive portal HTML rebrand (lib/wificaptive/)
- [ ] Serial boot banner → ECHO ASCII art

### Pipeline
- [x] echo/version.json — independent version manifest
- [x] echo/rebrand.py — automated string replacement engine
- [x] .github/workflows/echo-ci.yml — 4-stage CI/CD
- [ ] GitHub Actions secrets configured (DEPLOY_KEY, USER, HOST)

### Primitives (from shadow-echo product spec)
- [x] echo-field — portable WiFi connectivity (keychain stored)
- [ ] echo-surface — display abstraction layer
- [ ] echo-pulse — configurable wake cycle
- [ ] echo-frame — render pipeline abstraction
- [ ] echo-source — data source multiplexer
- [ ] echo-cast — webhook push abstraction
