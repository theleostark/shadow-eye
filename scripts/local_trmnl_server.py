#!/usr/bin/env python3
"""
TRMNL Local Server - Simple HTTP server for Xteink X4 device

Run this on any device on your local network to serve content to your TRMNL device.

Usage:
    python local_trmnl_server.py --port 8000 --images ./images

The Xteink X4 will fetch from: http://YOUR_IP:8000
"""

from http.server import HTTPServer, SimpleHTTPRequestHandler
import json
import socket
import argparse
import os
from urllib.parse import urlparse, parse_qs
import hashlib

# TRMNL API responses
TRMNL_SETUP_RESPONSE = {
    "status": 200,
    "api_key": "local-dev-key-12345",
    "friendly_id": "LOCAL",
    "image_url": "/api/current_screen",
    "filename": "current.bmp",
    "message": "Connected to local TRMNL server"
}

TRMNL_DISPLAY_RESPONSE_TEMPLATE = {
    "status": 0,
    "image_url": "/image.bmp",
    "filename": "image.bmp",
    "refresh_rate": 900,  # 15 minutes
    "reset_firmware": False,
    "update_firmware": False,
    "firmware_url": None,
    "special_function": None
}


class TrmnlRequestHandler(SimpleHTTPRequestHandler):
    """HTTP request handler that implements TRMNL API endpoints"""

    def log_message(self, format, *args):
        """Custom log format with client IP"""
        print(f"[{self.client_address[0]}] {format % args}")

    def do_GET(self):
        """Handle GET requests for TRMNL API endpoints"""

        # Parse URL and path
        parsed_path = urlparse(self.path)
        path = parsed_path.path

        # Log the request with headers
        print(f"\n=== GET {self.path} ===")
        print("Headers:")
        for header, value in self.headers.items():
            print(f"  {header}: {value}")

        # TRMNL API: /api/setup - Device registration
        if path == '/api/setup' or path.startswith('/api/setup/'):
            self.handle_setup()
            return

        # TRMNL API: /api/display - Get display content
        elif path == '/api/display':
            self.handle_display()
            return

        # TRMNL API: /api/current_screen - Get current image
        elif path == '/api/current_screen':
            self.handle_current_screen()
            return

        # TRMNL API: /api/models - Get device model info
        elif path == '/api/models':
            self.handle_models()
            return

        # Static file serving
        else:
            # Try to serve static file
            super().do_GET()

    def do_POST(self):
        """Handle POST requests"""
        print(f"\n=== POST {self.path} ===")
        print("Headers:")
        for header, value in self.headers.items():
            print(f"  {header}: {value}")

        content_length = int(self.headers.get('Content-Length', 0))
        if content_length > 0:
            body = self.rfile.read(content_length)
            print(f"Body: {body.decode('utf-8', errors='ignore')[:500]}")

        # TRMNL API: /api/log - Submit device logs
        if self.path == '/api/log':
            self.handle_log()
            return

        # 404 for unknown endpoints
        self.send_error(404, "Not Found")

    def handle_setup(self):
        """Handle /api/setup - Device registration"""
        mac_address = self.headers.get('ID', 'unknown')
        print(f"[SETUP] Device registration from MAC: {mac_address}")

        response = TRMNL_SETUP_RESPONSE.copy()
        response['friendly_id'] = f"LOCAL-{mac_address[-4:]}"

        self.send_json_response(200, response)

    def handle_display(self):
        """Handle /api/display - Get display content"""
        access_token = self.headers.get('access-token', self.headers.get('Access-Token', ''))
        refresh_rate = self.headers.get('Refresh-Rate', self.headers.get('refresh-rate', '900'))

        print(f"[DISPLAY] Fetch request (token: {access_token[:10]}...)")

        # Build response
        response = TRMNL_DISPLAY_RESPONSE_TEMPLATE.copy()
        response['refresh_rate'] = int(refresh_rate) if refresh_rate.isdigit() else 900

        # Check for image file in current directory
        if os.path.exists('image.bmp'):
            response['image_url'] = '/image.bmp'
            response['filename'] = 'image.bmp'
        elif os.path.exists('image.png'):
            response['image_url'] = '/image.png'
            response['filename'] = 'image.png'
        else:
            # Use a placeholder
            response['image_url'] = 'https://trmnl.app/images/setup/setup-logo.bmp'
            response['filename'] = 'setup-logo.bmp'

        self.send_json_response(200, response)

    def handle_current_screen(self):
        """Handle /api/current_screen - Get current image"""
        print(f"[CURRENT_SCREEN] Image request")

        response = TRMNL_DISPLAY_RESPONSE_TEMPLATE.copy()

        if os.path.exists('image.bmp'):
            response['image_url'] = '/image.bmp'
        elif os.path.exists('image.png'):
            response['image_url'] = '/image.png'
        else:
            response['image_url'] = 'https://trmnl.app/images/setup/setup-logo.bmp'

        self.send_json_response(200, response)

    def handle_models(self):
        """Handle /api/models - Device model information"""
        models = {
            "models": [
                {
                    "id": "xteink_x4",
                    "name": "Xteink X4",
                    "width": 800,
                    "height": 480,
                    "palette": ["1-bit", "2-bit", "4-gray"],
                    "description": "4.26\" E-Paper Display with ESP32-C3"
                }
            ]
        }
        self.send_json_response(200, models)

    def handle_log(self):
        """Handle /api/log - Device log submission"""
        print(f"[LOG] Device log received")

        self.send_json_response(200, {"status": "ok", "message": "Log received"})

    def send_json_response(self, code, data):
        """Send JSON response"""
        response_body = json.dumps(data, indent=2)
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', len(response_body))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(response_body.encode('utf-8'))
        print(f"[RESPONSE] {code}: {response_body[:200]}...")


def get_local_ip():
    """Get the local IP address of this machine"""
    try:
        # Create a socket to determine local IP
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("8.8.8.8", 80))
            local_ip = s.getsockname()[0]
        return local_ip
    except Exception:
        return "127.0.0.1"


def main():
    parser = argparse.ArgumentParser(description='TRMNL Local Server')
    parser.add_argument('--port', '-p', type=int, default=8000, help='Port to listen on')
    parser.add_argument('--bind', '-b', default='0.0.0.0', help='Bind address')
    args = parser.parse_args()

    local_ip = get_local_ip()

    print("=" * 60)
    print("       TRMNL LOCAL SERVER")
    print("=" * 60)
    print(f"\nServer starting on http://{local_ip}:{args.port}")
    print(f"\nConfigure your Xteink X4 device:")
    print(f"  API URL: http://{local_ip}:{args.port}")
    print(f"\nOr use mDNS (if enabled): http://trmnl-server.local:{args.port}")
    print(f"\nAPI Endpoints:")
    print(f"  GET  /api/setup          - Device registration")
    print(f"  GET  /api/display        - Get display content")
    print(f"  GET  /api/current_screen - Get current image")
    print(f"  GET  /api/models         - Device model info")
    print(f"  POST /api/log            - Submit device logs")
    print(f"\nPlace your image file (image.bmp or image.png) in:")
    print(f"  {os.getcwd()}")
    print(f"\nPress Ctrl+C to stop")
    print("=" * 60)

    # Create and start server
    server_address = (args.bind, args.port)
    httpd = HTTPServer(server_address, TrmnlRequestHandler)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n\nServer stopped.")
        httpd.server_close()


if __name__ == '__main__':
    main()
