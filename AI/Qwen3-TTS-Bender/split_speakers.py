import argparse
import json
import re
from pathlib import Path

import librosa
import numpy as np
import soundfile as sf


DEFAULT_MODEL = "pyannote/speaker-diarization-3.1"


def sanitize_label(label: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9._-]+", "_", label.strip())
    return sanitized or "speaker"


def load_pipeline(model_name: str, hf_token: str | None):
    try:
        from pyannote.audio import Pipeline
    except ImportError as exc:
        raise RuntimeError(
            "pyannote.audio is not installed. Install it with `pip install pyannote.audio`."
        ) from exc

    pipeline_kwargs = {}
    if hf_token:
        pipeline_kwargs["use_auth_token"] = hf_token

    try:
        return Pipeline.from_pretrained(model_name, **pipeline_kwargs)
    except Exception as exc:
        raise RuntimeError(
            "Failed to load the pyannote pipeline. Make sure you accepted the model terms on "
            "Hugging Face and supplied a valid token with `--hf-token` or `HF_TOKEN`."
        ) from exc


def diarize_audio(
    audio_path: Path,
    model_name: str,
    hf_token: str | None,
    min_speakers: int | None,
    max_speakers: int | None,
):
    pipeline = load_pipeline(model_name, hf_token)
    diarization_kwargs = {}
    if min_speakers is not None:
        diarization_kwargs["min_speakers"] = min_speakers
    if max_speakers is not None:
        diarization_kwargs["max_speakers"] = max_speakers
    return pipeline(str(audio_path), **diarization_kwargs)


def collect_segments(diarization) -> dict[str, list[dict[str, float]]]:
    speakers: dict[str, list[dict[str, float]]] = {}
    for turn, _, speaker in diarization.itertracks(yield_label=True):
        speakers.setdefault(speaker, []).append(
            {"start": float(turn.start), "end": float(turn.end)}
        )
    return speakers


def read_audio(audio_path: Path, sample_rate: int | None) -> tuple[np.ndarray, int]:
    audio, sr = librosa.load(audio_path, sr=sample_rate, mono=False)
    if audio.ndim == 1:
        return audio.astype(np.float32), sr

    # Collapse multi-channel audio so segment extraction is straightforward.
    return np.mean(audio, axis=0, dtype=np.float32), sr


def extract_segment(audio: np.ndarray, sr: int, start: float, end: float) -> np.ndarray:
    start_index = max(0, int(round(start * sr)))
    end_index = min(len(audio), int(round(end * sr)))
    if end_index <= start_index:
        return np.array([], dtype=np.float32)
    return audio[start_index:end_index]


def export_speaker_files(
    audio: np.ndarray,
    sr: int,
    speakers: dict[str, list[dict[str, float]]],
    output_dir: Path,
    prefix: str,
) -> list[dict[str, object]]:
    manifest: list[dict[str, object]] = []

    for index, (speaker, segments) in enumerate(sorted(speakers.items()), start=1):
        chunks = [extract_segment(audio, sr, segment["start"], segment["end"]) for segment in segments]
        chunks = [chunk for chunk in chunks if len(chunk) > 0]
        if not chunks:
            continue

        speaker_audio = np.concatenate(chunks)
        speaker_name = sanitize_label(speaker)
        output_path = output_dir / f"{prefix}_speaker_{index:02d}_{speaker_name}.wav"
        sf.write(output_path, speaker_audio, sr)

        manifest.append(
            {
                "speaker": speaker,
                "speaker_index": index,
                "output_path": str(output_path),
                "duration_seconds": round(len(speaker_audio) / sr, 3),
                "segments": segments,
            }
        )

    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Split an audio file into one WAV per detected speaker using pyannote.audio diarization"
    )
    parser.add_argument("audio", help="Input audio file path")
    parser.add_argument(
        "--output-dir",
        default="speaker_splits",
        help="Directory where speaker files and the manifest will be written",
    )
    parser.add_argument(
        "--prefix",
        default=None,
        help="Optional output filename prefix. Defaults to the input filename stem.",
    )
    parser.add_argument(
        "--model",
        default=DEFAULT_MODEL,
        help="pyannote diarization model name to load from Hugging Face",
    )
    parser.add_argument(
        "--hf-token",
        default=None,
        help="Hugging Face token. If omitted, pyannote will also check your local auth cache.",
    )
    parser.add_argument(
        "--sample-rate",
        type=int,
        default=None,
        help="Optional resample rate before exporting speaker files",
    )
    parser.add_argument(
        "--min-speakers",
        type=int,
        default=None,
        help="Optional minimum number of speakers to guide diarization",
    )
    parser.add_argument(
        "--max-speakers",
        type=int,
        default=None,
        help="Optional maximum number of speakers to guide diarization",
    )
    args = parser.parse_args()

    audio_path = Path(args.audio)
    if not audio_path.is_file():
        raise FileNotFoundError(f"Audio file not found: {audio_path}")

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    diarization = diarize_audio(
        audio_path=audio_path,
        model_name=args.model,
        hf_token=args.hf_token,
        min_speakers=args.min_speakers,
        max_speakers=args.max_speakers,
    )
    speakers = collect_segments(diarization)
    if not speakers:
        raise RuntimeError("No speakers were detected in the audio.")

    audio, sr = read_audio(audio_path, args.sample_rate)
    prefix = args.prefix or audio_path.stem
    manifest = export_speaker_files(audio, sr, speakers, output_dir, prefix)
    manifest_path = output_dir / f"{prefix}_segments.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    print(f"Detected {len(manifest)} speakers")
    for entry in manifest:
        print(f"{entry['speaker']}: {entry['output_path']}")
    print(f"Manifest: {manifest_path}")


if __name__ == "__main__":
    main()
