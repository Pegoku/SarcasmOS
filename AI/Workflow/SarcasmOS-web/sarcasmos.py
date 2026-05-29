#!/usr/bin/env python
"""Start, stop, restart, and inspect SarcasmOS local services."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path
from urllib.error import URLError
from urllib.request import urlopen


WEB_DIR = Path(__file__).resolve().parent
ROOT_DIR = WEB_DIR.parents[2]
VENV_DIR = ROOT_DIR / ".venv"
RUN_DIR = WEB_DIR / ".sarcasmos-run"
LOG_DIR = RUN_DIR / "logs"

PORTS = {
    "backend": 8001,
    "web": 5173,
    "proxy": 9000,
}
MANAGED_SERVICES = ["backend", "web", "proxy", "ngrok"]
WATCHDOG_INTERVAL_SECONDS = 8


def python_bin() -> Path:
    if os.name == "nt":
        return VENV_DIR / "Scripts" / "python.exe"
    return VENV_DIR / "bin" / "python"


def service_command(name: str) -> list[str]:
    py = str(python_bin())
    if name == "backend":
        return [py, "-m", "uvicorn", "backend.app:app", "--host", "0.0.0.0", "--port", str(PORTS["backend"])]
    if name == "web":
        return [py, "-m", "http.server", str(PORTS["web"]), "-d", str(WEB_DIR)]
    if name == "proxy":
        return [py, str(WEB_DIR / "proxy.py"), "--port", str(PORTS["proxy"])]
    if name == "ngrok":
        ngrok = shutil.which("ngrok")
        if not ngrok:
            raise RuntimeError("ngrok was not found in PATH. Install ngrok or start without --ngrok.")
        return [ngrok, "http", str(PORTS["proxy"])]
    if name == "watchdog":
        return [
            py,
            str(WEB_DIR / "sarcasmos.py"),
            "--watchdog",
            "--all",
            "--ngrok",
        ]
    raise ValueError(f"Unknown service: {name}")


def pid_file(name: str) -> Path:
    return RUN_DIR / f"{name}.json"


def ensure_environment() -> None:
    RUN_DIR.mkdir(exist_ok=True)
    LOG_DIR.mkdir(exist_ok=True)
    py = python_bin()
    if not py.exists():
        print(f"Creating virtual environment at {VENV_DIR}")
        subprocess.check_call([sys.executable, "-m", "venv", str(VENV_DIR)])

    reqs = WEB_DIR / "backend" / "requirements.txt"
    subprocess.check_call([str(py), "-m", "pip", "install", "-r", str(reqs)])
    try:
        subprocess.check_call([str(py), "-m", "pip", "check"], stdout=subprocess.DEVNULL)
    except subprocess.CalledProcessError:
        subprocess.check_call([str(py), "-m", "pip", "install", "--ignore-installed", "-r", str(reqs)])

    env_file = WEB_DIR / "backend" / ".env"
    env_example = WEB_DIR / "backend" / ".env.example"
    if not env_file.exists() and env_example.exists():
        shutil.copyfile(env_example, env_file)
        print("Created backend/.env from backend/.env.example. Add your API keys before using chat/TTS.")


def is_running(pid: int) -> bool:
    try:
        if os.name == "nt":
            result = subprocess.run(
                ["tasklist", "/FI", f"PID eq {pid}"],
                capture_output=True,
                text=True,
                check=False,
            )
            return str(pid) in result.stdout
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def is_port_open(port: int, host: str = "127.0.0.1") -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(0.25)
        return sock.connect_ex((host, port)) == 0


def read_pid(name: str) -> dict[str, object] | None:
    path = pid_file(name)
    if not path.exists():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        path.unlink(missing_ok=True)
        return None
    pid = int(data.get("pid", 0))
    if pid and is_running(pid):
        return data
    path.unlink(missing_ok=True)
    return None


def start_service(name: str) -> None:
    if read_pid(name):
        print(f"{name} already running.")
        return
    if name in PORTS and is_port_open(PORTS[name]):
        print(f"{name} port {PORTS[name]} is already in use by an external process; leaving it alone.")
        return
    command = service_command(name)
    log_path = LOG_DIR / f"{name}.log"
    log = log_path.open("a", encoding="utf-8")
    creationflags = 0
    start_new_session = False
    if os.name == "nt":
        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP
    else:
        start_new_session = True
    proc = subprocess.Popen(
        command,
        cwd=str(WEB_DIR),
        stdout=log,
        stderr=subprocess.STDOUT,
        stdin=subprocess.DEVNULL,
        creationflags=creationflags,
        start_new_session=start_new_session,
    )
    pid_file(name).write_text(
        json.dumps({"pid": proc.pid, "command": command, "log": str(log_path)}, indent=2),
        encoding="utf-8",
    )
    suffix = f" on http://localhost:{PORTS[name]}" if name in PORTS else ""
    print(f"Started {name}{suffix} (pid {proc.pid}). Log: {log_path}")


def stop_service(name: str) -> None:
    data = read_pid(name)
    if not data:
        print(f"{name} is not running.")
        return
    pid = int(data["pid"])
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], check=False, stdout=subprocess.DEVNULL)
    else:
        try:
            os.killpg(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    for _ in range(20):
        if not is_running(pid):
            break
        time.sleep(0.1)
    pid_file(name).unlink(missing_ok=True)
    print(f"Stopped {name} (pid {pid}).")


def restart_service(name: str) -> None:
    stop_service(name)
    start_service(name)


def show_status(names: list[str]) -> None:
    for name in names:
        data = read_pid(name)
        if data:
            extra = ""
            if name == "ngrok":
                public_url = ngrok_public_url()
                if public_url:
                    extra = f" -> {public_url}"
            print(f"{name}: running pid {data['pid']} ({data.get('log')}){extra}")
        else:
            print(f"{name}: stopped")
            if name == "ngrok":
                public_url = ngrok_public_url()
                if public_url:
                    print(f"ngrok tunnel active outside manager -> {public_url}")


def ngrok_public_url() -> str | None:
    try:
        with urlopen("http://127.0.0.1:4040/api/tunnels", timeout=0.75) as response:
            data = json.loads(response.read().decode("utf-8"))
    except (OSError, URLError, json.JSONDecodeError):
        return None
    tunnels = data.get("tunnels", [])
    https_urls = [
        tunnel.get("public_url")
        for tunnel in tunnels
        if str(tunnel.get("public_url", "")).startswith("https://")
    ]
    if https_urls:
        return str(https_urls[0])
    for tunnel in tunnels:
        public_url = tunnel.get("public_url")
        if public_url:
            return str(public_url)
    return None


def selected_services(args: argparse.Namespace) -> list[str]:
    base = ["backend", "web", "proxy"]
    if args.all:
        services = base[:]
    else:
        services = [name for name in base if getattr(args, name)]
    if args.ngrok and "ngrok" not in services:
        services.append("ngrok")
    if args.keepalive and "watchdog" not in services:
        services.append("watchdog")
    if not services:
        services = base[:]
    return services


def watchdog_loop(args: argparse.Namespace) -> int:
    services = [service for service in selected_services(args) if service != "watchdog"]
    ensure_environment()
    print(f"Watchdog active for: {', '.join(services)}")
    while True:
        for service in services:
            try:
                if not read_pid(service):
                    print(f"{service} is down; restarting.")
                    start_service(service)
            except Exception as exc:
                print(f"Watchdog could not ensure {service}: {exc}", file=sys.stderr)
        time.sleep(WATCHDOG_INTERVAL_SECONDS)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Manage SarcasmOS web services.")
    action = parser.add_mutually_exclusive_group()
    action.add_argument("-start", "--start", action="store_true", help="Start selected services.")
    action.add_argument("-stop", "--stop", action="store_true", help="Stop selected services.")
    action.add_argument("-restart", "--restart", action="store_true", help="Restart selected services.")
    action.add_argument("-status", "--status", action="store_true", help="Show selected service status.")
    parser.add_argument("-backend", "--backend", action="store_true")
    parser.add_argument("-web", "--web", action="store_true")
    parser.add_argument("-proxy", "--proxy", action="store_true")
    parser.add_argument("-ngrok", "--ngrok", action="store_true", help="Include ngrok http 9000.")
    parser.add_argument("-keepalive", "--keepalive", action="store_true", help="Keep services alive with a background watchdog.")
    parser.add_argument("--watchdog", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--all", action="store_true", help="Select backend, web, and proxy.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.watchdog:
        return watchdog_loop(args)

    services = selected_services(args)
    action = "start"
    if args.stop:
        action = "stop"
    elif args.restart:
        action = "restart"
    elif args.status:
        action = "status"

    if action == "stop" and "watchdog" in services:
        services = ["watchdog"] + [service for service in services if service != "watchdog"]
    if action == "restart" and "watchdog" in services:
        services = ["watchdog"] + [service for service in services if service != "watchdog"]

    if action in {"start", "restart"}:
        ensure_environment()

    try:
        for service in services:
            if action == "start":
                start_service(service)
            elif action == "stop":
                stop_service(service)
            elif action == "restart":
                restart_service(service)
            elif action == "status":
                show_status([service])
    except RuntimeError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    if action == "start" and "proxy" in services:
        print(f"Open the app through the proxy: http://localhost:{PORTS['proxy']}")
        print("To share it: python sarcasmos.py --start --ngrok")
        if "watchdog" in services:
            print("Keepalive is active. Stop it with: python sarcasmos.py --stop --keepalive")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
