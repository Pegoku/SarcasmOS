import argparse
import os
import shutil
import subprocess
import time
from pathlib import Path

import requests


DEFAULT_REPLICATE_BASE_URL = "https://ai.hackclub.com/proxy/v1/replicate"
DEFAULT_MODEL = "minimax/speech-2.8-turbo"
DEFAULT_OUTPUT = "minimax-output.mp3"
DEFAULT_POLL_INTERVAL = 1.0

SUPPORTED_MODELS = [
    "minimax/speech-2.8-hd",
    "minimax/speech-2.8-turbo",
    "minimax/speech-02-turbo",
]

EMOTIONS = [
    "auto",
    "happy",
    "sad",
    "angry",
    "fearful",
    "disgusted",
    "surprised",
    "calm",
    "fluent",
    "neutral",
]

LANGUAGE_BOOSTS = [
    "None",
    "Automatic",
    "Chinese",
    "Chinese,Yue",
    "Cantonese",
    "English",
    "Arabic",
    "Russian",
    "Spanish",
    "French",
    "Portuguese",
    "German",
    "Turkish",
    "Dutch",
    "Ukrainian",
    "Vietnamese",
    "Indonesian",
    "Japanese",
    "Italian",
    "Korean",
    "Thai",
    "Polish",
    "Romanian",
    "Greek",
    "Czech",
    "Finnish",
    "Hindi",
    "Bulgarian",
    "Danish",
    "Hebrew",
    "Malay",
    "Persian",
    "Slovak",
    "Swedish",
    "Croatian",
    "Filipino",
    "Hungarian",
    "Norwegian",
    "Slovenian",
    "Catalan",
    "Nynorsk",
    "Tamil",
    "Afrikaans",
]


def load_dotenv(env_path: Path) -> None:
    if not env_path.is_file():
        return

    for raw_line in env_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        os.environ.setdefault(key.strip(), value.strip().strip("\"'"))


def read_text_arg(text: str | None, text_file: str | None) -> str:
    if text and text_file:
        raise ValueError("Use either --text or --text-file, not both.")

    if text_file:
        return Path(text_file).read_text(encoding="utf-8").strip()

    if text:
        return text.strip()

    raise ValueError("Text is required. Pass --text or --text-file.")


def create_prediction(api_token: str, base_url: str, model: str, model_input: dict, stream: bool) -> dict:
    response = requests.post(
        f"{base_url}/models/{model}/predictions",
        headers={
            "Authorization": f"Bearer {api_token}",
            "Content-Type": "application/json",
            "Prefer": "wait",
        },
        json={"input": model_input, "stream": stream},
        timeout=300,
    )
    response.raise_for_status()
    return response.json()


def wait_for_prediction(api_token: str, base_url: str, prediction_id: str, poll_interval: float) -> dict:
    while True:
        response = requests.get(
            f"{base_url}/predictions/{prediction_id}",
            headers={"Authorization": f"Bearer {api_token}"},
            timeout=120,
        )
        response.raise_for_status()
        prediction = response.json()
        status = prediction.get("status")

        if status == "succeeded":
            return prediction

        if status in {"failed", "canceled"}:
            error = prediction.get("error") or f"Prediction ended with status: {status}"
            raise RuntimeError(error)

        time.sleep(poll_interval)


def get_output_url(output: object) -> str:
    if isinstance(output, str) and output:
        return output

    if isinstance(output, list) and output:
        return get_output_url(output[0])

    if isinstance(output, dict):
        for key in ("audio", "url", "output", "file"):
            value = output.get(key)
            if value:
                return get_output_url(value)

    raise RuntimeError("Prediction succeeded but no audio URL was returned.")


def resolve_stream_player(audio_path: Path) -> list[str]:
    if shutil.which("ffplay"):
        return ["ffplay", "-hide_banner", "-loglevel", "error", "-autoexit", "-nodisp", str(audio_path)]

    if shutil.which("vlc"):
        return ["vlc", "--play-and-exit", str(audio_path)]

    raise RuntimeError("--stream requires ffplay or vlc to be installed.")


def download_file(file_url: str, output_path: Path, stream_audio: bool) -> None:
    response = requests.get(file_url, stream=True, timeout=120)
    response.raise_for_status()

    with output_path.open("wb") as file:
        for chunk in response.iter_content(chunk_size=65536):
            if not chunk:
                continue

            file.write(chunk)

    if stream_audio:
        subprocess.run(resolve_stream_player(output_path), check=True)


def validate_range(name: str, value: float, minimum: float, maximum: float) -> None:
    if not minimum <= value <= maximum:
        raise ValueError(f"--{name} must be between {minimum:g} and {maximum:g}")


def main() -> None:
    load_dotenv(Path(__file__).with_name(".env"))

    parser = argparse.ArgumentParser(
        description="Generate audio from text using a Minimax voice_id via Replicate"
    )
    parser.add_argument(
        "--text",
        help="Text to narrate. Use pause markers like <#0.5#> to insert pauses.",
    )
    parser.add_argument(
        "--text-file",
        help="UTF-8 text file to narrate instead of passing --text.",
    )
    parser.add_argument(
        "--voice-id",
        default=os.environ.get("MINIMAX_VOICE_ID"),
        help="Trained voice_id from minimax-vc.py. Can also be set with MINIMAX_VOICE_ID.",
    )
    parser.add_argument(
        "--model",
        default=DEFAULT_MODEL,
        choices=SUPPORTED_MODELS,
        help="Replicate Minimax speech model to use.",
    )
    parser.add_argument(
        "--output",
        default=DEFAULT_OUTPUT,
        help="Path to save generated audio.",
    )
    parser.add_argument(
        "--speed",
        type=float,
        default=1.0,
        help="Speech speed multiplier from 0.5 to 2.0.",
    )
    parser.add_argument(
        "--volume",
        type=float,
        default=1.0,
        help="Relative loudness from 0 to 10.",
    )
    parser.add_argument(
        "--pitch",
        type=int,
        default=0,
        help="Semitone offset from -12 to 12.",
    )
    parser.add_argument(
        "--emotion",
        default="auto",
        choices=EMOTIONS,
        help="Delivery style.",
    )
    parser.add_argument(
        "--english-normalization",
        action="store_true",
        help="Improve English number/date reading at the cost of some latency.",
    )
    parser.add_argument(
        "--sample-rate",
        type=int,
        default=32000,
        choices=[8000, 16000, 22050, 24000, 32000, 44100],
        help="Audio sample rate in Hz.",
    )
    parser.add_argument(
        "--bitrate",
        type=int,
        default=128000,
        choices=[32000, 64000, 128000, 256000],
        help="MP3 bitrate in bits per second. Only used with --audio-format mp3.",
    )
    parser.add_argument(
        "--audio-format",
        default="mp3",
        choices=["mp3", "wav", "flac", "pcm"],
        help="Generated audio format.",
    )
    parser.add_argument(
        "--channel",
        default="mono",
        choices=["mono", "stereo"],
        help="Audio channel mode.",
    )
    parser.add_argument(
        "--subtitle-enable",
        action="store_true",
        help="Request sentence timestamp metadata from Minimax when available.",
    )
    parser.add_argument(
        "--language-boost",
        default="None",
        choices=LANGUAGE_BOOSTS,
        help="Optional language hint. Use Automatic to let Minimax detect the language.",
    )
    parser.add_argument(
        "--replicate-base-url",
        default=os.environ.get("REPLICATE_BASE_URL", DEFAULT_REPLICATE_BASE_URL),
        help="Replicate-compatible API base URL.",
    )
    parser.add_argument(
        "--poll-interval",
        type=float,
        default=DEFAULT_POLL_INTERVAL,
        help="Seconds to wait between prediction status checks if Prefer: wait returns early.",
    )
    parser.add_argument(
        "--stream",
        action="store_true",
        help="Play audio while downloading the final output file after generation completes.",
    )
    args = parser.parse_args()

    validate_range("speed", args.speed, 0.5, 2.0)
    validate_range("volume", args.volume, 0, 10)
    validate_range("pitch", args.pitch, -12, 12)
    validate_range("poll-interval", args.poll_interval, 0.1, 60)

    api_token = os.environ.get("HACK_CLUB_AI_KEY") or os.environ.get("REPLICATE_API_TOKEN")
    if not api_token:
        raise RuntimeError("HACK_CLUB_AI_KEY is not set. Add it to .env or export it in your shell.")

    if not args.voice_id:
        raise RuntimeError("--voice-id is required. Use the voice_id printed by minimax-vc.py.")

    text = read_text_arg(args.text, args.text_file)
    if not text:
        raise ValueError("Text is empty.")
    if len(text) > 10000:
        raise ValueError("Text must be 10,000 characters or fewer.")

    model_input = {
        "text": text,
        "voice_id": args.voice_id,
        "speed": args.speed,
        "volume": args.volume,
        "pitch": args.pitch,
        "emotion": args.emotion,
        "english_normalization": args.english_normalization,
        "sample_rate": args.sample_rate,
        "bitrate": args.bitrate,
        "audio_format": args.audio_format,
        "channel": args.channel,
        "subtitle_enable": args.subtitle_enable,
        "language_boost": args.language_boost,
    }

    base_url = args.replicate_base_url.rstrip("/")
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    prediction = create_prediction(api_token, base_url, args.model, model_input, args.stream)
    if prediction.get("status") != "succeeded":
        prediction = wait_for_prediction(api_token, base_url, prediction["id"], args.poll_interval)

    output_url = get_output_url(prediction.get("output"))
    download_file(output_url, output_path, args.stream)

    print(f"audio_url: {output_url}")
    print(f"saved: {output_path}")


if __name__ == "__main__":
    main()
