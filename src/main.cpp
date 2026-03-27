#include <Arduino.h>
#include "bl.h"
#include "esp_ota_ops.h"
#include "qa.h"
#include "mqtt_sprite.h"
#include "sprite_renderer.h"
#include "mdns_discovery.h"


void setup()
{
  bool testPassed = checkIfAlreadyPassed();
  if (!testPassed) {
    startQA();
  }
  esp_ota_mark_app_valid_cancel_rollback();
  bl_init();

  // Initialize mDNS discovery for local TRMNL servers
  mdns_discovery_init();

  // Initialize MQTT sprite system
  mqtt_sprite_init();

  // Initialize sprite renderer
  sprite_renderer_init();
}

void loop()
{
  bl_process();

  // Handle MQTT sprite messages
  if (mqtt_sprite_loop()) {
    // New sprite message received
    sprite_message_t msg;
    if (mqtt_sprite_get_message(&msg)) {
      log_i("[MQTT] State: %d, Message: %s", msg.state, msg.message);

      // Display sprite based on state
      if (msg.has_state) {
        if (msg.has_message) {
          sprite_display_with_message(msg.state, msg.message);
        } else {
          sprite_display_state(msg.state);
        }
      }
    }
  }

  // Update sprite animations
  sprite_update_animation();

  // Periodically scan for local TRMNL servers (every 60 seconds)
  static unsigned long last_mdns_scan = 0;
  if (millis() - last_mdns_scan > 60000) {
    last_mdns_scan = millis();

    DiscoveredServer servers[3];
    int count = mdns_discovery_browse(servers, 3);
    if (count > 0) {
      log_i("[MDNS] Found %d local TRMNL server(s)", count);
      for (int i = 0; i < count; i++) {
        log_i("[MDNS]   - %s @ %s:%d",
             servers[i].instance_name, servers[i].ip_address, servers[i].port);
      }
    }
  }
}
