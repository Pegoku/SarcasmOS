import argparse
import base64
import mimetypes
import os
import shutil
import subprocess
import time
from pathlib import Path

import requests


DEFAULT_REPLICATE_BASE_URL = "https://ai.hackclub.com/proxy/v1/replicate"
DEFAULT_MODEL = "qwen/qwen3-tts"
DEFAULT_POLL_INTERVAL = 1.0
DEFAULT_TEXT = (
    "Escucha, saco de carne: soy Bender, doblador, bebedor profesional y robot "
    "superior. Que quieres? Habla rapido, que mi bateria no se va a cargar sola... "
    "y no pienso hacerlo gratis"
)


def load_dotenv(env_path: Path) -> None:
    if not env_path.is_file():
        return

    for raw_line in env_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip("\"'")
        os.environ.setdefault(key, value)


def load_reference_text(ref_audio: Path, ref_text_path: Path | None) -> str:
    if ref_text_path is None:
        ref_text_path = ref_audio.with_suffix(".txt")

    if not ref_text_path.is_file():
        raise FileNotFoundError(f"Reference transcript not found: {ref_text_path}")

    ref_text = ref_text_path.read_text(encoding="utf-8").strip()
    if not ref_text:
        raise ValueError(f"Reference transcript is empty: {ref_text_path}")

    return ref_text


def file_to_data_uri(file_path: Path) -> str:
    mime_type, _ = mimetypes.guess_type(file_path.name)
    if not mime_type:
        mime_type = "application/octet-stream"

    encoded = base64.b64encode(file_path.read_bytes()).decode("ascii")
    return f"data:{mime_type};base64,{encoded}"


def create_prediction(api_token: str, model_input: dict, stream: bool) -> dict:
    response = requests.post(
        f"{DEFAULT_REPLICATE_BASE_URL}/models/{DEFAULT_MODEL}/predictions",
        headers={
            "Authorization": f"Bearer {api_token}",
            "Content-Type": "application/json",
        },
        json={"input": model_input, "stream": stream},
        timeout=120,
    )
    response.raise_for_status()
    return response.json()


def wait_for_prediction(api_token: str, prediction_id: str) -> dict:
    while True:
        response = requests.get(
            f"{DEFAULT_REPLICATE_BASE_URL}/predictions/{prediction_id}",
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

        time.sleep(DEFAULT_POLL_INTERVAL)


def resolve_stream_player() -> list[str]:
    if shutil.which("ffplay"):
        return ["ffplay", "-autoexit", "-nodisp", "-i", "pipe:0"]

    if shutil.which("vlc"):
        return ["vlc", "--play-and-exit", "-"]

    raise RuntimeError("--stream requires ffplay or vlc to be installed.")


def download_output(output_url: str, output_path: Path, stream_audio: bool) -> None:
    response = requests.get(output_url, stream=True, timeout=120)
    response.raise_for_status()

    player = None
    if stream_audio:
        player = subprocess.Popen(resolve_stream_player(), stdin=subprocess.PIPE)

    with output_path.open("wb") as file:
        for chunk in response.iter_content(chunk_size=65536):
            if not chunk:
                continue

            file.write(chunk)
            if player and player.stdin:
                player.stdin.write(chunk)
                player.stdin.flush()

    if player and player.stdin:
        player.stdin.close()
        player.wait()


def main():
    parser = argparse.ArgumentParser(
        description="Clone speech with Replicate's qwen/qwen3-tts API"
    )
    parser.add_argument(
        "ref_audio",
        nargs="?",
        help="Reference audio path. If --ref-text is omitted, the transcript defaults to the same filename with .txt",
    )
    parser.add_argument(
        "--ref-audio",
        dest="ref_audio_flag",
        help="Reference audio path for voice cloning",
    )
    parser.add_argument(
        "--ref-text",
        help="Reference transcript path. Defaults to the same filename as the reference audio with .txt",
    )
    parser.add_argument(
        "--text",
        default=DEFAULT_TEXT,
        help="Target text to synthesize into speech",
    )
    parser.add_argument(
        "--output",
        default="bender-es-api.wav",
        help="Output WAV filename",
    )
    parser.add_argument(
        "--language",
        default="Spanish",
        choices=[
            "auto",
            "Chinese",
            "English",
            "Japanese",
            "Korean",
            "French",
            "German",
            "Italian",
            "Spanish",
            "Portuguese",
            "Russian",
        ],
        help="Target language for the synthesized text",
    )
    parser.add_argument(
        "--style-instruction",
        default=None,
        help="Optional speaking style or emotion instruction",
    )
    parser.add_argument(
        "--stream",
        action="store_true",
        help="Play audio while downloading the final output file after generation completes",
    )
    args = parser.parse_args()

    load_dotenv(Path(__file__).with_name(".env"))
    api_token = os.environ.get("REPLICATE_API_TOKEN")
    if not api_token:
        raise RuntimeError("REPLICATE_API_TOKEN is not set. Add it to .env or export it in your shell.")

    ref_audio_value = args.ref_audio_flag or args.ref_audio
    if not ref_audio_value:
        raise RuntimeError("Reference audio is required. Pass it as the positional argument or with --ref-audio.")

    ref_audio = Path(ref_audio_value)
    if not ref_audio.is_file():
        raise FileNotFoundError(f"Reference audio not found: {ref_audio}")

    ref_text_path = Path(args.ref_text) if args.ref_text else None
    ref_text = load_reference_text(ref_audio, ref_text_path)
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    model_input = {
        "mode": "voice_clone",
        "text": args.text,
        "language": args.language,
        "reference_text": ref_text,
    }
    if args.style_instruction:
        model_input["style_instruction"] = args.style_instruction

    model_input["reference_audio"] = file_to_data_uri(ref_audio)
    prediction = create_prediction(api_token, model_input, args.stream)
    prediction = wait_for_prediction(api_token, prediction["id"])

    output_url = prediction.get("output")
    if not output_url:
        raise RuntimeError("Prediction succeeded but no output URL was returned.")

    print(output_url)
    download_output(output_url, output_path, args.stream)

    print(f"Saved to {output_path}")


if __name__ == "__main__":
    main()
