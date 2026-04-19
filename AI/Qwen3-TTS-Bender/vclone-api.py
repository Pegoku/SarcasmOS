import argparse
import os
from pathlib import Path

import replicate


DEFAULT_REPLICATE_BASE_URL = "https://ai.hackclub.com/proxy/v1/replicate"
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


def load_reference_text(ref_audio: Path) -> str:
    ref_text_path = ref_audio.with_suffix(".txt")

    if not ref_text_path.is_file():
        raise FileNotFoundError(f"Reference transcript not found: {ref_text_path}")

    ref_text = ref_text_path.read_text(encoding="utf-8").strip()
    if not ref_text:
        raise ValueError(f"Reference transcript is empty: {ref_text_path}")

    return ref_text


def main():
    parser = argparse.ArgumentParser(
        description="Clone speech with Replicate's qwen/qwen3-tts API"
    )
    parser.add_argument(
        "ref_audio",
        help="Reference audio path. The transcript must exist as the same filename with .txt",
    )
    parser.add_argument(
        "--text",
        default=DEFAULT_TEXT,
        help="Text to synthesize",
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
    args = parser.parse_args()

    load_dotenv(Path(__file__).with_name(".env"))
    api_token = os.environ.get("REPLICATE_API_TOKEN")
    if not api_token:
        raise RuntimeError("REPLICATE_API_TOKEN is not set. Add it to .env or export it in your shell.")

    client = replicate.Client(
        api_token=api_token,
        base_url=DEFAULT_REPLICATE_BASE_URL,
    )

    ref_audio = Path(args.ref_audio)
    if not ref_audio.is_file():
        raise FileNotFoundError(f"Reference audio not found: {ref_audio}")

    ref_text = load_reference_text(ref_audio)
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

    with ref_audio.open("rb") as audio_file:
        model_input["reference_audio"] = audio_file
        output = client.run("qwen/qwen3-tts", input=model_input)

    print(output.url)

    with output_path.open("wb") as file:
        file.write(output.read())

    print(f"Saved to {output_path}")


if __name__ == "__main__":
    main()
