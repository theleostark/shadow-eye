# ECHO Product Roadmap — From Prototype to Platform

**Date:** 2026-04-02 | **Author:** Shadow Lab | **Status:** Living document
**Hardware:** xteink X4 (ESP32-C3, SSD1677, 800x480, Frost White)

---

## Vision

ECHO is not a display. It's an **ambient intelligence surface** — a physical
location where the mesh becomes visible without asking. E-ink chosen for
persistence: updates survive power loss. The shadow, cast in permanent ink.

The product evolves through four phases, each building on the last.

---

## Phase 0: Prototype (CURRENT — ECHO 0.0.1-alpha)

**Status:** Built 2026-04-02. Pending flash.

### What exists
- TRMNL BYOD firmware with ECHO overlay (cleanroom 63%)
- 🌑 Moon mark brand identity
- WiFi WPA3 fix + retry loop + 30s timeout
- RTC WiFi channel caching (1-3s faster wake)
- GPIO sleep hardening (300-1200 uA saved)
- MQTT removal + IPv6 disable (60-100 KB flash saved)
- echo-field portable WiFi primitive
- CI/CD pipeline (GitHub Actions → shadowlab.cc OTA)
- Shadow Lab relay API (FastAPI on OCI, TRMNL-compatible proxy)
- TurboQuant rendering pipeline (Pillow → dither → webhook push)
- 6 echo primitives defined: surface, signal, pulse, frame, source, cast
- 6 enterprise transcription tools stubbed in shadow-enterprise-cli

### Metrics
- Binary: ~1,277 KB (68.8% of OTA bank)
- Deep sleep: ~50 uA target (down from 350-1250 uA with GPIO fix)
- Wake cycle: ~4-8s nominal (down from 8-15s with channel cache)
- Cleanroom: 63% (15/24 items)

### Gaps
- Device not yet flashed with latest firmware
- echo-field WiFi untested on device
- No on-device AI running yet (Kernel Whisper is spec only)
- SD card reader not implemented
- Captive portal still TRMNL-branded visually (HTML patch ready but not compiled in)

---

## Phase 1: Connected (ECHO 0.1.0-beta)

**Goal:** Reliable daily driver. ECHO wakes, connects, renders, sleeps — every time.

### 1.1 Adaptive Firmware Engine
- [ ] EchoContext struct in RTC memory
- [ ] 7 strategies: FULL, HOTSPOT, OFFLINE, ZEN, PORTAL, OTA, FIRST_BOOT
- [ ] select_strategy() decision tree
- [ ] Adaptive sleep duration (battery-aware, connectivity-aware)
- [ ] Hotspot detection (172.20.10.x = iPhone, 192.168.43.x = Android)

### 1.2 Display Optimization
- [ ] Implement SSD1677 temperature sensor read (currently stubbed at 22°C)
- [ ] Temperature-to-waveform LUT mapping (<10°C slow, >35°C fast)
- [ ] Image delta skip (hash comparison, skip render if unchanged)
- [ ] Atkinson dithering option (less diffusion = cleaner text on e-paper)
- [ ] Damage tracking for partial refresh (only update changed regions)

### 1.3 Network Hardening
- [ ] DNS caching in RTC memory (save 0.5-10s per wake)
- [ ] HTTP keep-alive across API calls in same wake cycle (save 2-6s TLS)
- [ ] Consolidated log submission (1 POST per wake, not 3-4)
- [ ] Fallback IP for shadowlab.cc (hardcoded, skip DNS on failure)

### 1.4 Cleanroom Completion
- [ ] loading.h spinner replacement (ECHO brand animation frames)
- [ ] wifi_connect_qr.h → shadowlab.cc/echo/setup
- [ ] NOTICE file (MIT license, TRMNL attribution)
- [ ] Shadow Lab copyright headers on all echo/ files
- [ ] Captive portal HTML compiled into firmware (WifiCaptivePage.h.patch → live)
- [ ] Target: cleanroom 100%

### Metrics Target
- Cleanroom: 100%
- Wake reliability: >99% (connect + render + sleep without crash)
- Binary: <1,200 KB (after MQTT + IPv6 savings confirmed)
- Battery: 21+ days (with GPIO fix + channel cache + adaptive sleep)

---

## Phase 2: Intelligent (ECHO 0.2.0)

**Goal:** ECHO becomes context-aware. On-device intelligence, SD card memory, multi-source rendering.

### 2.1 echo-memory (SD Card Primitive)
- [ ] SD card reader via SPI (ESP32-C3 has second SPI bus)
- [ ] Read echo.md config on boot (WiFi creds, display prefs, mesh role)
- [ ] Read context/*.md for offline rendering
- [ ] Read prompts/*.md for Kernel Whisper input
- [ ] Cache last frame to SD (faster offline fallback than SPIFFS)
- [ ] Physical context injection as product feature

### 2.2 On-Device ML (Kernel Whisper v1)
- [ ] Evaluate TinyMaix vs TFLite Micro vs ESP-DL for ESP32-C3 RISC-V
- [ ] Keyword spotting model (<100 KB, 8-bit quantized)
  - "ECHO" wake word → trigger immediate refresh
  - "UPDATE" → force OTA check
  - Button + voice = richer input vocabulary
- [ ] Anomaly detection on battery curve (predict low-battery before it happens)
- [ ] Pattern learning from button press timing (predict user preference)

### 2.3 Multi-Source Rendering
- [ ] Source multiplexer: mesh API | webhook | SD card | on-device generate
- [ ] Priority system: live data > cached > generated > error screen
- [ ] Templating engine: simple Liquid-like on-device (reuse TRMNL framework concepts)
- [ ] Multiple display zones (header: static, body: rotating, footer: status)

### 2.4 ESP-NOW Mesh
- [ ] ECHO ↔ ECHO peer communication without WiFi
- [ ] Use case: desk ECHO sends button event to wall ECHO
- [ ] 250-byte packets sufficient for commands + small status updates
- [ ] Coexists with WiFi (ESP32-C3 supports both simultaneously)

### 2.5 BLE Phone Bridge
- [ ] BLE service for phone → ECHO image transfer
- [ ] Use case: push image from phone when no WiFi available
- [ ] BLE provisioning: tap phone to configure WiFi (instead of captive portal)
- [ ] Companion app concept (or Web Bluetooth from shadowlab.cc/echo)

---

## Phase 3: Platform (ECHO 1.0.0)

**Goal:** ECHO becomes a product others can build on. Multi-device fleet, developer SDK, enterprise features.

### 3.1 ECHO Fleet Management
- [ ] Multi-device registry in Shadow Lab API
- [ ] Per-device display profiles (what each ECHO shows)
- [ ] Fleet-wide OTA with staged rollout (canary → 10% → 50% → 100%)
- [ ] Telemetry dashboard on shadowlab.cc (battery, RSSI, uptime per device)
- [ ] Device groups (e.g., "office displays" vs "personal devices")

### 3.2 Developer SDK
- [ ] echo-sdk: Python/JS library for pushing frames to ECHO
- [ ] Plugin system (TRMNL-compatible but self-hosted)
- [ ] Liquid template rendering (server-side, same as TRMNL framework)
- [ ] Webhook API with authentication (API key per device)
- [ ] CLI tool: `echo push image.png` → renders on device

### 3.3 Enterprise Features
- [ ] Live transcription display (transcribe.display primitive)
  - Whisper on FRIDAY M2 → stream text → render to ECHO frame
  - Bilingual JP↔EN with domain vocabulary (Astemo terms)
  - Action item extraction post-meeting
- [ ] Meeting mode: agenda + action items rendered on desk ECHO
- [ ] Schedule display: Outlook calendar → daily view on e-paper
- [ ] Document display: render .md/.pdf previews for review

### 3.4 Claw Personalities
- [ ] NullClaw: STRATEGY_ZEN — minimal, battery critical
- [ ] ZeroClaw: STRATEGY_OFFLINE — zero network, SD/cache only
- [ ] IronClaw: STRATEGY_FULL — all primitives, aggressive refresh
- [ ] Map personality to display style (NullClaw = blank, IronClaw = dense dashboard)
- [ ] Personality persisted on SD card (persona.md)

### 3.5 Display Hardware Expansion
- [ ] Support for TRMNL X (1872x1404, 16-gray, ESP32-S3)
- [ ] Support for color e-paper (Waveshare 7-color, ACeP)
- [ ] Support for larger panels (10.3", 13.3" for wall mounting)
- [ ] Modular display driver abstraction (SSD1677, IT8951, UC8159)

---

## Phase 4: Ecosystem (ECHO 2.0.0)

**Goal:** ECHO devices form a self-organizing ambient intelligence network.

### 4.1 ECHO Mesh Protocol
- Multi-device mesh via ESP-NOW (no WiFi infrastructure needed)
- Leader election: one ECHO bridges to WiFi, shares with mesh peers
- Content distribution: one fetch, broadcast to N displays
- Mesh topology visible on shadowlab.cc dashboard

### 4.2 Physical Computing
- NFC tap to configure (phone → ECHO provisioning)
- LoRa bridge for remote displays (field deployments)
- Solar power option (e-paper's zero-power-to-maintain enables solar-only operation)
- Environmental sensors (SCD41 CO2/temp/humidity already on board)

### 4.3 AI-Native Display
- On-device LLM (when ESP32 successors have more RAM)
- Context-aware content generation (no server needed)
- Federated learning across ECHO fleet (each device contributes to model improvement)
- Voice interaction via external mic module

### 4.4 Open Source
- ECHO firmware fully open-source (MIT, like TRMNL base)
- ECHO SDK + templates published
- Community plugin ecosystem
- shadowlab.cc as reference implementation, not lock-in

---

## Technology Bets

| Bet | Timeframe | Risk | Payoff |
|-----|-----------|------|--------|
| Adaptive firmware (strategies) | Phase 1 | Low | 2-4x battery improvement |
| SD card as context store | Phase 2 | Low | Offline-first, physical portability |
| ESP-NOW mesh | Phase 2 | Medium | Multi-ECHO without infrastructure |
| On-device keyword spotting | Phase 2 | Medium | Voice input on a $69 device |
| BLE image transfer | Phase 2 | Low | No-WiFi image push from phone |
| Custom waveform LUTs | Phase 1 | Medium | 2-5x faster refresh, better gray |
| Whisper live transcription | Phase 3 | Low (server-side) | Enterprise killer feature |
| LoRa bridge | Phase 4 | High | Truly remote displays |
| Solar-powered ECHO | Phase 4 | Medium | Zero-maintenance ambient surface |

---

## Competitive Positioning

```
                    CONNECTIVITY
                    ↑
                    |
    Tidbyt ●        |        ● TRMNL X
    (LED, cloud)    |        (e-paper, cloud+BYOD)
                    |
    DAKboard ●      |    ● ECHO (us)
    (LCD, cloud)    |    (e-paper, mesh+cloud+offline)
                    |
                    |        ● Inkplate
                    |        (e-paper, WiFi, open-source)
  ──────────────────+────────────────────── INTELLIGENCE
    Kindle ●        |        ● reMarkable
    (e-paper,       |        (e-paper, stylus,
     static)        |         cloud sync)
                    |
                    ↓
                    STANDALONE
```

ECHO's unique position: **mesh-connected + offline-capable + ambient intelligence**.
No other product combines e-paper + mesh networking + on-device AI + physical context (SD card).

---

## Patent Opportunities

| ID | Concept | Phase | Status |
|----|---------|-------|--------|
| SP-015 | Absolute addressing mode (e-paper as persistent display surface) | 0 | Filed |
| SP-017 | Kernel Whisper (on-device ternary GPT for e-paper) | 2 | Filed |
| SP-018 | Configuration evolution (adaptive firmware strategies) | 1 | Filed |
| NEW | Physical context injection via SD card .md files | 2 | Candidate |
| NEW | ESP-NOW display mesh (multi-ECHO content distribution) | 2 | Candidate |
| NEW | Temperature-adaptive waveform selection for e-paper | 1 | Candidate |
| NEW | Hotspot detection via DHCP IP range classification | 1 | Candidate |
| NEW | RTC-cached WiFi reconnection for battery optimization | 1 | Implemented |

---

## Success Metrics

| Metric | Phase 0 | Phase 1 | Phase 2 | Phase 3 |
|--------|---------|---------|---------|---------|
| Battery life | ~7 days | 21+ days | 30+ days | 60+ days (solar) |
| Wake cycle | 8-15s | 4-6s | 2-4s | <2s |
| Cleanroom | 63% | 100% | 100% | 100% |
| Display modes | 2 (1-bit, 4-gray) | 4 (+partial, +temp-comp) | 6 (+zones, +templates) | 8+ |
| Input methods | 1 (button) | 2 (+SD card) | 4 (+BLE, +ESP-NOW) | 6+ (+NFC, +voice) |
| Devices | 1 | 1 | 2-5 | 10+ fleet |
| Data sources | 1 (webhook) | 3 (+SD, +cache) | 5 (+BLE, +mesh) | 7+ |
