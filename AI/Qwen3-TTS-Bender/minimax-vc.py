import argparse
import base64
import mimetypes
import os
import time
from pathlib import Path

import requests


DEFAULT_REPLICATE_BASE_URL = "https://ai.hackclub.com/proxy/v1/replicate"
DEFAULT_MODEL = "minimax/voice-cloning"
DEFAULT_MINIMAX_MODEL = "speech-02-turbo"
DEFAULT_ACCURACY = 0.7
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


def file_to_data_uri(file_path: Path) -> str:
    mime_type, _ = mimetypes.guess_type(file_path.name)
    if not mime_type:
        mime_type = "application/octet-stream"

    encoded = base64.b64encode(file_path.read_bytes()).decode("ascii")
    return f"data:{mime_type};base64,{encoded}"


def create_prediction(api_token: str, model_input: dict) -> dict:
    response = requests.post(
        f"{DEFAULT_REPLICATE_BASE_URL}/models/{DEFAULT_MODEL}/predictions",
        headers={
            "Authorization": f"Bearer {api_token}",
            "Content-Type": "application/json",
        },
        json={"input": model_input},
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


def download_file(file_url: str, output_path: Path) -> None:
    response = requests.get(file_url, timeout=120)
    response.raise_for_status()
    output_path.write_bytes(response.content)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Clone a voice with Minimax voice-cloning via the Hack Club Replicate proxy"
    )
    parser.add_argument(
        "voice_file",
        help="Reference audio path. Must be MP3, M4A, or WAV, 10s to 5min, under 20MB",
    )
    parser.add_argument(
        "--model",
        default=DEFAULT_MINIMAX_MODEL,
        choices=["speech-2.6-turbo", "speech-2.6-hd", "speech-02-turbo", "speech-02-hd"],
        help="Minimax TTS model family to bind the cloned voice to",
    )
    parser.add_argument(
        "--accuracy",
        type=float,
        default=DEFAULT_ACCURACY,
        help="Text validation accuracy threshold from 0 to 1",
    )
    parser.add_argument(
        "--need-noise-reduction",
        action="store_true",
        help="Enable noise reduction for the reference audio",
    )
    parser.add_argument(
        "--need-volume-normalization",
        action="store_true",
        help="Enable volume normalization for the reference audio",
    )
    parser.add_argument(
        "--preview-output",
        help="Optional path to save the preview audio returned by Minimax",
    )
    args = parser.parse_args()

    if not 0 <= args.accuracy <= 1:
        raise ValueError("--accuracy must be between 0 and 1")

    load_dotenv(Path(__file__).with_name(".env"))
    api_token = os.environ.get("REPLICATE_API_TOKEN")
    if not api_token:
        raise RuntimeError("REPLICATE_API_TOKEN is not set. Add it to .env or export it in your shell.")

    voice_file = Path(args.voice_file)
    if not voice_file.is_file():
        raise FileNotFoundError(f"Voice file not found: {voice_file}")

    model_input = {
        "voice_file": file_to_data_uri(voice_file),
        "model": args.model,
        "accuracy": args.accuracy,
        "need_noise_reduction": args.need_noise_reduction,
        "need_volume_normalization": args.need_volume_normalization,
    }

    prediction = create_prediction(api_token, model_input)
    prediction = wait_for_prediction(api_token, prediction["id"])

    output = prediction.get("output")
    if not isinstance(output, dict):
        raise RuntimeError("Prediction succeeded but no structured output was returned.")

    voice_id = output.get("voice_id")
    preview_url = output.get("preview")
    model_name = output.get("model")

    if not voice_id or not preview_url or not model_name:
        raise RuntimeError("Prediction output is missing one of: voice_id, preview, or model.")

    print(f"voice_id: {voice_id}")
    print(f"model: {model_name}")
    print(f"preview: {preview_url}")

    if args.preview_output:
        preview_output = Path(args.preview_output)
        preview_output.parent.mkdir(parents=True, exist_ok=True)
        download_file(preview_url, preview_output)
        print(f"Saved preview to {preview_output}")


if __name__ == "__main__":
    main()
