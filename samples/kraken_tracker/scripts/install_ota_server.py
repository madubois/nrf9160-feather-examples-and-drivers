#!/usr/bin/env python3
"""Install the tracker OTA manifest endpoint on the webserver host."""

from pathlib import Path
import re


SERVER_PATH = Path("/root/containers/webserver/server.py")
COMPOSE_PATH = Path("/root/containers/webserver/docker-compose.yml")
STATE_PATH = Path("/root/containers/webserver/ota_state.json")


def main() -> None:
    server = SERVER_PATH.read_text()
    if "import hashlib\n" not in server:
        server = server.replace("import json\n", "import json\nimport hashlib\n", 1)
    if "from pathlib import Path\n" not in server:
        server = server.replace("import logging\n", "import logging\nfrom pathlib import Path\n", 1)
    handler = "class Handler(BaseHTTPRequestHandler):\n"
    if "class Handler(BaseHTTPRequestHandler):\n    protocol_version = \"HTTP/1.1\"\n" not in server:
        if handler not in server:
            raise RuntimeError("Could not find Handler class in server.py")
        server = server.replace(
            handler,
            'class Handler(BaseHTTPRequestHandler):\n    protocol_version = "HTTP/1.1"\n',
            1,
        )

    content_length = '        self.send_header("Content-Length", str(len(payload)))\n'
    connection_close = content_length + '        self.send_header("Connection", "close")\n'
    if connection_close not in server:
        if content_length not in server:
            raise RuntimeError("Could not find response Content-Length header in server.py")
        server = server.replace(content_length, connection_close, 1)

    route = '''        if path == "/ota/manifest":
            try:
                state = json.loads(open("/ota_state.json", encoding="utf-8").read())
            except (FileNotFoundError, json.JSONDecodeError):
                state = {"available": False}

            try:
                firmware = open("/app_update.bin", "rb").read()
            except FileNotFoundError:
                self._send_json({"available": False})
                return

            manifest = {
                "available": bool(state.get("available", False)),
                "version": str(state.get("version", "")),
                "size": len(firmware),
                "sha256": hashlib.sha256(firmware).hexdigest(),
                "path": "/firmware",
            }
            self._send_json(manifest)
            return

'''
    marker = '        if path == "/firmware":\n'
    manifest_pattern = r'        if path == "/ota/manifest":\n.*?(?=        if path == "/firmware":)'
    if re.search(manifest_pattern, server, flags=re.DOTALL):
        server = re.sub(manifest_pattern, route, server, count=1, flags=re.DOTALL)
    else:
        if marker not in server:
            raise RuntimeError("Could not find firmware route in server.py")
        server = server.replace(marker, route + marker, 1)

    old_clear = '''            self._send_json(manifest)
            if manifest["available"]:
                with open("/ota_state.json", "w", encoding="utf-8") as state_file:
                    json.dump({"available": False}, state_file)
            return
'''
    new_clear = '''            self._send_json(manifest)
            return
'''
    server = server.replace(old_clear, new_clear, 1)

    helper = '''    def _send_firmware(self) -> None:
        try:
            size = Path("/app_update.bin").stat().st_size
        except FileNotFoundError:
            self._send_text("Not found", status=404)
            return

        start = 0
        end = size - 1
        range_header = self.headers.get("Range", "")
        if range_header.startswith("bytes="):
            try:
                start_text, end_text = range_header[6:].split("-", 1)
                start = int(start_text) if start_text else 0
                end = int(end_text) if end_text else end
                end = min(end, size - 1)
                if start < 0 or start > end:
                    raise ValueError
            except ValueError:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{size}")
                self.end_headers()
                return

        length = end - start + 1
        if range_header.startswith("bytes=") and start == 0:
            try:
                state = json.loads(open("/ota_state.json", encoding="utf-8").read())
            except (FileNotFoundError, json.JSONDecodeError):
                state = {}
            state["available"] = False
            with open("/ota_state.json", "w", encoding="utf-8") as state_file:
                json.dump(state, state_file)
        self.send_response(206 if range_header.startswith("bytes=") else 200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(length))
        self.send_header("Accept-Ranges", "bytes")
        if range_header.startswith("bytes="):
            self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.end_headers()
        with open("/app_update.bin", "rb") as firmware:
            firmware.seek(start)
            self.wfile.write(firmware.read(length))

'''
    helper_marker = "    def _read_json(self):\n"
    helper_pattern = r"    def _send_firmware\(self\) -> None:\n.*?(?=    def _read_json\(self\):)"
    if re.search(helper_pattern, server, flags=re.DOTALL):
        server = re.sub(helper_pattern, helper, server, count=1, flags=re.DOTALL)
    else:
        server = server.replace(helper_marker, helper + helper_marker, 1)

    server = server.replace(
        '        if path == "/firmware":\n            self._send_file("app_update.bin", "application/octet-stream")\n',
        '        if path == "/firmware":\n            self._send_firmware()\n',
        1,
    )
    SERVER_PATH.write_text(server)

    compose = COMPOSE_PATH.read_text()
    mount = "      - /root/containers/webserver/ota_state.json:/ota_state.json:rw\n"
    firmware_mount = "      - /root/containers/webserver/app_update.bin:/app_update.bin:ro\n"
    if mount not in compose:
        if firmware_mount not in compose:
            raise RuntimeError("Could not find firmware mount in docker-compose.yml")
        COMPOSE_PATH.write_text(compose.replace(firmware_mount, firmware_mount + mount, 1))

    if not STATE_PATH.exists():
        STATE_PATH.write_text('{"available": false}\n')


if __name__ == "__main__":
    main()