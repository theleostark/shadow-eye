/**
 * @file mdns_discovery.h
 * @brief mDNS/Bonjour service discovery for local TRMNL servers
 *
 * Allows Xteink X4 device to automatically discover local TRMNL servers
 * advertising via mDNS (e.g., trmnl-server.local)
 */

#ifndef MDNS_DISCOVERY_H
#define MDNS_DISCOVERY_H

#include <Arduino.h>
#include <Preferences.h>

// mDNS service type for TRMNL servers
#define MDNS_SERVICE_TYPE "_trmnl._tcp"
#define MDNS_SERVICE_INSTANCE "TRMNL-Local-Server"

// Maximum number of discovered servers
#define MAX_DISCOVERED_SERVERS 5

/**
 * @brief Discovered server information
 */
struct DiscoveredServer {
    char hostname[64];      // e.g., "trmnl-server.local"
    char ip_address[16];    // e.g., "192.168.1.100"
    uint16_t port;          // e.g., 8000
    char instance_name[64]; // Service instance name
    bool is_valid;
};

/**
 * @brief Initialize mDNS discovery
 *
 * Starts mDNS responder and begins browsing for TRMNL servers
 *
 * @return true on success
 */
bool mdns_discovery_init(void);

/**
 * @brief Update discovered servers list
 *
 * Should be called periodically to refresh the list of available servers
 *
 * @param servers Output array for discovered servers
 * @param max_servers Maximum number of servers to return
 * @return Number of servers discovered
 */
int mdns_discovery_browse(DiscoveredServer *servers, int max_servers);

/**
 * @brief Get the first available local server
 *
 * Convenience function to get a single server URL
 *
 * @param server_url Output buffer for server URL (e.g., "http://192.168.1.100:8000")
 * @param max_len Maximum length of output buffer
 * @return true if a server was found
 */
bool mdns_get_local_server_url(char *server_url, size_t max_len);

/**
 * @brief Advertise this device as a TRMNL client (optional)
 *
 * Allows other devices to discover this Xteink X4
 *
 * @param device_name Custom name for this device
 * @return true on success
 */
bool mdns_advertise_device(const char *device_name);

/**
 * @brief Stop mDNS discovery and cleanup
 */
void mdns_discovery_stop(void);

/**
 * @brief Get the device ID (mDNS hostname)
 * @return Device ID string (e.g., "trmnl-aabbcc")
 */
const char* get_device_id(void);

#endif // MDNS_DISCOVERY_H
