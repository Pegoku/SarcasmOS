import argparse
import os
import time
from pathlib import Path

import requests


DEFAULT_REPLICATE_BASE_URL = "https://ai.hackclub.com/proxy/v1/replicate"
DEFAULT_MODEL = "inworld/realtime-tts-1.5-mini"
DEFAULT_OUTPUT = "inworld-output.mp3"
DEFAULT_POLL_INTERVAL = 1.0


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


def create_prediction(api_token: str, base_url: str, model: str, model_input: dict) -> dict:
    response = requests.post(
        f"{base_url}/models/{model}/predictions",
        headers={
            "Authorization": f"Bearer {api_token}",
            "Content-Type": "application/json",
            "Prefer": "wait",
        },
        json={"input": model_input},
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


def download_file(file_url: str, output_path: Path) -> None:
    response = requests.get(file_url, timeout=120)
    response.raise_for_status()
    output_path.write_bytes(response.content)


def validate_range(name: str, value: float, minimum: float, maximum: float) -> None:
    if not minimum <= value <= maximum:
        raise ValueError(f"--{name} must be between {minimum:g} and {maximum:g}")


def main() -> None:
    load_dotenv(Path(__file__).with_name(".env"))

    parser = argparse.ArgumentParser(
        description="Generate audio with Inworld Realtime TTS 1.5 Mini via the Hack Club Replicate proxy"
    )
    parser.add_argument("--text", help="Text to narrate. Max 2,000 characters.")
    parser.add_argument("--text-file", help="UTF-8 text file to narrate instead of passing --text.")
    parser.add_argument(
        "--voice-id",
        default="Ashley",
        help="Inworld preset voice name, such as Ashley, Dennis, or Alex, or an Inworld custom cloned voice ID.",
    )
    parser.add_argument(
        "--sample-rate",
        type=int,
        default=48000,
        choices=[8000, 16000, 22050, 24000, 32000, 44100, 48000],
        help="Audio sample rate in Hz.",
    )
    parser.add_argument(
        "--temperature",
        type=float,
        default=0,
        help="Randomness from 0 to 2. Set 0 to use the model default.",
    )
    parser.add_argument(
        "--audio-format",
        default="mp3",
        choices=["mp3", "wav", "ogg_opus", "flac"],
        help="Generated audio format.",
    )
    parser.add_argument(
        "--speaking-rate",
        type=float,
        default=0,
        help="Speaking speed multiplier from 0 to 1.5. Set 0 for normal speed.",
    )
    parser.add_argument(
        "--text-normalization",
        default="auto",
        choices=["auto", "on", "off"],
        help="Controls number/date/abbreviation expansion.",
    )
    parser.add_argument("--model", default=DEFAULT_MODEL, help="Replicate model ref.")
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="Path to save generated audio.")
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
    args = parser.parse_args()

    validate_range("temperature", args.temperature, 0, 2)
    validate_range("speaking-rate", args.speaking_rate, 0, 1.5)
    validate_range("poll-interval", args.poll_interval, 0.1, 60)

    api_token = os.environ.get("HACK_CLUB_AI_KEY") or os.environ.get("REPLICATE_API_TOKEN")
    if not api_token:
        raise RuntimeError("HACK_CLUB_AI_KEY is not set. Add it to .env or export it in your shell.")

    text = read_text_arg(args.text, args.text_file)
    if not text:
        raise ValueError("Text is empty.")
    if len(text) > 2000:
        raise ValueError("Text must be 2,000 characters or fewer.")

    model_input = {
        "text": text,
        "voice_id": args.voice_id,
        "sample_rate": args.sample_rate,
        "temperature": args.temperature,
        "audio_format": args.audio_format,
        "speaking_rate": args.speaking_rate,
        "text_normalization": args.text_normalization,
    }

    base_url = args.replicate_base_url.rstrip("/")
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    prediction = create_prediction(api_token, base_url, args.model, model_input)
    if prediction.get("status") != "succeeded":
        prediction = wait_for_prediction(api_token, base_url, prediction["id"], args.poll_interval)

    output_url = get_output_url(prediction.get("output"))
    download_file(output_url, output_path)

    print(f"audio_url: {output_url}")
    print(f"saved: {output_path}")


if __name__ == "__main__":
    main()
