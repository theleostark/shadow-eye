#!/usr/bin/env python3
"""
Moltbot to MQTT Bridge

This script acts as a bridge between Moltbot (Clawdbot) and MQTT.
It monitors a file that Moltbot writes to and publishes changes to MQTT.

Usage:
    python moltbot_to_mqtt.py --topic "xteink/x4/display" --file ./moltbot_state.json

Moltbot Configuration:
- Add a skill that writes state to the monitored file
- File format: {"message": "...", "state": "thinking"}
"""

import json
import time
import os
import argparse
import paho.mqtt.client as mqtt
from pathlib import Path
from datetime import datetime

# Default configuration
DEFAULT_BROKER = "broker.hivemq.com"
DEFAULT_PORT = 1883
DEFAULT_TOPIC = "xteink/x4/display"
DEFAULT_FILE = "./moltbot_state.json"
POLL_INTERVAL = 10  # Check file every 10 seconds

# Sprite states
SPRITE_STATES = [
    "idle", "thinking", "talking", "excited",
    "sleeping", "error", "alert", "working"
]


class MoltbotMQTTBridge:
    """Bridge between Moltbot file and MQTT broker"""

    def __init__(self, broker, port, topic, file_path, client_id="moltbot-bridge"):
        self.broker = broker
        self.port = port
        self.topic = topic
        self.file_path = Path(file_path)
        self.client_id = client_id
        self.last_mtime = 0
        self.last_content = None
        self.mqtt_client = None

    def log(self, message):
        """Print log message with timestamp"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        print(f"[{timestamp}] {message}")

    def on_connect(self, client, userdata, flags, rc):
        """MQTT connect callback"""
        if rc == 0:
            self.log(f"Connected to {self.broker}:{self.port}")
            client.subscribe(self.topic)
            self.log(f"Subscribed to: {self.topic}")
        else:
            self.log(f"Connection failed with code {rc}")

    def on_publish(self, client, userdata, mid):
        """MQTT publish callback"""
        self.log(f"Message published (mid={mid})")

    def connect_mqtt(self):
        """Connect to MQTT broker"""
        self.mqtt_client = mqtt.Client(client_id=self.client_id)
        self.mqtt_client.on_connect = self.on_connect
        self.mqtt_client.on_publish = self.on_publish

        self.log(f"Connecting to {self.broker}:{self.port}...")
        try:
            self.mqtt_client.connect(self.broker, self.port, keepalive=60)
            self.mqtt_client.loop_start()
            return True
        except Exception as e:
            self.log(f"MQTT connection error: {e}")
            return False

    def read_state_file(self):
        """Read the state file"""
        if not self.file_path.exists():
            return None

        try:
            with open(self.file_path, 'r') as f:
                content = f.read().strip()
                if content:
                    return json.loads(content)
        except (json.JSONDecodeError, IOError) as e:
            self.log(f"Error reading file: {e}")

        return None

    def validate_state(self, state_data):
        """Validate and clean state data"""
        if not isinstance(state_data, dict):
            return None

        # Validate state field
        if 'state' in state_data:
            if state_data['state'] not in SPRITE_STATES:
                self.log(f"Invalid state: {state_data['state']}, using 'idle'")
                state_data['state'] = 'idle'

        # Add timestamp if not present
        if 'timestamp' not in state_data:
            state_data['timestamp'] = datetime.now().isoformat()

        return state_data

    def publish_state(self, state_data):
        """Publish state to MQTT"""
        if not self.mqtt_client or not self.mqtt_client.is_connected():
            self.log("Not connected to MQTT broker")
            return False

        payload = json.dumps(state_data)
        result = self.mqtt_client.publish(self.topic, payload)

        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            self.log(f"Published: {payload}")
            return True
        else:
            self.log(f"Publish failed: {result.rc}")
            return False

    def check_and_update(self):
        """Check file for changes and publish to MQTT"""
        if not self.file_path.exists():
            return

        # Check modification time
        current_mtime = self.file_path.stat().st_mtime
        if current_mtime <= self.last_mtime:
            return  # No change

        self.log(f"File modified: {self.file_path}")

        # Read and validate
        state_data = self.read_state_file()
        if state_data:
            validated_data = self.validate_state(state_data)
            if validated_data:
                # Check if content actually changed
                if validated_data != self.last_content:
                    self.publish_state(validated_data)
                    self.last_content = validated_data

        self.last_mtime = current_mtime

    def run(self):
        """Main loop"""
        self.log("=== Moltbot to MQTT Bridge ===")
        self.log(f"Monitoring: {self.file_path}")
        self.log(f"Publishing to: {self.broker}:{self.port}/{self.topic}")
        self.log(f"Poll interval: {POLL_INTERVAL}s")
        self.log("Press Ctrl+C to stop")

        if not self.connect_mqtt():
            self.log("Failed to connect to MQTT broker")
            return

        try:
            while True:
                self.check_and_update()
                time.sleep(POLL_INTERVAL)
        except KeyboardInterrupt:
            self.log("\nStopping...")
        finally:
            if self.mqtt_client:
                self.mqtt_client.loop_stop()
                self.mqtt_client.disconnect()
                self.log("Disconnected")


def create_sample_state(file_path):
    """Create a sample state file for testing"""
    sample = {
        "message": "Hello from Moltbot!",
        "state": "idle",
        "timestamp": datetime.now().isoformat()
    }

    with open(file_path, 'w') as f:
        json.dump(sample, f, indent=2)

    print(f"Created sample state file: {file_path}")
    print(f"Contents: {json.dumps(sample, indent=2)}")


def main():
    parser = argparse.ArgumentParser(
        description='Bridge Moltbot state file to MQTT for Xteink X4'
    )
    parser.add_argument(
        '--broker', '-b',
        default=DEFAULT_BROKER,
        help=f'MQTT broker (default: {DEFAULT_BROKER})'
    )
    parser.add_argument(
        '--port', '-p',
        type=int,
        default=DEFAULT_PORT,
        help=f'MQTT port (default: {DEFAULT_PORT})'
    )
    parser.add_argument(
        '--topic', '-t',
        default=DEFAULT_TOPIC,
        help=f'MQTT topic (default: {DEFAULT_TOPIC})'
    )
    parser.add_argument(
        '--file', '-f',
        default=DEFAULT_FILE,
        help=f'State file to monitor (default: {DEFAULT_FILE})'
    )
    parser.add_argument(
        '--init',
        action='store_true',
        help='Create sample state file and exit'
    )
    parser.add_argument(
        '--client-id',
        default='moltbot-bridge',
        help='MQTT client ID'
    )

    args = parser.parse_args()

    # Create sample file if requested
    if args.init:
        create_sample_state(args.file)
        return

    # Create and run bridge
    bridge = MoltbotMQTTBridge(
        broker=args.broker,
        port=args.port,
        topic=args.topic,
        file_path=args.file,
        client_id=args.client_id
    )

    bridge.run()


if __name__ == '__main__':
    main()
