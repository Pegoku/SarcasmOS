#!/usr/bin/env python3
"""Collect SarcasmOS microphone samples and train a local microWakeWord model.

Collection uses only Python's standard library. The Setup action creates a
separate training environment because TensorFlow and microWakeWord are large.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import math
import os
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import urllib.parse
import wave
from array import array
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SAMPLE_RATE = 16_000
SAMPLE_WIDTH = 2
CHANNELS = 1
UPSTREAM_URL = "https://github.com/OHF-Voice/micro-wake-word.git"
MODEL_RELATIVE_PATH = Path(
    "trained_models/wakeword/tflite_stream_state_internal_quant/"
    "stream_state_internal_quant.tflite"
)

CATEGORIES: dict[str, dict[str, Any]] = {
    "positive": {
        "title": "Positive wake phrase",
        "instruction": "Say the exact wake phrase naturally, varying distance and tone.",
        "count": 100,
        "seconds": 3.2,
    },
    "negative": {
        "title": "Negative speech",
        "instruction": "Speak normally and include confusing phrases, but never the wake phrase.",
        "count": 100,
        "seconds": 3.2,
    },
    "noise": {
        "title": "Room/background noise",
        "instruction": "Do not speak. Capture TV, music, fans, movement and normal room sounds.",
        "count": 20,
        "seconds": 10.0,
    },
    "blank": {
        "title": "Quiet/blank room",
        "instruction": "Stay silent and still so the model learns the microphone noise floor.",
        "count": 20,
        "seconds": 3.2,
    },
}


def clear() -> None:
    if sys.stdout.isatty():
        print("\033[2J\033[H", end="")


def atomic_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n")
    temporary.replace(path)


def wav_metrics(pcm: bytes) -> tuple[int, int, float]:
    samples = array("h")
    samples.frombytes(pcm)
    if sys.byteorder != "little":
        samples.byteswap()
    if not samples:
        return 0, 0, 0.0
    peak = max(abs(sample) for sample in samples)
    rms = int(math.sqrt(sum(sample * sample for sample in samples) / len(samples)))
    clipped = 100.0 * sum(abs(sample) >= 32760 for sample in samples) / len(samples)
    return peak, rms, clipped


def save_wav(path: Path, pcm: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    with wave.open(str(temporary), "wb") as output:
        output.setnchannels(CHANNELS)
        output.setsampwidth(SAMPLE_WIDTH)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(pcm)
    temporary.replace(path)


class RawWebSocket:
    """Minimal RFC 6455 client for the Brain's unfragmented ws:// stream."""

    def __init__(self, url: str, timeout: float = 10.0):
        parsed = urllib.parse.urlsplit(url)
        if parsed.scheme != "ws" or not parsed.hostname:
            raise ValueError("the microphone URL must use ws://")
        self.host = parsed.hostname
        self.port = parsed.port or 80
        self.path = urllib.parse.urlunsplit(("", "", parsed.path or "/", parsed.query, ""))
        self.socket = socket.create_connection((self.host, self.port), timeout=timeout)
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        host_header = self.host if self.port == 80 else f"{self.host}:{self.port}"
        request = (
            f"GET {self.path} HTTP/1.1\r\nHost: {host_header}\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        )
        self.socket.sendall(request.encode("ascii"))
        response = bytearray()
        while b"\r\n\r\n" not in response:
            chunk = self.socket.recv(4096)
            if not chunk:
                raise ConnectionError("WebSocket closed during HTTP upgrade")
            response.extend(chunk)
            if len(response) > 16_384:
                raise ConnectionError("oversized WebSocket upgrade response")
        header, self.buffer = bytes(response).split(b"\r\n\r\n", 1)
        lines = header.decode("iso-8859-1").split("\r\n")
        if " 101 " not in f" {lines[0]} ":
            raise ConnectionError(f"WebSocket upgrade rejected: {lines[0]}")
        headers = {
            name.strip().lower(): value.strip()
            for line in lines[1:] if ":" in line
            for name, value in [line.split(":", 1)]
        }
        expected = base64.b64encode(
            hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()
        ).decode("ascii")
        if headers.get("sec-websocket-accept") != expected:
            raise ConnectionError("invalid Sec-WebSocket-Accept response")
        self.socket.settimeout(None)

    def _read_exact(self, length: int) -> bytes:
        output = bytearray()
        if self.buffer:
            take = min(length, len(self.buffer))
            output.extend(self.buffer[:take])
            self.buffer = self.buffer[take:]
        while len(output) < length:
            chunk = self.socket.recv(length - len(output))
            if not chunk:
                raise ConnectionError("WebSocket closed")
            output.extend(chunk)
        return bytes(output)

    def _send_control(self, opcode: int, payload: bytes = b"") -> None:
        mask = os.urandom(4)
        header = bytes((0x80 | opcode, 0x80 | len(payload)))
        masked = bytes(value ^ mask[index % 4] for index, value in enumerate(payload))
        self.socket.sendall(header + mask + masked)

    def recv(self) -> str | bytes | None:
        while True:
            first, second = self._read_exact(2)
            final = bool(first & 0x80)
            opcode = first & 0x0f
            masked = bool(second & 0x80)
            length = second & 0x7f
            if length == 126:
                length = struct.unpack("!H", self._read_exact(2))[0]
            elif length == 127:
                length = struct.unpack("!Q", self._read_exact(8))[0]
            mask = self._read_exact(4) if masked else b""
            payload = self._read_exact(length)
            if masked:
                payload = bytes(
                    value ^ mask[index % 4] for index, value in enumerate(payload)
                )
            if not final:
                raise ConnectionError("fragmented WebSocket frames are unsupported")
            if opcode == 0x8:
                return None
            if opcode == 0x9:
                self._send_control(0xA, payload)
                continue
            if opcode == 0xA:
                continue
            if opcode == 0x1:
                return payload.decode("utf-8")
            if opcode == 0x2:
                return payload
            raise ConnectionError(f"unsupported WebSocket opcode {opcode}")

    def close(self) -> None:
        try:
            self._send_control(0x8)
        except OSError:
            pass
        try:
            self.socket.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.socket.close()


class MicrophoneStream:
    def __init__(self, url: str):
        self.url = url
        self.socket: RawWebSocket | None = None
        self.metadata: dict[str, Any] = {}
        self.error: Exception | None = None
        self.closed = threading.Event()
        self.complete = threading.Event()
        self.lock = threading.Lock()
        self.capture: bytearray | None = None
        self.target_bytes = 0
        self.thread: threading.Thread | None = None

    def connect(self) -> None:
        self.socket = RawWebSocket(self.url, timeout=10)
        first = self.socket.recv()
        if not isinstance(first, str):
            self.socket.close()
            raise RuntimeError("the first WebSocket frame was not JSON metadata")
        self.metadata = json.loads(first)
        expected = ("pcm_s16le", SAMPLE_RATE, CHANNELS)
        actual = (
            self.metadata.get("format"),
            self.metadata.get("sample_rate"),
            self.metadata.get("channels"),
        )
        if actual != expected:
            self.socket.close()
            raise RuntimeError(f"unsupported stream format {actual}; expected {expected}")
        self.thread = threading.Thread(target=self._receive, daemon=True)
        self.thread.start()

    def _receive(self) -> None:
        try:
            while True:
                frame = self.socket.recv()
                if frame is None or frame == b"":
                    raise ConnectionError("microphone WebSocket closed")
                if isinstance(frame, str):
                    try:
                        self.metadata.update(json.loads(frame))
                    except json.JSONDecodeError:
                        pass
                    continue
                with self.lock:
                    if self.capture is None:
                        continue
                    remaining = self.target_bytes - len(self.capture)
                    self.capture.extend(frame[:remaining])
                    if len(self.capture) >= self.target_bytes:
                        self.complete.set()
        except Exception as exc:  # reader must report failures to the TUI thread
            self.error = exc
            self.complete.set()
        finally:
            self.closed.set()

    def record(self, seconds: float) -> bytes:
        with self.lock:
            self.target_bytes = int(seconds * SAMPLE_RATE) * SAMPLE_WIDTH
            self.capture = bytearray()
            self.complete.clear()
        started = time.monotonic()
        while not self.complete.wait(0.1):
            elapsed = min(seconds, time.monotonic() - started)
            width = int(30 * elapsed / seconds)
            print(
                f"\rRecording [{'#' * width}{'.' * (30 - width)}] "
                f"{elapsed:4.1f}/{seconds:.1f}s",
                end="",
                flush=True,
            )
        print()
        with self.lock:
            result = bytes(self.capture or b"")
            self.capture = None
        if self.error is not None:
            raise ConnectionError(f"microphone stream failed: {self.error}")
        if len(result) != self.target_bytes:
            raise ConnectionError(
                f"short microphone capture: {len(result)}/{self.target_bytes} bytes"
            )
        return result

    def close(self) -> None:
        if self.socket is not None:
            self.socket.close()
        if self.thread is not None:
            self.thread.join(timeout=2)


@dataclass
class Project:
    root: Path
    config: dict[str, Any]

    @property
    def config_path(self) -> Path:
        return self.root / "project.json"

    @classmethod
    def load(cls, root: Path, device: str, phrase: str) -> "Project":
        path = root / "project.json"
        if path.exists():
            config = json.loads(path.read_text())
        else:
            config = {
                "device": device,
                "phrase": phrase,
                "created_at": datetime.now(timezone.utc).isoformat(),
                "categories": {
                    name: {"count": values["count"], "seconds": values["seconds"]}
                    for name, values in CATEGORIES.items()
                },
            }
        if device:
            config["device"] = device
        if phrase:
            config["phrase"] = phrase
        project = cls(root.resolve(), config)
        project.save()
        return project

    def save(self) -> None:
        atomic_json(self.config_path, self.config)

    def directory(self, category: str) -> Path:
        return self.root / "recordings" / category

    def files(self, category: str) -> list[Path]:
        return sorted(self.directory(category).glob("*.wav"))

    def target(self, category: str) -> int:
        return int(self.config["categories"][category]["count"])

    def seconds(self, category: str) -> float:
        return float(self.config["categories"][category]["seconds"])


def countdown() -> None:
    for value in (3, 2, 1):
        print(f"\rStarting in {value}...", end="", flush=True)
        time.sleep(1)
    print("\rSpeak now.       ")


def next_path(project: Project, category: str) -> Path:
    existing = project.files(category)
    number = 1
    if existing:
        try:
            number = max(int(path.stem.split("_", 1)[0]) for path in existing) + 1
        except ValueError:
            number = len(existing) + 1
    return project.directory(category) / f"{number:04d}_{category}.wav"


def collect_category(project: Project, category: str) -> None:
    settings = CATEGORIES[category]
    target = project.target(category)
    if len(project.files(category)) >= target:
        input(f"{settings['title']} already has {target} samples. Press Enter.")
        return
    device = str(project.config["device"]).strip().rstrip("/")
    if not device:
        device = input("Brain IP address or hostname: ").strip()
        project.config["device"] = device
        project.save()
    url = device if device.startswith("ws://") else f"ws://{device}/api/audio/mic"
    stream = MicrophoneStream(url)
    print(f"Connecting to {url} ...")
    try:
        stream.connect()
        print(
            "Connected: "
            f"gain={stream.metadata.get('gain', '?')}x, "
            f"VAD={stream.metadata.get('vad_threshold', '?')}"
        )
        while len(project.files(category)) < target:
            completed = len(project.files(category))
            clear()
            print(f"SarcasmOS wake-word collection — {settings['title']}")
            print("=" * 64)
            print(f"Wake phrase: {project.config['phrase']}")
            print(f"Progress:    {completed}/{target}")
            print(f"Duration:    {project.seconds(category):.1f} seconds")
            print(f"\n{settings['instruction']}\n")
            command = input("Enter=record, s=skip category, q=main menu: ").strip().lower()
            if command in {"s", "q"}:
                return
            countdown()
            pcm = stream.record(project.seconds(category))
            peak, rms, clipped = wav_metrics(pcm)
            print(f"Peak={peak}, RMS={rms}, clipped={clipped:.3f}%")
            if category == "positive" and peak < 100:
                print("Rejected automatically: effectively silent positive sample.")
                input("Press Enter to retry.")
                continue
            if clipped > 1.0:
                print("Warning: more than 1% of samples are clipped.")
            action = input("Enter=save, r=retry, q=save and return: ").strip().lower()
            if action == "r":
                continue
            output = next_path(project, category)
            save_wav(output, pcm)
            print(f"Saved {output}")
            if action == "q":
                return
    finally:
        stream.close()


def configure(project: Project) -> None:
    device = input(f"Brain IP [{project.config.get('device', '')}]: ").strip()
    phrase = input(f"Wake phrase [{project.config.get('phrase', '')}]: ").strip()
    if device:
        project.config["device"] = device
    if phrase:
        project.config["phrase"] = phrase
    for name, defaults in CATEGORIES.items():
        current = project.config["categories"][name]
        count = input(f"{defaults['title']} count [{current['count']}]: ").strip()
        seconds = input(f"{defaults['title']} seconds [{current['seconds']}]: ").strip()
        if count:
            current["count"] = max(1, int(count))
        if seconds:
            current["seconds"] = max(0.5, float(seconds))
    project.save()


def training_paths(project: Project) -> tuple[Path, Path, Path]:
    environment = project.root / ".training-venv"
    upstream = project.root / "vendor" / "micro-wake-word"
    python = environment / "bin" / "python"
    if os.name == "nt":
        python = environment / "Scripts" / "python.exe"
    return environment, upstream, python


def setup_training(project: Project) -> None:
    environment, upstream, python = training_paths(project)
    project.root.mkdir(parents=True, exist_ok=True)
    if not upstream.exists():
        upstream.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            ["git", "clone", "--depth", "1", UPSTREAM_URL, str(upstream)],
            check=True,
        )
    if not python.exists():
        candidates = [
            os.environ.get("MWW_PYTHON", ""), "python3.12", "python3.11",
            "python3.13", sys.executable,
        ]
        creator = None
        for candidate in candidates:
            if not candidate:
                continue
            resolved = shutil.which(candidate) if not os.path.isabs(candidate) else candidate
            if not resolved:
                continue
            version = subprocess.run(
                [resolved, "-c", "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')"],
                check=True, capture_output=True, text=True,
            ).stdout.strip()
            major, minor = (int(part) for part in version.split("."))
            if major == 3 and 10 <= minor <= 13:
                creator = resolved
                break
        if creator is None:
            bootstrap = project.root / ".bootstrap-venv"
            bootstrap_python = bootstrap / "bin" / "python"
            bootstrap_uv = bootstrap / "bin" / "uv"
            if os.name == "nt":
                bootstrap_python = bootstrap / "Scripts" / "python.exe"
                bootstrap_uv = bootstrap / "Scripts" / "uv.exe"
            if not bootstrap_python.exists():
                subprocess.run(
                    [sys.executable, "-m", "venv", str(bootstrap)], check=True
                )
            if not bootstrap_uv.exists():
                subprocess.run(
                    [str(bootstrap_python), "-m", "pip", "install", "uv"],
                    check=True,
                )
            subprocess.run([str(bootstrap_uv), "python", "install", "3.12"], check=True)
            subprocess.run(
                [str(bootstrap_uv), "venv", "--seed", "--python", "3.12",
                 str(environment)],
                check=True,
            )
        else:
            subprocess.run([creator, "-m", "venv", str(environment)], check=True)
    subprocess.run([str(python), "-m", "pip", "install", "-U", "pip"], check=True)
    subprocess.run([str(python), "-m", "pip", "install", "-e", str(upstream)], check=True)
    print(f"Training environment ready: {environment}")


def validate_training_data(project: Project) -> None:
    minimums = {"positive": 20, "negative": 20, "noise": 10, "blank": 10}
    missing = [
        f"{name}: {len(project.files(name))}/{minimum} minimum"
        for name, minimum in minimums.items()
        if len(project.files(name)) < minimum
    ]
    if missing:
        raise RuntimeError("collect more samples before training:\n  " + "\n  ".join(missing))


def run_training(project: Project, steps: int) -> None:
    validate_training_data(project)
    _, upstream, python = training_paths(project)
    if not python.exists() or not upstream.exists():
        raise RuntimeError("training environment is absent; choose Setup first")
    command = [
        str(python), str(Path(__file__).resolve()), "--train-worker",
        "--project", str(project.root), "--mww-root", str(upstream),
        "--steps", str(steps),
    ]
    subprocess.run(command, check=True)


def _make_mmap(output: Path, generator: Any) -> None:
    from mmap_ninja.ragged import RaggedMmap

    output.parent.mkdir(parents=True, exist_ok=True)
    RaggedMmap.from_generator(
        out_dir=str(output), sample_generator=generator,
        batch_size=100, verbose=True,
    )


def _prepare_feature_set(
    recordings: Path, output: Path, *, positive: bool,
    backgrounds: list[str], ambient: bool = False,
) -> None:
    from microwakeword.audio.augmentation import Augmentation
    from microwakeword.audio.clips import Clips
    from microwakeword.audio.spectrograms import SpectrogramGeneration

    clips = Clips(
        input_directory=str(recordings), file_pattern="*.wav",
        remove_silence=positive, random_split_seed=117, split_count=0.1,
    )
    probabilities = {
        "SevenBandParametricEQ": 0.1, "TanhDistortion": 0.1,
        "PitchShift": 0.1 if positive else 0.0, "BandStopFilter": 0.1,
        "AddColorNoise": 0.15, "AddBackgroundNoise": 0.65,
        "Gain": 1.0, "GainTransition": 0.1, "RIR": 0.0,
    }
    augmenter = Augmentation(
        augmentation_duration_s=3.2,
        augmentation_probabilities=probabilities,
        background_paths=backgrounds,
        background_min_snr_db=-5, background_max_snr_db=15,
        min_jitter_s=0.15, max_jitter_s=0.30,
    )
    for mode, split in (("training", "train"),
                        ("validation", "validation"),
                        ("testing", "test")):
        generation = SpectrogramGeneration(
            clips, augmenter if mode == "training" else None,
            slide_frames=10 if mode != "testing" else 1, step_ms=10,
        )
        _make_mmap(
            output / mode / f"{recordings.name}_mmap",
            generation.spectrogram_generator(
                split=split, repeat=8 if mode == "training" else 1
            ),
        )
    if ambient:
        generation = SpectrogramGeneration(clips, None, step_ms=10)
        for mode, split in (("validation_ambient", "validation"),
                            ("testing_ambient", "test")):
            _make_mmap(
                output / mode / f"{recordings.name}_mmap",
                generation.spectrogram_generator(split=split),
            )


def training_worker(project_root: Path, mww_root: Path, steps: int) -> None:
    sys.path.insert(0, str(mww_root))
    import yaml

    project = Project.load(project_root, "", "")
    validate_training_data(project)
    run_id = datetime.now().strftime("%Y%m%d-%H%M%S")
    run_root = project.root / "training" / run_id
    features_root = run_root / "features"
    background_dirs = [
        str(project.directory("noise")), str(project.directory("blank"))
    ]
    feature_entries: list[dict[str, Any]] = []
    definitions = (
        ("positive", True, 3.0, False),
        ("negative", False, 10.0, False),
        ("blank", False, 4.0, False),
        ("noise", False, 8.0, True),
    )
    for name, truth, weight, ambient in definitions:
        destination = features_root / name
        print(f"\nPreparing {name} features...")
        _prepare_feature_set(
            project.directory(name), destination,
            positive=truth, backgrounds=background_dirs,
            ambient=ambient,
        )
        feature_entries.append({
            "features_dir": str(destination),
            "sampling_weight": weight,
            "penalty_weight": 1.0,
            "truth": truth,
            "truncation_strategy": "split" if ambient else (
                "truncate_start" if truth else "random"
            ),
            "type": "mmap",
        })
    train_dir = run_root / "trained_models" / "wakeword"
    config = {
        "window_step_ms": 10,
        "train_dir": str(train_dir),
        "features": feature_entries,
        "training_steps": [steps],
        "positive_class_weight": [1],
        "negative_class_weight": [20],
        "learning_rates": [0.001],
        "batch_size": 128,
        "time_mask_max_size": [0], "time_mask_count": [0],
        "freq_mask_max_size": [0], "freq_mask_count": [0],
        "eval_step_interval": min(500, max(50, steps // 10)),
        "clip_duration_ms": 1500,
        "target_minimization": 0.9,
        "minimization_metric": None,
        "maximization_metric": "average_viable_recall",
    }
    config_path = run_root / "training_parameters.yaml"
    config_path.parent.mkdir(parents=True, exist_ok=True)
    config_path.write_text(yaml.safe_dump(config, sort_keys=False))
    command = [
        sys.executable, "-m", "microwakeword.model_train_eval",
        f"--training_config={config_path}", "--train", "1",
        "--restore_checkpoint", "0", "--test_tf_nonstreaming", "0",
        "--test_tflite_nonstreaming", "0",
        "--test_tflite_nonstreaming_quantized", "0",
        "--test_tflite_streaming", "0",
        "--test_tflite_streaming_quantized", "1",
        "--use_weights", "best_weights", "mixednet",
        "--pointwise_filters", "64,64,64,64",
        "--repeat_in_block", "1,1,1,1",
        "--mixconv_kernel_sizes", "[5], [7,11], [9,15], [23]",
        "--residual_connection", "0,0,0,0",
        "--first_conv_filters", "32", "--first_conv_kernel_size", "5",
        "--stride", "3",
    ]
    environment = os.environ.copy()
    environment["PYTHONPATH"] = str(mww_root) + os.pathsep + environment.get("PYTHONPATH", "")
    subprocess.run(command, check=True, cwd=project.root, env=environment)
    source = train_dir / "tflite_stream_state_internal_quant" / "stream_state_internal_quant.tflite"
    if not source.exists():
        raise RuntimeError(f"training finished without producing {source}")
    output = project.root / "models" / f"{project.config['phrase'].replace(' ', '_')}.tflite"
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, output)
    atomic_json(output.with_suffix(".json"), {
        "type": "micro", "wake_word": project.config["phrase"],
        "author": "SarcasmOS local trainer", "website": "",
        "model": output.name, "version": 2,
        "micro": {"probability_cutoff": 0.5, "sliding_window_size": 5,
                  "feature_step_size": 10, "tensor_arena_size": 100000},
    })
    print(f"\nQuantized wake-word model: {output}")
    print(f"Model metadata:             {output.with_suffix('.json')}")


def show_menu(project: Project) -> None:
    while True:
        clear()
        print("SarcasmOS local wake-word trainer")
        print("=" * 64)
        print(f"Project:     {project.root}")
        print(f"Brain:       {project.config.get('device') or '<not set>'}")
        print(f"Wake phrase: {project.config.get('phrase') or '<not set>'}\n")
        for key, name in zip("1234", CATEGORIES):
            values = CATEGORIES[name]
            print(
                f"  {key}  {values['title']:<24} "
                f"{len(project.files(name)):>3}/{project.target(name):<3}"
            )
        print("\n  a  Collect every incomplete category")
        print("  c  Configure device, phrase, counts and durations")
        print("  s  Set up/update isolated microWakeWord environment")
        print("  t  Prepare features, train, evaluate and export .tflite")
        print("  q  Quit")
        command = input("\nSelection: ").strip().lower()
        try:
            if command in "1234" and command:
                collect_category(project, list(CATEGORIES)[int(command) - 1])
            elif command == "a":
                for category in CATEGORIES:
                    if len(project.files(category)) < project.target(category):
                        collect_category(project, category)
            elif command == "c":
                configure(project)
            elif command == "s":
                setup_training(project)
                input("Press Enter to continue.")
            elif command == "t":
                raw_steps = input("Training steps [10000]: ").strip()
                run_training(project, int(raw_steps or "10000"))
                input("Press Enter to continue.")
            elif command == "q":
                return
        except (Exception, KeyboardInterrupt) as exc:
            print(f"\nError: {exc}")
            input("Press Enter to return to the menu.")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default="192.168.1.125")
    parser.add_argument("--phrase", default="oye bender")
    parser.add_argument("--project", type=Path, default=Path("wakeword_data"))
    parser.add_argument("--train-worker", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--mww-root", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--steps", type=int, default=10_000, help=argparse.SUPPRESS)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    if arguments.train_worker:
        if arguments.mww_root is None:
            raise SystemExit("--mww-root is required for --train-worker")
        training_worker(arguments.project.resolve(), arguments.mww_root.resolve(), arguments.steps)
        return 0
    project = Project.load(arguments.project, arguments.device, arguments.phrase)
    show_menu(project)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
