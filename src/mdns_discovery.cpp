/**
 * @file mdns_discovery.cpp
 * @brief mDNS discovery for Shadow Lab mesh + TRMNL servers
 *
 * Uses Arduino ESP32 mDNS library to discover local content servers.
 * Advertises ECHO as a Shadow Lab mesh device via _shadowlab._tcp service.
 */

#include "mdns_discovery.h"
#include "config.h"
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

// ============================================================
// CONSTANTS
// ============================================================

#define MDNS_SERVICE_TYPE "http"
#define MDNS_SERVICE_PROTO "tcp"

// ============================================================
// STATE
// ============================================================

static struct {
    bool initialized;
    unsigned long last_scan;
    uint16_t scan_interval_ms;

    // Discovered servers cache
    DiscoveredServer servers[MAX_DISCOVERED_SERVERS];
    int server_count;
} s_mdns = {
    .initialized = false,
    .last_scan = 0,
    .scan_interval_ms = 60000,
    .server_count = 0,
};

extern Preferences preferences;

// ============================================================
// PUBLIC API
// ============================================================

bool mdns_discovery_init(void) {
    if (s_mdns.initialized) {
        return true;
    }

    // Start mDNS responder
    if (!MDNS.begin(get_device_id())) {
        return false;
    }

    // Shadow Lab: ECHO device identity
    MDNS.setInstanceName(SHADOW_DEVICE_CODENAME);

    // Advertise HTTP service + Shadow Lab mesh service
    MDNS.addService(MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO, 80);
    MDNS.addService("shadowlab", "tcp", 80);
    MDNS.addServiceTxt("shadowlab", "tcp", "device", SHADOW_DEVICE_CODENAME);
    MDNS.addServiceTxt("shadowlab", "tcp", "product", SHADOW_PRODUCT_NAME);
    MDNS.addServiceTxt("shadowlab", "tcp", "fw", SHADOW_FIRMWARE_TAG);

    s_mdns.initialized = true;
    log_i("[MDNS] Ready: %s.local", get_device_id());

    return true;
}

int mdns_discovery_browse(DiscoveredServer* servers, int max_servers) {
    if (!s_mdns.initialized) {
        if (!mdns_discovery_init()) {
            return 0;
        }
    }

    // Check if we need to refresh (cached results valid for scan_interval_ms)
    unsigned long now = millis();
    if (now - s_mdns.last_scan < s_mdns.scan_interval_ms && s_mdns.server_count > 0) {
        int count = (max_servers < s_mdns.server_count) ? max_servers : s_mdns.server_count;
        if (servers && count > 0) {
            memcpy(servers, s_mdns.servers, count * sizeof(DiscoveredServer));
        }
        return count;
    }

    // Perform new scan
    s_mdns.last_scan = now;
    s_mdns.server_count = 0;
    memset(s_mdns.servers, 0, sizeof(s_mdns.servers));

    // Query for HTTP services
    int n = MDNS.queryService(MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO);

    if (n > 0) {
        for (int i = 0; i < n && s_mdns.server_count < MAX_DISCOVERED_SERVERS; i++) {
            String hostname = MDNS.hostname(i);
            IPAddress ip = MDNS.IP(i);
            uint16_t port = MDNS.port(i);

            // Check if hostname contains "trmnl" (case-insensitive)
            bool is_trmnl = (hostname.indexOf("trmnl") >= 0 ||
                            hostname.indexOf("TRMNL") >= 0 ||
                            hostname.indexOf("Trmnl") >= 0);

            if (is_trmnl) {
                DiscoveredServer& server = s_mdns.servers[s_mdns.server_count];
                memset(&server, 0, sizeof(server));

                // Copy hostname
                int host_len = hostname.length();
                if (host_len > 0) {
                    strncpy(server.hostname, hostname.c_str(), sizeof(server.hostname) - 1);
                }

                // Copy IP address
                strncpy(server.ip_address, ip.toString().c_str(), sizeof(server.ip_address) - 1);
                server.port = port;
                server.is_valid = true;

                // Use hostname as instance name
                strncpy(server.instance_name, hostname.c_str(), sizeof(server.instance_name) - 1);

                s_mdns.server_count++;
                log_i("[MDNS] Found: %s @ %s:%d", server.hostname, server.ip_address, server.port);
            }
        }
    }

    // Copy results to output
    int count = (max_servers < s_mdns.server_count) ? max_servers : s_mdns.server_count;
    if (servers && count > 0) {
        memcpy(servers, s_mdns.servers, count * sizeof(DiscoveredServer));
    }

    return count;
}

bool mdns_get_local_server_url(char* server_url, size_t max_len) {
    if (!server_url || max_len == 0) {
        return false;
    }

    // Try cached server from preferences
    preferences.begin(PREFERENCES_NAMESPACE, true);
    String local_url = preferences.getString(PREFERENCES_LOCAL_SERVER_URL, "");
    preferences.end();

    if (local_url.length() > 0) {
        strncpy(server_url, local_url.c_str(), max_len - 1);
        server_url[max_len - 1] = '\0';
        log_i("[MDNS] Using cached: %s", server_url);
        return true;
    }

    // Scan for servers
    DiscoveredServer servers[MAX_DISCOVERED_SERVERS];
    int count = mdns_discovery_browse(servers, MAX_DISCOVERED_SERVERS);

    if (count > 0) {
        // Use first discovered server
        const DiscoveredServer& server = servers[0];
        snprintf(server_url, max_len, "http://%s:%d", server.ip_address, server.port);

        log_i("[MDNS] Using discovered: %s", server_url);

        // Cache for next time
        preferences.begin(PREFERENCES_NAMESPACE, false);
        preferences.putString(PREFERENCES_LOCAL_SERVER_URL, server_url);
        preferences.end();

        return true;
    }

    return false;
}

bool mdns_advertise_device(const char* device_name) {
    if (!s_mdns.initialized) {
        if (!mdns_discovery_init()) {
            return false;
        }
    }

    if (device_name && *device_name) {
        MDNS.setInstanceName(device_name);
    }

    log_i("[MDNS] Advertised as: %s", device_name ? device_name : "ECHO");
    return true;
}

void mdns_discovery_stop(void) {
    if (s_mdns.initialized) {
        MDNS.end();
        s_mdns.initialized = false;
        log_i("[MDNS] Stopped");
    }
}

const char* get_device_id(void) {
    static char device_id[32] = {0};

    if (device_id[0] == '\0') {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        snprintf(device_id, sizeof(device_id), "trmnl-%02x%02x%02x",
                 mac[3], mac[4], mac[5]);
    }

    return device_id;
}
