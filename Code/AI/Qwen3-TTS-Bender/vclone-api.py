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
MODES = ["custom_voice", "voice_clone", "voice_design"]
SPEAKERS = ["Aiden", "Dylan", "Eric", "Ono_anna", "Ryan", "Serena", "Sohee", "Uncle_fu", "Vivian"]
LANGUAGES = [
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
]


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


def load_reference_text(ref_audio: Path | None, ref_text_path: Path | None) -> str | None:
    if ref_text_path is None and ref_audio is not None:
        default_ref_text_path = ref_audio.with_suffix(".txt")
        if default_ref_text_path.is_file():
            ref_text_path = default_ref_text_path

    if ref_text_path is None:
        return None

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


def audio_input(reference_audio: str) -> tuple[str, Path | None]:
    if reference_audio.startswith(("http://", "https://", "data:")):
        return reference_audio, None

    reference_audio_path = Path(reference_audio)
    if not reference_audio_path.is_file():
        raise FileNotFoundError(f"Reference audio not found: {reference_audio_path}")

    return file_to_data_uri(reference_audio_path), reference_audio_path


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

    raise RuntimeError("Prediction succeeded but no output URL was returned.")


def main():
    load_dotenv(Path(__file__).with_name(".env"))

    parser = argparse.ArgumentParser(
        description="Clone speech with Replicate's qwen/qwen3-tts API"
    )
    parser.add_argument(
        "ref_audio",
        nargs="?",
        help="Reference audio path for voice_clone mode. If --ref-text is omitted, a sibling .txt transcript is used when present",
    )
    parser.add_argument(
        "--ref-audio",
        dest="ref_audio_flag",
        help="Reference audio path for voice cloning",
    )
    parser.add_argument(
        "--reference-audio",
        help="Reference audio path, URL, or data URI for voice_clone mode",
    )
    parser.add_argument(
        "--ref-text",
        dest="reference_text_file",
        help="Reference transcript path. If omitted, a sibling .txt file is used when present",
    )
    parser.add_argument(
        "--reference-text",
        help="Transcript of the reference audio for voice_clone mode",
    )
    parser.add_argument(
        "--text",
        default=DEFAULT_TEXT,
        help="Target text to synthesize into speech",
    )
    parser.add_argument(
        "--mode",
        default="voice_clone",
        choices=MODES,
        help="TTS mode: custom_voice, voice_clone, or voice_design",
    )
    parser.add_argument(
        "--speaker",
        default="Serena",
        choices=SPEAKERS,
        help="Preset speaker voice for custom_voice mode",
    )
    parser.add_argument(
        "--voice-description",
        help="Natural-language voice description for voice_design mode",
    )
    parser.add_argument(
        "--output",
        default="bender-es-api.wav",
        help="Output WAV filename",
    )
    parser.add_argument(
        "--language",
        default="Spanish",
        choices=LANGUAGES,
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
    parser.add_argument(
        "--model",
        default=DEFAULT_MODEL,
        help="Replicate model ref",
    )
    parser.add_argument(
        "--replicate-base-url",
        default=os.environ.get("REPLICATE_BASE_URL", DEFAULT_REPLICATE_BASE_URL),
        help="Replicate-compatible API base URL",
    )
    parser.add_argument(
        "--poll-interval",
        type=float,
        default=DEFAULT_POLL_INTERVAL,
        help="Seconds to wait between prediction status checks if Prefer: wait returns early",
    )
    args = parser.parse_args()

    if not 0.1 <= args.poll_interval <= 60:
        raise ValueError("--poll-interval must be between 0.1 and 60")

    api_token = os.environ.get("HACK_CLUB_AI_KEY") or os.environ.get("REPLICATE_API_TOKEN")
    if not api_token:
        raise RuntimeError("HACK_CLUB_AI_KEY is not set. Add it to .env or export it in your shell.")

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    model_input = {
        "mode": args.mode,
        "text": args.text,
        "language": args.language,
    }
    if args.mode == "custom_voice":
        model_input["speaker"] = args.speaker

    if args.mode == "voice_design":
        if not args.voice_description:
            raise RuntimeError("--voice-description is required for voice_design mode.")
        model_input["voice_description"] = args.voice_description

    if args.mode == "voice_clone":
        ref_audio_value = args.reference_audio or args.ref_audio_flag or args.ref_audio
        if not ref_audio_value:
            raise RuntimeError("Reference audio is required for voice_clone mode. Pass it positionally, with --ref-audio, or with --reference-audio.")

        reference_audio, local_ref_audio = audio_input(ref_audio_value)
        ref_text_path = Path(args.reference_text_file) if args.reference_text_file else None
        reference_text = args.reference_text or load_reference_text(local_ref_audio, ref_text_path)

        model_input["reference_audio"] = reference_audio
        if reference_text:
            model_input["reference_text"] = reference_text

    if args.style_instruction:
        model_input["style_instruction"] = args.style_instruction

    base_url = args.replicate_base_url.rstrip("/")
    prediction = create_prediction(api_token, base_url, args.model, model_input, args.stream)
    if prediction.get("status") != "succeeded":
        prediction = wait_for_prediction(api_token, base_url, prediction["id"], args.poll_interval)

    output_url = get_output_url(prediction.get("output"))

    print(output_url)
    download_output(output_url, output_path, args.stream)

    print(f"Saved to {output_path}")


if __name__ == "__main__":
    main()
