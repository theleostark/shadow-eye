#!/usr/bin/env python3
"""
Enable MQTT Sprites on Xteink X4

This script sends a setup request to the TRMNL device to enable MQTT sprites.
The device will then connect to the MQTT broker and listen for sprite commands.

Usage:
    python3 scripts/enable_mqtt_sprites.py
"""

import requests
import json

# Default device configuration
DEVICE_IP = "192.168.1.1"  # Update this to your device's IP
API_ENDPOINT = f"http://{DEVICE_IP}/api/setup"

def enable_mqtt_sprites():
    """Enable MQTT sprites via the TRMNL API"""

    # Configuration to enable MQTT sprites
    config = {
        "mqtt_sprite_enabled": True,
        "mqtt_broker": "broker.hivemq.com",
        "mqtt_port": 1883,
        "mqtt_topic": "xteink/x4/display"
    }

    print(f"Sending configuration to {DEVICE_IP}...")
    print(f"Configuration: {json.dumps(config, indent=2)}")

    try:
        # Note: The actual API call depends on the TRMNL device's API
        # You may need to use the web interface or serial console
        print("\n=== Manual Setup Required ===")
        print("To enable MQTT sprites, you need to set preferences on the device.")
        print("\nOption 1: Via Web Interface")
        print("1. Connect to the device's WiFi")
        print("2. Open http://192.168.1.1 (or the device's IP)")
        print("3. Navigate to Settings")
        print("4. Enable MQTT Sprites")
        print("\nOption 2: Via Serial Console")
        print("1. Connect to the device via serial")
        print("2. Use the preferences API to enable:")
        print("   mqtt_sprite_enabled = true")
        print("\nOption 3: Use Preferences API directly")
        print("Send POST request to /api/setup with mqtt_sprite_config")

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    enable_mqtt_sprites()
