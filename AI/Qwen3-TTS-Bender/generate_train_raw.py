import argparse
import json
import re
from pathlib import Path


def clip_sort_key(path: Path) -> tuple[int, str]:
    match = re.search(r"(\d+)$", path.stem)
    if match:
        return int(match.group(1)), path.name
    return 10**9, path.name


def normalize_text(text: str) -> str:
    return " ".join(text.split())


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate train_raw.jsonl from paired WAV and TXT files"
    )
    parser.add_argument(
        "--clips-dir",
        default="Clips/Good",
        help="Directory containing .wav files and matching .txt transcripts",
    )
    parser.add_argument(
        "--output",
        default="train_raw.jsonl",
        help="Output JSONL path",
    )
    parser.add_argument(
        "--ref-audio",
        default="./Clips/Good/bender-voz20.wav",
        help="Reference audio path written into each JSONL row",
    )
    args = parser.parse_args()

    clips_dir = Path(args.clips_dir)
    if not clips_dir.is_dir():
        raise FileNotFoundError(f"Clips directory not found: {clips_dir}")

    wav_paths = sorted(clips_dir.glob("*.wav"), key=clip_sort_key)
    if not wav_paths:
        raise FileNotFoundError(f"No .wav files found in: {clips_dir}")

    rows = []
    for wav_path in wav_paths:
        txt_path = wav_path.with_suffix(".txt")
        if not txt_path.is_file():
            raise FileNotFoundError(f"Missing transcript for {wav_path}: {txt_path}")

        text = normalize_text(txt_path.read_text(encoding="utf-8"))
        if not text:
            raise ValueError(f"Transcript is empty: {txt_path}")

        rows.append(
            {
                "audio": f"./{wav_path.as_posix()}",
                "text": text,
                "ref_audio": args.ref_audio,
            }
        )

    output_path = Path(args.output)
    output_path.write_text(
        "\n".join(json.dumps(row, ensure_ascii=False) for row in rows) + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {len(rows)} rows to {output_path}")


if __name__ == "__main__":
    main()
