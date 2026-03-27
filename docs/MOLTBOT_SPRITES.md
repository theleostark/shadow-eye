# Moltbot MQTT Sprite System

Complete desktop pet sprite display system for Xteink X4 e-ink display.

## Overview

The Moltbot sprite system allows an AI agent (like Moltbot) to display animated "desktop pet" sprites on the Xteink X4 e-ink display via MQTT. This brings the device to life with visual feedback of the bot's current state.

## Architecture

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐      ┌──────────────┐
│   Moltbot   │ ───> │ moltbot_     │ ───> │   MQTT      │ ───> │  Xteink X4   │
│  (AI Agent) │      │ to_mqtt.py   │      │  Broker     │      │  E-Ink       │
└─────────────┘      └──────────────┘      └─────────────┘      └──────────────┘
     JSON File          Python Script        HiveMQ Public        ESP32-C3
```

## Sprite States

| State | Description | Visual |
|-------|-------------|--------|
| `idle` | Bot is resting | Calm face with closed eyes |
| `thinking` | Processing request | Face with thought bubbles |
| `talking` | Speaking/Responding | Face with speech waves |
| `excited` | Happy/Enthusiastic | Star eyes, big smile |
| `sleeping` | Idle/Sleeping | Face with Z's |
| `error` | Something went wrong | X eyes, sad expression |
| `alert` | Needs attention | Wide eyes, surprised |
| `working` | Busy processing | Face with gear icon |

## Quick Start

### 1. Enable MQTT Sprites on Device

The firmware includes MQTT sprite support but it's disabled by default. Enable it via:

**Via Web Interface:**
1. Connect to device WiFi
2. Open `http://192.168.1.1` (or device IP)
3. Settings → Enable MQTT Sprites

**Via Preferences API:**
```cpp
// Set via your configuration method
preferences.putBool("mqtt_sprite_enabled", true);
```

### 2. Test MQTT Sprites

Run the test script to verify the connection:
```bash
python3 scripts/test_mqtt_sprites.py
```

This cycles through all 8 sprite states with 3-second intervals.

### 3. Integrate with Moltbot

Use the bridge script to connect Moltbot to MQTT:
```bash
python3 scripts/moltbot_to_mqtt.py
```

Or publish directly:
```python
import json
import paho.mqtt.client as mqtt

client = mqtt.Client()
client.connect("broker.hivemq.com", 1883)

# Send a sprite update
message = {"state": "thinking", "message": "Processing..."}
client.publish("xteink/x4/display", json.dumps(message))
```

## MQTT Message Format

```json
{
  "state": "thinking",     // Required: sprite state
  "message": "Hello!"      // Optional: text message
}
```

### Valid States
- `idle`
- `thinking`
- `talking`
- `excited`
- `sleeping`
- `error`
- `alert`
- `working`

## Configuration

### MQTT Broker Settings

| Setting | Default | Description |
|---------|---------|-------------|
| Broker | `broker.hivemq.com` | Public MQTT broker |
| Port | `1883` | MQTT port |
| Topic | `xteink/x4/display` | Subscription topic |
| Client ID | `xteink-x4` | MQTT client identifier |

### Custom Broker

To use a different broker (recommended for production):

```python
# In your bridge script
BROKER = "your.local.broker"
PORT = 1883
TOPIC = "xteink/x4/display"
USERNAME = "mqtt_user"  # Optional
PASSWORD = "mqtt_pass"  # Optional
```

## Files

| File | Description |
|------|-------------|
| `include/mqtt_sprite.h` | MQTT client API |
| `src/mqtt_sprite.cpp` | MQTT implementation |
| `include/sprite_renderer.h` | Sprite rendering API |
| `src/sprite_renderer.cpp` | Sprite rendering implementation |
| `src/sprite_data.h` | Embedded PNG sprite data |
| `scripts/moltbot_to_mqtt.py` | Moltbot bridge |
| `scripts/test_mqtt_sprites.py` | Test script |
| `scripts/generate_sprites.py` | Sprite generator |
| `scripts/png_to_header.py` | PNG to C converter |

## Creating Custom Sprites

### Option 1: Edit `generate_sprites.py`

Modify the sprite drawing functions in `scripts/generate_sprites.py`:
```python
def create_custom_sprite():
    img = Image.new('1', (200, 200), 1)
    draw = ImageDraw.Draw(img)
    # Your drawing code here
    return img
```

Then regenerate:
```bash
python3 scripts/generate_sprites.py
python3 scripts/png_to_header.py
pio run --target upload
```

### Option 2: Provide Your Own PNGs

1. Create 200x200 pixel 1-bit (black/white) PNGs
2. Save as `sprites/sprite_<state>.png`
3. Run `python3 scripts/png_to_header.py`
4. Rebuild and flash firmware

## Performance

- **Flash usage:** ~5KB for all 8 sprites
- **RAM usage:** ~1KB during rendering
- **Update time:** ~2-3 seconds per sprite
- **Battery impact:** Minimal (e-ink only uses power when updating)

## Troubleshooting

### Sprites not displaying

1. Check MQTT is enabled on device
2. Verify WiFi connection
3. Test with `test_mqtt_sprites.py`
4. Check serial logs for errors

### MQTT connection failing

- Ensure broker is reachable
- Check firewall settings
- Try a different broker (e.g., local mosquitto)

### Sprites displaying incorrectly

- Verify PNG files are 1-bit (black/white)
- Check sprite size is 200x200 pixels
- Regenerate sprites and reflash

## Resources

- [HiveMQ Public Broker](https://www.hivemq.com/public-mqtt-broker/)
- [PubSubClient Documentation](https://pubsubclient.knolleary.net/)
- [PNGdec Library](https://github.com/bitbank2/PNGdec)

## License

Same as parent project (MIT/Apache 2.0 dual license)
