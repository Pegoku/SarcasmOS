import argparse
import base64
import json
import mimetypes
import os
import time
from pathlib import Path

import requests


DEFAULT_REPLICATE_BASE_URL = "https://ai.hackclub.com/proxy/v1/replicate"
DEFAULT_MODEL = "nvidia/parakeet-rnnt-1.1b:73ddbebaef172a47c8dfdd79381f110bfdc7691bcc7a4edde82f0a39e380ce50"
DEFAULT_OUTPUT = "parakeet-output.json"
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


def audio_input(audio: str) -> str:
    if audio.startswith(("http://", "https://", "data:")):
        return audio
    audio_path = Path(audio)
    if not audio_path.is_file():
        raise FileNotFoundError(f"Audio file not found: {audio_path}")
    return file_to_data_uri(audio_path)


def prediction_request(model_ref: str, model_input: dict) -> tuple[str, dict]:
    if ":" not in model_ref:
        return f"/models/{model_ref}/predictions", {"input": model_input}
    _, version = model_ref.split(":", 1)
    return "/predictions", {"version": version, "input": model_input}


def create_prediction(api_token: str, base_url: str, model_ref: str, model_input: dict) -> dict:
    path, body = prediction_request(model_ref, model_input)
    response = requests.post(
        f"{base_url}{path}",
        headers={
            "Authorization": f"Bearer {api_token}",
            "Content-Type": "application/json",
            "Prefer": "wait",
        },
        json=body,
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
            raise RuntimeError(prediction.get("error") or f"Prediction ended with status: {status}")
        time.sleep(poll_interval)


def write_output(output: object, output_path: Path) -> None:
    if isinstance(output, str) and output.startswith(("http://", "https://")):
        response = requests.get(output, timeout=120)
        response.raise_for_status()
        output_path.write_bytes(response.content)
        return
    if isinstance(output, str):
        output_path.write_text(output, encoding="utf-8")
        return
    output_path.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")


def main() -> None:
    load_dotenv(Path(__file__).with_name(".env"))

    parser = argparse.ArgumentParser(description="Transcribe audio with NVIDIA Parakeet RNNT 1.1B")
    parser.add_argument("--audio", required=True, help="Audio path, URL, or data URI.")
    parser.add_argument("--model", default=DEFAULT_MODEL, help="Replicate model ref.")
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    parser.add_argument("--replicate-base-url", default=os.environ.get("REPLICATE_BASE_URL", DEFAULT_REPLICATE_BASE_URL))
    parser.add_argument("--poll-interval", type=float, default=DEFAULT_POLL_INTERVAL)
    args = parser.parse_args()

    if not 0.1 <= args.poll_interval <= 60:
        raise ValueError("--poll-interval must be between 0.1 and 60")

    api_token = os.environ.get("HACK_CLUB_AI_KEY") or os.environ.get("REPLICATE_API_TOKEN")
    if not api_token:
        raise RuntimeError("HACK_CLUB_AI_KEY is not set. Add it to .env or export it in your shell.")

    model_input = {"audio_file": audio_input(args.audio)}
    base_url = args.replicate_base_url.rstrip("/")
    prediction = create_prediction(api_token, base_url, args.model, model_input)
    if prediction.get("status") != "succeeded":
        prediction = wait_for_prediction(api_token, base_url, prediction["id"], args.poll_interval)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    write_output(prediction.get("output"), output_path)
    print(f"saved: {output_path}")


if __name__ == "__main__":
    main()
