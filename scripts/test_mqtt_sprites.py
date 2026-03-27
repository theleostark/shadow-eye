#!/usr/bin/env python3
"""
Test script for MQTT sprite system.
Publishes test messages to trigger sprite state changes on the Xteink X4.
"""

import json
import time
import paho.mqtt.client as mqtt

# MQTT configuration
BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC = "xteink/x4/display"

# Test messages for each state
TEST_MESSAGES = [
    {"state": "idle", "message": "Moltbot is resting"},
    {"state": "thinking", "message": "Processing your request..."},
    {"state": "talking", "message": "Hello! I'm Moltbot!"},
    {"state": "excited", "message": "That's amazing!"},
    {"state": "sleeping", "message": "Zzz..."},
    {"state": "error", "message": "Something went wrong"},
    {"state": "alert", "message": "Attention required!"},
    {"state": "working", "message": "Working on it..."},
]

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to {BROKER}:{PORT}")
        print(f"Publishing to topic: {TOPIC}")
    else:
        print(f"Connection failed with code {rc}")

def on_publish(client, userdata, mid):
    print(f"Message {mid} published")

def main():
    client = mqtt.Client(client_id="moltbot-test-publisher")
    client.on_connect = on_connect
    client.on_publish = on_publish

    print(f"Connecting to {BROKER}:{PORT}...")
    client.connect(BROKER, PORT, keepalive=60)

    client.loop_start()

    # Wait for connection
    time.sleep(2)

    print("\n=== Sending test messages ===\n")

    for i, msg in enumerate(TEST_MESSAGES, 1):
        payload = json.dumps(msg)
        result = client.publish(TOPIC, payload)

        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"[{i}/{len(TEST_MESSAGES)}] Sent: {msg}")
        else:
            print(f"[{i}/{len(TEST_MESSAGES)}] Failed to send: {msg}")

        # Wait between messages to see the display update
        if i < len(TEST_MESSAGES):
            time.sleep(3)

    print("\n=== Test complete ===")
    print("Your Xteink X4 should have cycled through all 8 sprite states.")

    client.loop_stop()
    client.disconnect()

if __name__ == "__main__":
    main()
