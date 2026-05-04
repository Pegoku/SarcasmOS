import argparse
import os
from collections.abc import Iterable
from pathlib import Path
from urllib.request import urlopen

import replicate


DEFAULT_MODEL = "minimax/speech-2.8-turbo"
DEFAULT_OUTPUT = "minimax-output.mp3"

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


def first_output(output: object) -> object:
    if isinstance(output, (str, bytes)) or hasattr(output, "read"):
        return output

    if isinstance(output, Iterable):
        for item in output:
            return item

    return output


def output_url(output: object) -> str | None:
    url = getattr(output, "url", None)
    if isinstance(url, str) and url:
        return url

    if isinstance(output, str) and output.startswith(("http://", "https://")):
        return output

    return None


def write_output(output: object, output_path: Path) -> None:
    output = first_output(output)

    if hasattr(output, "read"):
        output_path.write_bytes(output.read())
        return

    if isinstance(output, bytes):
        output_path.write_bytes(output)
        return

    url = output_url(output)
    if url:
        with urlopen(url, timeout=120) as response:
            output_path.write_bytes(response.read())
        return

    raise RuntimeError("Prediction succeeded but no downloadable audio output was returned.")


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
        default=os.environ.get("REPLICATE_BASE_URL"),
        help="Optional Replicate-compatible API base URL. Defaults to the official Replicate API.",
    )
    args = parser.parse_args()

    validate_range("speed", args.speed, 0.5, 2.0)
    validate_range("volume", args.volume, 0, 10)
    validate_range("pitch", args.pitch, -12, 12)

    api_token = os.environ.get("REPLICATE_API_TOKEN")
    if not api_token:
        raise RuntimeError("REPLICATE_API_TOKEN is not set. Add it to .env or export it in your shell.")

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

    base_url = args.replicate_base_url.rstrip("/") if args.replicate_base_url else None
    client = replicate.Client(api_token=api_token, base_url=base_url)
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output = client.run(args.model, input=model_input)
    write_output(output, output_path)

    url = output_url(first_output(output))
    if url:
        print(f"audio_url: {url}")
    print(f"saved: {output_path}")


if __name__ == "__main__":
    main()
