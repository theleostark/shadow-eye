# WiFi Hotspot Compatibility Audit — ECHO Firmware

**Date:** 2026-04-02
**Target:** iPhone "echo-field" personal hotspot
**Board:** ESP32-C3 (esp32-c3-devkitc-02), Arduino + ESP-IDF framework
**Platform:** espressif32@6.12.0

---

## 1. Root Cause Analysis

The phone hotspot connection failure is likely caused by **multiple compounding issues**, not a single root cause. In order of probability:

### 1A. DHCP Timeout Too Short (HIGH probability)

**File:** `lib/wificaptive/src/WifiCaptive.h:28`
```
#define CONNECTION_TIMEOUT 15000
```

The `waitForConnectResult()` in `connect.cpp:307-331` polls `WiFi.status()` for 15 seconds, breaking on `WL_CONNECTED` or `WL_CONNECT_FAILED`. iPhone hotspots have notoriously slow DHCP servers (3-8 seconds for DHCP alone), and there is a race between WiFi association and DHCP completion.

**Critical code smell** at `connect.cpp:321-322`:
```cpp
// @todo detect additional states, connect happens, then dhcp then get ip,
// there is some delay here, make sure not to timeout if waiting on IP
if (status == WL_CONNECTED || status == WL_CONNECT_FAILED)
```

The existing TODO comment acknowledges that `WL_CONNECTED` may fire before the IP is actually assigned. On ESP32 Arduino, `WL_CONNECTED` is set after L2 association — DHCP happens asynchronously afterward. For iPhone hotspots, the sequence is:

1. WiFi association (2-5s)
2. `WL_CONNECTED` fires
3. DHCP request sent
4. iPhone DHCP server responds (3-8s, sometimes more when waking from sleep)
5. `ARDUINO_EVENT_WIFI_STA_GOT_IP` fires

The 15s timeout can expire between step 2 and step 5 if the phone is slow to respond. However, `waitForConnectResult()` actually returns `WL_CONNECTED` at step 2, so the timeout *should not* be the primary issue unless the WiFi association itself is slow (which it can be with hotspots — see 1B).

### 1B. WPA3/PMF Negotiation Failure (HIGH probability)

**No `esp_wifi_set_protocol()` is called anywhere in the codebase.** This means the ESP32-C3 uses the default protocol mask, which on ESP-IDF 5.x (used by espressif32@6.12.0) includes 802.11b/g/n.

iPhone personal hotspots on iOS 15+ default to **WPA3 Personal** (or WPA2/WPA3 Transitional) with **Protected Management Frames (PMF)** required. The ESP32-C3 supports WPA3 in hardware, but the Arduino WiFi library's `WiFi.begin()` does not explicitly negotiate WPA3 auth mode — it relies on ESP-IDF auto-detection.

**The problem:** `WiFi.setMinSecurity(WIFI_AUTH_OPEN)` is only called in `autoConnect()` at `WifiCaptive.cpp:536`. It is **NOT called** in:
- `startPortal()` (captive portal flow) at `WifiCaptive.cpp:107`
- `connect()` at `WifiCaptive.cpp:209`
- `initiateConnectionAndWaitForOutcome()` at `connect.cpp:161`

Without `setMinSecurity()`, the default minimum security is `WIFI_AUTH_WPA2_PSK`. If the iPhone advertises as WPA3-only (not transitional), the ESP32 may reject the AP because it doesn't meet WPA3 auth requirements, or the PMF negotiation may fail silently.

### 1C. Captive Portal Mode Conflict (MEDIUM probability)

**File:** `WifiCaptive.cpp:22` — Portal starts in `WIFI_MODE_AP` mode:
```cpp
WiFi.mode(WIFI_MODE_AP);
```

When user submits credentials at `WifiCaptive.cpp:107`, `connect()` is called, which calls `WiFi.enableSTA(true)` at `WifiCaptive.cpp:215`. This transitions from AP-only to AP+STA mode.

At `WifiCaptive.cpp:41-46`, the portal **reinitializes the entire WiFi stack**:
```cpp
esp_wifi_stop();
esp_wifi_deinit();
wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
my_config.ampdu_rx_enable = false;
esp_wifi_init(&my_config);
esp_wifi_start();
```

This AMPDU-disable workaround (for Android captive portal bug) **replaces the WiFi configuration**. Any subsequent `WiFi.begin()` call may not properly re-negotiate with the AP if the AMPDU setting interferes with 802.11n required by the hotspot.

### 1D. No Retry in Portal Flow (CONFIRMED — design issue)

**File:** `WifiCaptive.cpp:83-126`

The portal connection loop has **exactly one attempt**. If `connect()` fails:
```cpp
} else {
    _ssid = "";
    _password = "";
    _enterprise_credentials = WifiCredentials{};
    WiFi.disconnect();
    WiFi.enableSTA(false);
    break;  // <-- exits the loop immediately
}
```

Compare this with `autoConnect()` which calls `tryConnectWithRetries()` (up to 3 attempts with exponential backoff). The portal flow gets **zero retries**. For a phone hotspot that may take a moment to accept the new client, this single-shot approach is insufficient.

### 1E. AP Teardown Race (LOW-MEDIUM probability)

After the portal loop exits (success or fail), at `WifiCaptive.cpp:129-132`:
```cpp
WiFi.scanDelete();
WiFi.softAPdisconnect(true);
delay(1000);
```

Then at line 134 it checks `WiFi.status()`. If the STA connection was briefly established but the softAP disconnect caused a transient state change, the reconnection attempt at lines 140-151 may also fail because `_enterprise_credentials` was already cleared on failure (line 119).

---

## 2. WiFi Channel/Scanning Analysis

**Scan behavior:** The portal starts an async scan at `WifiCaptive.cpp:79`:
```cpp
WiFi.scanNetworks(true);
```

The scan runs while in AP mode. The AP is fixed to channel 6 (`WifiCaptive.h:22`). iPhones hotspots typically use **channel 1, 6, or 11** (or 44, 149 in 5GHz). Since ESP32-C3 only supports 2.4GHz, it should see the hotspot if it's on any 2.4GHz channel.

**Hidden SSID:** The `findNetwork()` function at `WifiCaptive.cpp:648-681` does a standard scan. Hidden SSIDs appear as empty strings in scan results. If the hotspot is hidden, it will not appear in the portal list. However, `WiFi.begin(ssid, password)` can still connect to hidden networks — the user just can't select it from the list. The portal **does not offer manual SSID entry** (the web form only shows scanned networks). This is a secondary issue.

---

## 3. Static IP Path Analysis

**File:** `connect.cpp:25-121`

`configureStaticIP()` fully bypasses DHCP:
- Sets IP, gateway, subnet, DNS via `WiFi.config()`
- Explicitly stops DHCP client via `esp_netif_dhcpc_stop()`

For iPhone hotspots, the default range is `172.20.10.0/28`:
- Gateway: `172.20.10.1`
- DHCP range: `172.20.10.2` to `172.20.10.14`
- Only 13 usable addresses

Static IP **would bypass the DHCP timing issue** but requires the user to know the iPhone's IP range. This is a viable workaround for power users.

---

## 4. Error Diagnostics

The `captureEventData()` function at `connect.cpp:134-159` logs:
- `ARDUINO_EVENT_WIFI_STA_GOT_IP` — IP and gateway
- `ARDUINO_EVENT_WIFI_STA_CONNECTED` — SSID, BSSID, channel, **authmode**
- `ARDUINO_EVENT_WIFI_STA_DISCONNECTED` — **disconnect reason code**
- All other events by name

The disconnect reason code (`wifi_err_reason_t`) is the most diagnostic. Common codes for hotspot failures:
- `WIFI_REASON_NO_AP_FOUND` (201) — hotspot not visible
- `WIFI_REASON_AUTH_FAIL` (202) — WPA3/PMF negotiation failed
- `WIFI_REASON_ASSOC_FAIL` (203) — association rejected
- `WIFI_REASON_HANDSHAKE_TIMEOUT` (204) — 4-way handshake timeout
- `WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT` (15) — WPA handshake timeout

**Problem:** These events are only logged to serial. The captive portal web UI receives only a simple success/fail response (`WebServer.cpp:175-176`). The user never sees the actual failure reason.

---

## 5. Proposed Fixes

### Quick Fixes (apply now)

**QF-1: Add `setMinSecurity` before every connection attempt**

`connect.cpp:280`, before `WiFi.begin()`:
```cpp
// line 280: after WiFi.mode(WIFI_STA);
WiFi.setMinSecurity(WIFI_AUTH_OPEN);  // ADD THIS
```

Also in the enterprise path, after `WiFi.mode(WIFI_STA)` at line 201.

This ensures WPA3 and WPA2/WPA3 transitional networks are not rejected.

**QF-2: Increase CONNECTION_TIMEOUT for all paths**

`WifiCaptive.h:28`:
```
#define CONNECTION_TIMEOUT 15000  // CHANGE TO:
#define CONNECTION_TIMEOUT 30000
```

30 seconds gives iPhone DHCP enough time even in worst-case scenarios.

**QF-3: Add retry logic in the portal connection flow**

`WifiCaptive.cpp:107` — replace single `connect()` call with retry loop:
```cpp
// Replace line 107:
//   bool res = connect(credentials) == WL_CONNECTED;
// With:
bool res = false;
for (int attempt = 0; attempt < WIFI_CONNECTION_ATTEMPTS && !res; attempt++) {
    if (attempt > 0) {
        WiFi.disconnect();
        delay(2000 * (1 << (attempt - 1)));
        Log_info("Portal: retry attempt %d", attempt + 1);
    }
    res = connect(credentials) == WL_CONNECTED;
}
```

### Proper Fixes (needs more work)

**PF-1: Separate L2 association from DHCP/IP in timeout logic**

Rewrite `waitForConnectResult()` to have two phases:
1. Wait for `WL_CONNECTED` (L2 association) — 15s timeout
2. Wait for `ARDUINO_EVENT_WIFI_STA_GOT_IP` — additional 20s timeout

This prevents false timeouts when association succeeds but DHCP is slow.

**PF-2: Return error reason to captive portal UI**

Modify the `/connect` endpoint to:
1. Accept the credentials
2. Return 202 (accepted) immediately
3. Add a `/connect-status` polling endpoint that returns:
   - Connection state
   - Disconnect reason code (human-readable)
   - IP address on success

This lets the portal UI show "Connecting...", "Association failed (reason: auth_fail)", etc.

**PF-3: Explicitly set WiFi protocol and PMF mode**

Add to `initiateConnectionAndWaitForOutcome()` before `WiFi.begin()`:
```cpp
// Enable all 2.4GHz protocols including 802.11n
esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

// Enable PMF (required for WPA3 / iPhone hotspots)
wifi_config_t wifi_config;
esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
wifi_config.sta.pmf_cfg.capable = true;
wifi_config.sta.pmf_cfg.required = false;  // capable but not required
esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
```

**PF-4: Add manual SSID entry to captive portal**

For hidden hotspots or SSIDs not appearing in scan results, allow the user to type an SSID manually in the portal web form.

**PF-5: Separate AMPDU disable from WiFi init**

The current approach of `esp_wifi_stop() / esp_wifi_deinit() / esp_wifi_init()` at portal startup is heavy-handed. Use `esp_wifi_set_config()` to disable AMPDU without tearing down the stack, or at minimum, ensure the re-init does not interfere with subsequent STA connections.

---

## 6. Recommended Test Plan

1. Apply QF-1 + QF-2 + QF-3
2. Flash to device
3. Reset WiFi credentials (trigger captive portal)
4. Connect to iPhone "echo-field" hotspot
5. Monitor serial output — capture the `authmode` from `STA_CONNECTED` event and any disconnect reason codes
6. If still failing, the serial logs will indicate whether it's auth (WPA3/PMF) or DHCP timing
7. If auth-related, apply PF-3

---

## 7. File Reference Index

| File | Lines | Issue |
|------|-------|-------|
| `lib/wificaptive/src/WifiCaptive.h` | 28 | `CONNECTION_TIMEOUT` = 15000ms (too short) |
| `lib/wificaptive/src/WifiCaptive.h` | 26 | `WIFI_CONNECTION_ATTEMPTS` = 3 (not used in portal) |
| `lib/wificaptive/src/connect.cpp` | 280-286 | No `setMinSecurity()` before `WiFi.begin()` |
| `lib/wificaptive/src/connect.cpp` | 307-331 | `waitForConnectResult()` — no L2/DHCP separation |
| `lib/wificaptive/src/connect.cpp` | 321 | Existing TODO acknowledges the DHCP race |
| `lib/wificaptive/src/WifiCaptive.cpp` | 83-126 | Portal loop — single attempt, no retry |
| `lib/wificaptive/src/WifiCaptive.cpp` | 107 | `connect()` called once, fail = exit portal |
| `lib/wificaptive/src/WifiCaptive.cpp` | 116-123 | On failure: clears creds, breaks, shows error |
| `lib/wificaptive/src/WifiCaptive.cpp` | 41-46 | AMPDU disable reinits entire WiFi stack |
| `lib/wificaptive/src/WifiCaptive.cpp` | 536 | `setMinSecurity` only in `autoConnect()` |
| `lib/wificaptive/src/WebServer.cpp` | 129-176 | `/connect` handler — no status feedback |
