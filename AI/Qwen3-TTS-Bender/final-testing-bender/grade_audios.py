#!/usr/bin/env python3

import argparse
import csv
import datetime as dt
import os
import re
import select
import shutil
import subprocess
import sys
import termios
import tty
from contextlib import contextmanager
from pathlib import Path


AUDIO_EXTENSIONS = {".wav", ".mp3", ".flac", ".m4a", ".ogg"}
CSV_HEADERS = ["group", "audio_file", "audio_path", "rating", "comment", "graded_at"]


def natural_key(value: str):
    return [
        int(part) if part.isdigit() else part.lower()
        for part in re.split(r"(\d+)", value)
    ]


def find_groups(root: Path):
    groups = []
    for path in root.iterdir():
        if not path.is_dir():
            continue
        audio_files = sorted(
            [
                item
                for item in path.iterdir()
                if item.is_file() and item.suffix.lower() in AUDIO_EXTENSIONS
            ],
            key=lambda item: natural_key(item.name),
        )
        if audio_files:
            groups.append((path, audio_files))
    groups.sort(key=lambda item: natural_key(item[0].name))
    return groups


def load_existing_results(csv_path: Path):
    graded = {}
    if not csv_path.exists():
        return graded

    with csv_path.open("r", newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            graded[row["audio_path"]] = row
    return graded


def ensure_csv(csv_path: Path):
    if csv_path.exists():
        return
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_HEADERS)
        writer.writeheader()


def append_result(csv_path: Path, row):
    with csv_path.open("a", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_HEADERS)
        writer.writerow(row)


def detect_player():
    candidates = [
        ["ffplay", "-nodisp", "-autoexit", "{file}"],
        ["mpv", "--really-quiet", "{file}"],
        ["cvlc", "--play-and-exit", "{file}"],
        ["vlc", "--intf", "dummy", "--play-and-exit", "{file}"],
        ["aplay", "{file}"],
    ]

    for candidate in candidates:
        if shutil.which(candidate[0]):
            return candidate
    return None


@contextmanager
def cbreak_stdin():
    if not sys.stdin.isatty():
        yield False
        return

    fd = sys.stdin.fileno()
    original_settings = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        yield True
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, original_settings)


def stop_process(process: subprocess.Popen):
    if process.poll() is not None:
        return

    process.terminate()
    try:
        process.wait(timeout=1)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def play_audio(player_cmd, audio_path: Path):
    if not player_cmd:
        print(f"No supported audio player found. Play this file manually: {audio_path}")
        return

    command = [part if part != "{file}" else str(audio_path) for part in player_cmd]
    try:
        if os.name != "posix" or not sys.stdin.isatty():
            subprocess.run(command, check=False)
            return

        print("Press 'e' while playing to stop early.")
        with cbreak_stdin() as stdin_ready:
            if not stdin_ready:
                subprocess.run(command, check=False)
                return

            process = subprocess.Popen(command)
            while process.poll() is None:
                ready, _, _ = select.select([sys.stdin], [], [], 0.1)
                if not ready:
                    continue

                key = sys.stdin.read(1).lower()
                if key == "e":
                    stop_process(process)
                    print("Playback stopped early.")
                    break

            if process.poll() is None:
                process.wait()
    except OSError as exc:
        print(f"Could not play {audio_path.name}: {exc}")


def ask_rating(audio_name: str):
    while True:
        raw = (
            input(
                f"Rating for {audio_name} (0-10, r=replay, s=skip, n=next group, q=quit): "
            )
            .strip()
            .lower()
        )
        if raw == "q":
            raise KeyboardInterrupt
        if raw == "r":
            return "replay"
        if raw == "s":
            return None
        if raw == "n":
            return "next_group"
        try:
            value = float(raw)
        except ValueError:
            print(
                "Enter a number from 0 to 10, 'r' to replay, 's' to skip, 'n' for next group, or 'q' to quit."
            )
            continue
        if 0 <= value <= 10:
            if value.is_integer():
                return int(value)
            return value
        print("Rating must be between 0 and 10.")


def grade_group(group_path: Path, audio_files, csv_path: Path, graded, player_cmd):
    print(f"\n=== {group_path.name} ({len(audio_files)} files) ===")
    current_rows = []

    for index, audio_path in enumerate(audio_files, start=1):
        audio_key = str(audio_path)
        if audio_key in graded:
            existing = graded[audio_key]
            current_rows.append(existing)
            print(
                f"[{index}/{len(audio_files)}] {audio_path.name} already graded: {existing['rating']}"
            )
            continue

        print(f"\n[{index}/{len(audio_files)}] {audio_path.name}")
        play_audio(player_cmd, audio_path)
        while True:
            rating = ask_rating(audio_path.name)
            if rating == "replay":
                play_audio(player_cmd, audio_path)
                continue
            break

        if rating == "next_group":
            print(f"Moving to the next group from {group_path.name}.")
            break
        if rating is None:
            print("Skipped.")
            continue

        comment = input("Optional comment: ").strip()
        row = {
            "group": group_path.name,
            "audio_file": audio_path.name,
            "audio_path": audio_key,
            "rating": rating,
            "comment": comment,
            "graded_at": dt.datetime.now().isoformat(timespec="seconds"),
        }
        append_result(csv_path, row)
        graded[audio_key] = row
        current_rows.append(row)

    numeric_ratings = [
        float(row["rating"])
        for row in current_rows
        if str(row.get("rating", "")).strip()
    ]
    if numeric_ratings:
        average = sum(numeric_ratings) / len(numeric_ratings)
        print(f"Group average for {group_path.name}: {average:.2f}")
    else:
        print(f"No ratings recorded yet for {group_path.name}.")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Grade audio files inside each subfolder and save ratings to a CSV file."
    )
    parser.add_argument(
        "root",
        nargs="?",
        default=".",
        help="Folder that contains the audio groups. Defaults to the current directory.",
    )
    parser.add_argument(
        "--output",
        default="audio_grades.csv",
        help="CSV file where grades will be stored. Defaults to audio_grades.csv.",
    )
    parser.add_argument(
        "--include-graded",
        action="store_true",
        help="Show already graded files and keep them in the session summary.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    root = Path(args.root).expanduser().resolve()
    csv_path = Path(args.output).expanduser().resolve()

    if not root.exists() or not root.is_dir():
        raise SystemExit(f"Folder not found: {root}")

    groups = find_groups(root)
    if not groups:
        raise SystemExit(f"No audio groups found in: {root}")

    ensure_csv(csv_path)
    graded = load_existing_results(csv_path)
    player_cmd = detect_player()

    print(f"Found {len(groups)} groups in {root}")
    print(f"Saving results to {csv_path}")
    if player_cmd:
        print(f"Using player: {player_cmd[0]}")
    else:
        print(
            "No supported player found. You can still grade by playing files manually."
        )

    try:
        for group_path, audio_files in groups:
            if not args.include_graded:
                remaining_files = [
                    audio for audio in audio_files if str(audio) not in graded
                ]
                if not remaining_files:
                    print(f"\n=== {group_path.name} ===")
                    print(
                        "All files already graded. Use --include-graded to show them again."
                    )
                    continue
                grade_group(group_path, remaining_files, csv_path, graded, player_cmd)
            else:
                grade_group(group_path, audio_files, csv_path, graded, player_cmd)
    except KeyboardInterrupt:
        print("\nStopped. Progress is already saved in the CSV file.")
        return

    print("\nFinished grading.")


if __name__ == "__main__":
    main()
