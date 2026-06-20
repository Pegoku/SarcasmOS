#!/usr/bin/env python3
"""Tiny reverse proxy for SarcasmOS.

Routes API requests to the FastAPI backend and everything else to the static
web server, so one public port can expose the whole app.
"""

from __future__ import annotations

import argparse
import http.client
import select
import socket
import socketserver
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


HOP_BY_HOP_HEADERS = {
    "connection",
    "content-length",
    "date",
    "keep-alive",
    "proxy-authenticate",
    "proxy-authorization",
    "te",
    "trailers",
    "transfer-encoding",
    "upgrade",
    "server",
}


def should_route_to_backend(path: str) -> bool:
    return (
        path.startswith("/api/")
        or path == "/api"
        or path == "/openapi.json"
        or path.startswith("/docs")
        or path.startswith("/redoc")
    )


class SarcasmOSProxyHandler(BaseHTTPRequestHandler):
    backend_host: str = "127.0.0.1"
    backend_port: int = 8001
    web_host: str = "127.0.0.1"
    web_port: int = 5173

    protocol_version = "HTTP/1.1"

    def handle_one_request(self) -> None:
        try:
            super().handle_one_request()
        except ConnectionResetError:
            self.close_connection = True

    def do_GET(self) -> None:
        self.proxy_request()

    def do_HEAD(self) -> None:
        self.proxy_request()

    def do_POST(self) -> None:
        self.proxy_request()

    def do_PUT(self) -> None:
        self.proxy_request()

    def do_PATCH(self) -> None:
        self.proxy_request()

    def do_DELETE(self) -> None:
        self.proxy_request()

    def do_OPTIONS(self) -> None:
        self.proxy_request()

    def proxy_request(self) -> None:
        if self.is_websocket_request():
            self.proxy_websocket()
            return

        target_host, target_port = self.target_for_path(self.path)
        body = self.read_body()
        headers = self.forward_headers(target_host, target_port)

        conn = http.client.HTTPConnection(target_host, target_port, timeout=60)
        try:
            conn.request(self.command, self.path, body=body, headers=headers)
            response = conn.getresponse()
            response_body = response.read()
        except Exception as exc:
            self.send_error(502, f"Proxy target unavailable: {exc}")
            return
        finally:
            conn.close()

        self.send_response(response.status, response.reason)
        for key, value in response.getheaders():
            if key.lower() not in HOP_BY_HOP_HEADERS:
                self.send_header(key, value)
        self.send_header("Content-Length", str(len(response_body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(response_body)

    def is_websocket_request(self) -> bool:
        return self.headers.get("Upgrade", "").lower() == "websocket"

    def proxy_websocket(self) -> None:
        target_host, target_port = self.target_for_path(self.path)
        try:
            upstream = socket.create_connection((target_host, target_port), timeout=20)
        except OSError as exc:
            self.send_error(502, f"WebSocket target unavailable: {exc}")
            return

        try:
            request_lines = [f"{self.command} {self.path} {self.request_version}\r\n"]
            for key, value in self.headers.items():
                if key.lower() == "host":
                    continue
                request_lines.append(f"{key}: {value}\r\n")
            request_lines.append(f"Host: {target_host}:{target_port}\r\n")
            request_lines.append(f"X-Forwarded-Host: {self.headers.get('Host', '')}\r\n")
            request_lines.append("X-Forwarded-Proto: http\r\n")
            request_lines.append("\r\n")
            upstream.sendall("".join(request_lines).encode("iso-8859-1"))
            self.tunnel_sockets(self.connection, upstream)
        finally:
            upstream.close()

    def tunnel_sockets(self, client: socket.socket, upstream: socket.socket) -> None:
        sockets = [client, upstream]
        for item in sockets:
            item.setblocking(False)
        while True:
            readable, _, errored = select.select(sockets, [], sockets, 60)
            if errored:
                return
            if not readable:
                return
            for source in readable:
                try:
                    data = source.recv(65536)
                except OSError:
                    return
                if not data:
                    return
                target = upstream if source is client else client
                try:
                    target.sendall(data)
                except OSError:
                    return

    def target_for_path(self, path: str) -> tuple[str, int]:
        parsed = urlsplit(path)
        if should_route_to_backend(parsed.path):
            return self.backend_host, self.backend_port
        return self.web_host, self.web_port

    def read_body(self) -> bytes | None:
        content_length = self.headers.get("Content-Length")
        if not content_length:
            return None
        return self.rfile.read(int(content_length))

    def forward_headers(self, target_host: str, target_port: int) -> dict[str, str]:
        headers = {}
        for key, value in self.headers.items():
            if key.lower() not in HOP_BY_HOP_HEADERS and key.lower() != "host":
                headers[key] = value
        headers["Host"] = f"{target_host}:{target_port}"
        headers["X-Forwarded-Host"] = self.headers.get("Host", "")
        headers["X-Forwarded-Proto"] = "http"
        return headers

    def log_message(self, fmt: str, *args: object) -> None:
        print(f"{self.address_string()} - {fmt % args}")


class ReusableThreadingHTTPServer(ThreadingHTTPServer):
    allow_reuse_address = True
    daemon_threads = True


def main() -> None:
    parser = argparse.ArgumentParser(description="SarcasmOS reverse proxy")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=9000)
    parser.add_argument("--backend-host", default="127.0.0.1")
    parser.add_argument("--backend-port", type=int, default=8001)
    parser.add_argument("--web-host", default="127.0.0.1")
    parser.add_argument("--web-port", type=int, default=5173)
    args = parser.parse_args()

    SarcasmOSProxyHandler.backend_host = args.backend_host
    SarcasmOSProxyHandler.backend_port = args.backend_port
    SarcasmOSProxyHandler.web_host = args.web_host
    SarcasmOSProxyHandler.web_port = args.web_port

    with ReusableThreadingHTTPServer((args.host, args.port), SarcasmOSProxyHandler) as server:
        print(f"SarcasmOS proxy listening on http://{args.host}:{args.port}")
        print(f"  /api -> http://{args.backend_host}:{args.backend_port}")
        print(f"  /*   -> http://{args.web_host}:{args.web_port}")
        server.serve_forever()


if __name__ == "__main__":
    main()
