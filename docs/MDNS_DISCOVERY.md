# Local Network TRMNL Server Discovery

## Overview

The Xteink X4 can automatically discover local TRMNL content servers on your network using mDNS/Bonjour service discovery. This allows the device to work with local servers when cloud services are unavailable or when you want to host content locally.

## How It Works

1. **Device Advertisement**: The Xteink X4 advertises itself as `trmnl-XXXXXX.local` where XXXXXX is the last 3 bytes of the MAC address
2. **Server Discovery**: The device scans for HTTP services with "trmnl" in the hostname
3. **Automatic Fallback**: When a local server is found, its URL is cached and used for future connections

## Device Information

When connected to WiFi, the device is reachable at:
```
trmnl-<mac_suffix>.local
```

Example: `trmnl-a1b2c3.local`

The suffix is derived from the last 3 octets of the device's MAC address.

## Setting Up a Local TRMNL Server

### Option 1: Python HTTP Server

Use the included script to run a simple HTTP server:

```bash
python3 scripts/local_trmnl_server.py
```

This server will advertise itself via mDNS if supported on your platform.

### Option 2: Any Device on Your Network

Any HTTP server on your local network can be used. To be discovered:

1. The hostname should contain "trmnl" (case-insensitive)
2. The server should respond to `/api/display` endpoint

Example: Your computer at `trmnl-server.local` or `mytrmnl-pc.local`

### Option 3: Manual Configuration

You can manually configure a local server URL via preferences:

```cpp
preferences.putString("local_server", "http://192.168.1.100:8000");
```

Or via the web UI (if available).

## API

### Initialization

```cpp
#include "mdns_discovery.h"

// Initialize mDNS (usually called in setup())
mdns_discovery_init();
```

### Browsing for Servers

```cpp
DiscoveredServer servers[5];
int count = mdns_discovery_browse(servers, 5);

for (int i = 0; i < count; i++) {
    Serial.printf("Found: %s @ %s:%d\n",
        servers[i].instance_name,
        servers[i].ip_address,
        servers[i].port);
}
```

### Get Local Server URL

```cpp
char url[128];
if (mdns_get_local_server_url(url, sizeof(url))) {
    Serial.printf("Using local server: %s\n", url);
    // Use url for API calls
}
```

### Advertise Device Name

```cpp
mdns_advertise_device("My-Xteink-X4");
```

### Stop Discovery

```cpp
mdns_discovery_stop();
```

## Periodic Scanning

The device automatically scans for local servers every 60 seconds. When a server is found:
1. It is logged to the serial console
2. The URL is cached in preferences
3. The cached URL is used on subsequent boots

## Testing mDNS

### On macOS

```bash
# List all mDNS services
dns-sd -B _http._tcp local

# Browse for a specific device
dns-sd -L trmnl-a1b2c3.local
```

### On Linux

```bash
# Install avahi-utils
sudo apt-get install avahi-utils

# Browse for HTTP services
avahi-browse -r _http._tcp

# Resolve a hostname
avahi-resolve trmnl-a1b2c3.local
```

### On Windows

```bash
# List mDNS services
mdns-sd.exe -B _http._tcp local
```

Or use a third-party tool like "Bonjour Browser".

## Network Requirements

- mDNS works on the local network (LAN) only
- All devices must be on the same subnet
- Router/switch must not block mDNS (UDP port 5353)
- Multicast must be enabled on your network

## Troubleshooting

### Not Discovering Servers

1. **Check network**: Ensure device and server are on the same subnet
2. **Check hostname**: Server hostname must contain "trmnl"
3. **Check firewall**: Ensure UDP 5353 is not blocked
4. **Check scan interval**: Device scans every 60 seconds, wait for next scan

### Device Not Reachable

1. **Check mDNS is running**: Look for `[MDNS] Ready: trmnl-XXXXXX.local` in logs
2. **Check WiFi connection**: Device must be connected to WiFi
3. **Try ping**: From another device, try `ping trmnl-XXXXXX.local`

### Clearing Cached Server

To clear the cached local server URL:

```cpp
preferences.begin("trmnl_x4", false);
preferences.putString("local_server", "");
preferences.end();
```

## Files

| File | Description |
|------|-------------|
| `include/mdns_discovery.h` | mDNS discovery API |
| `src/mdns_discovery.cpp` | Implementation |
| `scripts/local_trmnl_server.py` | Example local server |
