#!/usr/bin/env python3
"""
Moltbot Sprite Generator

Generates 1-bit PNG sprite images for the 8 Moltbot states:
- idle, thinking, talking, excited, sleeping, error, alert, working

Generated sprites are optimized for e-ink displays (black/white only).
"""

from PIL import Image, ImageDraw
import os

# Sprite configuration
SPRITE_SIZE = (200, 200)
OUTPUT_DIR = "sprites"
STATE_NAMES = [
    "idle",
    "thinking",
    "talking",
    "excited",
    "sleeping",
    "error",
    "alert",
    "working"
]

def create_idle_sprite():
    """Create a calm, idle face sprite"""
    img = Image.new('1', SPRITE_SIZE, 1)  # White background
    draw = ImageDraw.Draw(img)
    cx, cy = 100, 100

    # Face outline (circle)
    draw.ellipse([cx-40, cy-40, cx+40, cy+40], outline=0, width=3)

    # Closed eyes (happy curves)
    draw.arc([cx-25, cy-20, cx-5, cy-5], 0, 180, fill=0, width=2)
    draw.arc([cx+5, cy-20, cx+25, cy-5], 0, 180, fill=0, width=2)

    # Small smile
    draw.arc([cx-15, cy+5, cx+15, cy+20], 0, 180, fill=0, width=2)

    return img

def create_thinking_sprite():
    """Create a thinking face with thought bubble"""
    img = Image.new('1', SPRITE_SIZE, 1)
    draw = ImageDraw.Draw(img)
    cx, cy = 80, 120

    # Face
    draw.ellipse([cx-40, cy-40, cx+40, cy+40], outline=0, width=3)

    # Open eyes looking up
    draw.ellipse([cx-20, cy-20, cx-10, cy-10], fill=0)
    draw.ellipse([cx+10, cy-20, cx+20, cy-10], fill=0)

    # Thoughtful mouth (slight frown)
    draw.line([cx-10, cy+15, cx+10, cy+12], fill=0, width=2)

    # Thought bubbles
    draw.ellipse([cx+50, cy-60, cx+60, cy-50], fill=0)
    draw.ellipse([cx+60, cy-75, cx+75, cy-60], fill=0)
    draw.ellipse([cx+75, cy-95, cx+100, cy-70], outline=0, width=2)

    # Question mark in bubble
    draw.text((cx+82, cy-88), "?", fill=0)

    return img

def create_talking_sprite():
    """Create a talking face with speech indicator"""
    img = Image.new('1', SPRITE_SIZE, 1)
    draw = ImageDraw.Draw(img)
    cx, cy = 80, 100

    # Face
    draw.ellipse([cx-40, cy-40, cx+40, cy+40], outline=0, width=3)

    # Alert eyes
    draw.ellipse([cx-20, cy-15, cx-10, cy-5], fill=0)
    draw.ellipse([cx+10, cy-15, cx+20, cy-5], fill=0)

    # Open mouth (ellipse)
    draw.ellipse([cx-10, cy+8, cx+10, cy+20], outline=0, width=2)

    # Speech waves
    for i in range(3):
        y_pos = cy - 5 + (i * 8)
        draw.line([cx+50, y_pos, cx+50+(i+1)*5, y_pos-3], fill=0, width=2)
        draw.line([cx+50, y_pos, cx+50+(i+1)*5, y_pos+3], fill=0, width=2)

    return img

def create_excited_sprite():
    """Create an excited/happy face"""
    img = Image.new('1', SPRITE_SIZE, 1)
    draw = ImageDraw.Draw(img)
    cx, cy = 100, 100

    # Face
    draw.ellipse([cx-45, cy-45, cx+45, cy+45], outline=0, width=3)

    # Star eyes (simplified as circles with shine)
    draw.ellipse([cx-22, cy-18, cx-8, cy-5], fill=0)
    draw.ellipse([cx+8, cy-18, cx+22, cy-5], fill=0)
    draw.ellipse([cx-18, cy-16, cx-12, cy-10], fill=1)
    draw.ellipse([cx+12, cy-16, cx+18, cy-10], fill=1)

    # Big smile
    draw.arc([cx-20, cy+5, cx+20, cy+25], 0, 180, fill=0, width=3)

    # Excitement marks (sparkles)
    for x, y in [(40, 40), (160, 40), (40, 160), (160, 160)]:
        draw.line([x-5, y, x+5, y], fill=0, width=2)
        draw.line([x, y-5, x, y+5], fill=0, width=2)

    return img

def create_sleeping_sprite():
    """Create a sleeping face with Z's"""
    img = Image.new('1', SPRITE_SIZE, 1)
    draw = ImageDraw.Draw(img)
    cx, cy = 80, 100

    # Face
    draw.ellipse([cx-40, cy-40, cx+40, cy+40], outline=0, width=3)

    # Closed eyes (lines)
    draw.line([cx-25, cy-12, cx-5, cy-12], fill=0, width=2)
    draw.line([cx+5, cy-12, cx+25, cy-12], fill=0, width=2)

    # Small peaceful mouth
    draw.arc([cx-10, cy+5, cx+10, cy+15], 0, 180, fill=0, width=2)

    # Z's getting smaller
    draw.text((cx+50, cy-40), "Z", fill=0, font=None)
    draw.text((cx+65, cy-55), "Z", fill=0, font=None)
    draw.text((cx+80, cy-70), "Z", fill=0, font=None)

    # Sleep bubble
    draw.ellipse([cx+45, cy-50, cx+55, cy-40], fill=0)
    draw.ellipse([cx+60, cy-65, cx+75, cy-50], fill=0)

    return img

def create_error_sprite():
    """Create an error face with X eyes"""
    img = Image.new('1', SPRITE_SIZE, 1)
    draw = ImageDraw.Draw(img)
    cx, cy = 100, 100

    # Face
    draw.ellipse([cx-40, cy-40, cx+40, cy+40], outline=0, width=3)

    # X eyes
    draw.line([cx-25, cy-15, cx-10, cy-5], fill=0, width=2)
    draw.line([cx-25, cy-5, cx-10, cy-15], fill=0, width=2)
    draw.line([cx+10, cy-15, cx+25, cy-5], fill=0, width=2)
    draw.line([cx+10, cy-5, cx+25, cy-15], fill=0, width=2)

    # Sad mouth
    draw.arc([cx-15, cy+15, cx+15, cy+25], 180, 0, fill=0, width=2)

    # Sweat drop
    draw.ellipse([cx+45, cy-25, cx+55, cy-10], outline=0, width=2)
    draw.line([cx+50, cy-25, cx+50, cy-30], fill=0, width=2)

    return img

def create_alert_sprite():
    """Create an alert face with wide eyes"""
    img = Image.new('1', SPRITE_SIZE, 1)
    draw = ImageDraw.Draw(img)
    cx, cy = 100, 100

    # Face
    draw.ellipse([cx-40, cy-40, cx+40, cy+40], outline=0, width=3)

    # Wide eyes (large circles with dots)
    draw.ellipse([cx-25, cy-20, cx-5, cy], outline=0, width=2)
    draw.ellipse([cx+5, cy-20, cx+25, cy], outline=0, width=2)
    draw.ellipse([cx-18, cy-13, cx-12, cy-7], fill=0)
    draw.ellipse([cx+12, cy-13, cx+18, cy-7], fill=0)

    # O mouth (surprised)
    draw.ellipse([cx-8, cy+10, cx+8, cy+25], outline=0, width=2)

    # Exclamation marks
    for x, y in [(30, 30), (170, 30)]:
        draw.line([x, y-10, x, y], fill=0, width=3)
        draw.ellipse([x-2, y+2, x+2, y+6], fill=0)

    return img

def create_working_sprite():
    """Create a working/determined face with gear"""
    img = Image.new('1', SPRITE_SIZE, 1)
    draw = ImageDraw.Draw(img)
    cx, cy = 80, 100

    # Face
    draw.ellipse([cx-40, cy-40, cx+40, cy+40], outline=0, width=3)

    # Determined eyes
    draw.ellipse([cx-20, cy-15, cx-10, cy-5], fill=0)
    draw.ellipse([cx+10, cy-15, cx+20, cy-5], fill=0)

    # Determined mouth (flat line)
    draw.line([cx-12, cy+12, cx+12, cy+12], fill=0, width=2)

    # Gear icon
    gx, gy = 150, 60
    gear_radius = 20
    draw.ellipse([gx-gear_radius, gy-gear_radius, gx+gear_radius, gy+gear_radius], outline=0, width=2)
    draw.ellipse([gx-8, gy-8, gx+8, gy+8], fill=0)

    # Gear teeth
    import math
    for i in range(8):
        angle = i * (360/8)
        rad = math.radians(angle)
        tx = gx + math.cos(rad) * gear_radius
        ty = gy + math.sin(rad) * gear_radius
        draw.ellipse([tx-4, ty-4, tx+4, ty+4], fill=0)

    return img

def generate_all_sprites():
    """Generate all sprite PNGs"""
    # Create output directory
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    sprite_generators = {
        "idle": create_idle_sprite,
        "thinking": create_thinking_sprite,
        "talking": create_talking_sprite,
        "excited": create_excited_sprite,
        "sleeping": create_sleeping_sprite,
        "error": create_error_sprite,
        "alert": create_alert_sprite,
        "working": create_working_sprite,
    }

    for state_name, generator in sprite_generators.items():
        sprite = generator()
        filename = os.path.join(OUTPUT_DIR, f"sprite_{state_name}.png")
        sprite.save(filename, "PNG")
        print(f"Generated: {filename}")

    print(f"\nAll sprites generated in '{OUTPUT_DIR}/' directory")

    # Also generate a test image with all sprites
    create_sprite_sheet()

def create_sprite_sheet():
    """Create a sprite sheet showing all states"""
    sheet_width = 600  # 3 columns x 200
    sheet_height = 600  # 3 rows x 200
    sheet = Image.new('1', (sheet_width, sheet_height), 1)

    sprite_files = [
        "sprite_idle.png",
        "sprite_thinking.png",
        "sprite_talking.png",
        "sprite_excited.png",
        "sprite_sleeping.png",
        "sprite_error.png",
        "sprite_alert.png",
        "sprite_working.png",
    ]

    positions = [
        (0, 0), (200, 0), (400, 0),
        (0, 200), (200, 200), (400, 200),
        (0, 400), (200, 400),
    ]

    for sprite_file, (x, y) in zip(sprite_files, positions):
        sprite_path = os.path.join(OUTPUT_DIR, sprite_file)
        if os.path.exists(sprite_path):
            sprite = Image.open(sprite_path)
            sheet.paste(sprite, (x, y))

            # Add state label
            draw = ImageDraw.Draw(sheet)
            state_name = sprite_file.replace("sprite_", "").replace(".png", "")
            draw.text((x + 5, y + 180), state_name, fill=0)

    sheet_path = os.path.join(OUTPUT_DIR, "sprite_sheet.png")
    sheet.save(sheet_path, "PNG")
    print(f"Generated sprite sheet: {sheet_path}")

if __name__ == "__main__":
    print("Moltbot Sprite Generator")
    print("=" * 40)
    generate_all_sprites()
