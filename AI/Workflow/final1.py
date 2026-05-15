import argparse
import base64
import json
import mimetypes
import os
import shutil
import subprocess
import time
from pathlib import Path

import requests


DEFAULT_OPENROUTER_BASE_URL = "https://ai.hackclub.com/proxy/v1"
DEFAULT_REPLICATE_BASE_URL = "https://ai.hackclub.com/proxy/v1/replicate"
DEFAULT_STT_MODEL = (
    "vaibhavs10/incredibly-fast-whisper:"
    "3ab86df6c8f54c11309d4d1f930ac292bad43ace52d10c80d87eb258b3c9f79c"
)
DEFAULT_LLM_MODEL = "~anthropic/claude-sonnet-latest"
DEFAULT_TTS_MODEL = "minimax/speech-02-turbo"
DEFAULT_OUTPUT = "answer.wav"
DEFAULT_POLL_INTERVAL = 1.0

LANGUAGES = [
    "None", "afrikaans", "albanian", "amharic", "arabic", "armenian", "assamese", "azerbaijani",
    "bashkir", "basque", "belarusian", "bengali", "bosnian", "breton", "bulgarian", "cantonese",
    "catalan", "chinese", "croatian", "czech", "danish", "dutch", "english", "estonian", "faroese",
    "finnish", "french", "galician", "georgian", "german", "greek", "gujarati", "haitian creole",
    "hausa", "hawaiian", "hebrew", "hindi", "hungarian", "icelandic", "indonesian", "italian",
    "japanese", "javanese", "kannada", "kazakh", "khmer", "korean", "lao", "latin", "latvian",
    "lingala", "lithuanian", "luxembourgish", "macedonian", "malagasy", "malay", "malayalam",
    "maltese", "maori", "marathi", "mongolian", "myanmar", "nepali", "norwegian", "nynorsk",
    "occitan", "pashto", "persian", "polish", "portuguese", "punjabi", "romanian", "russian",
    "sanskrit", "serbian", "shona", "sindhi", "sinhala", "slovak", "slovenian", "somali",
    "spanish", "sundanese", "swahili", "swedish", "tagalog", "tajik", "tamil", "tatar", "telugu",
    "thai", "tibetan", "turkish", "turkmen", "ukrainian", "urdu", "uzbek", "vietnamese", "welsh",
    "yiddish", "yoruba",
]

LANGUAGE_BOOSTS = [
    "None", "Automatic", "Chinese", "Chinese,Yue", "Cantonese", "English", "Arabic", "Russian",
    "Spanish", "French", "Portuguese", "German", "Turkish", "Dutch", "Ukrainian", "Vietnamese",
    "Indonesian", "Japanese", "Italian", "Korean", "Thai", "Polish", "Romanian", "Greek",
    "Czech", "Finnish", "Hindi", "Bulgarian", "Danish", "Hebrew", "Malay", "Persian", "Slovak",
    "Swedish", "Croatian", "Filipino", "Hungarian", "Norwegian", "Slovenian", "Catalan",
    "Nynorsk", "Tamil", "Afrikaans",
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


def normalize_replicate_base_url(base_url: str) -> str:
    base_url = base_url.rstrip("/")
    if base_url == DEFAULT_OPENROUTER_BASE_URL:
        return f"{base_url}/replicate"
    return base_url


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


def prediction_request(model_ref: str, model_input: dict, extra_body: dict | None = None) -> tuple[str, dict]:
    body = {"input": model_input}
    if extra_body:
        body.update(extra_body)

    if ":" not in model_ref:
        return f"/models/{model_ref}/predictions", body

    _, version = model_ref.split(":", 1)
    body["version"] = version
    return "/predictions", body


def create_prediction(
    api_token: str,
    base_url: str,
    model_ref: str,
    model_input: dict,
    extra_body: dict | None = None,
) -> dict:
    path, body = prediction_request(model_ref, model_input, extra_body)
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


def run_prediction(
    api_token: str,
    base_url: str,
    model_ref: str,
    model_input: dict,
    poll_interval: float,
    extra_body: dict | None = None,
) -> object:
    prediction = create_prediction(api_token, base_url, model_ref, model_input, extra_body)
    if prediction.get("status") != "succeeded":
        prediction = wait_for_prediction(api_token, base_url, prediction["id"], poll_interval)
    return prediction.get("output")


def extract_transcript(output: object) -> str:
    if isinstance(output, str):
        return output.strip()

    if isinstance(output, dict):
        for key in ("text", "transcription", "transcript", "output"):
            value = output.get(key)
            if isinstance(value, str) and value.strip():
                return value.strip()

        chunks = output.get("chunks")
        if isinstance(chunks, list):
            text = " ".join(
                chunk.get("text", "").strip()
                for chunk in chunks
                if isinstance(chunk, dict) and chunk.get("text")
            )
            if text.strip():
                return text.strip()

    raise RuntimeError(f"Could not extract transcript from Whisper output: {json.dumps(output, ensure_ascii=False)[:500]}")


def generate_answer(api_token: str, base_url: str, model: str, transcript: str, sysprompt: str) -> str:
    response = requests.post(
        f"{base_url.rstrip('/')}/chat/completions",
        headers={
            "Authorization": f"Bearer {api_token}",
            "Content-Type": "application/json",
        },
        json={
            "model": model,
            "messages": [
                {"role": "system", "content": sysprompt},
                {"role": "user", "content": transcript},
            ],
        },
        timeout=300,
    )
    response.raise_for_status()
    data = response.json()
    answer = data["choices"][0]["message"]["content"]
    if not answer or not answer.strip():
        raise RuntimeError("LLM returned an empty answer.")
    return answer.strip()


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

    raise RuntimeError("TTS prediction succeeded but no audio URL was returned.")


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
            if chunk:
                file.write(chunk)

    if stream_audio:
        subprocess.run(resolve_stream_player(output_path), check=True)


def validate_range(name: str, value: float, minimum: float, maximum: float) -> None:
    if not minimum <= value <= maximum:
        raise ValueError(f"--{name} must be between {minimum:g} and {maximum:g}")


def read_sysprompt(sysprompt: str | None, sysprompt_file: str | None) -> str:
    if sysprompt:
        return sysprompt
    if sysprompt_file:
        return Path(sysprompt_file).read_text(encoding="utf-8")
    return "Answer the user's transcribed audio clearly and briefly in the same language."


def main() -> None:
    load_dotenv(Path(__file__).with_name(".env"))

    parser = argparse.ArgumentParser(description="Turn input audio into an LLM-generated audio answer.")
    parser.add_argument("--audio", required=True, help="Input audio path, URL, or data URI.")
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="Path to save the final answer audio.")

    parser.add_argument("--stt-model", default=DEFAULT_STT_MODEL, help="Replicate Whisper model ref.")
    parser.add_argument("--language", default="spanish", choices=LANGUAGES, help="Whisper language hint.")
    parser.add_argument("--task", default="transcribe", choices=["transcribe", "translate"])
    parser.add_argument("--timestamp", default="chunk", choices=["chunk", "word"])
    parser.add_argument("--batch-size", type=int, default=24)
    parser.add_argument("--diarise-audio", action="store_true")
    parser.add_argument("--hf-token", default=os.environ.get("HF_TOKEN"))

    parser.add_argument("--llm-model", default=DEFAULT_LLM_MODEL, help="OpenRouter model.")
    parser.add_argument("--sysprompt", help="System prompt for the LLM.")
    parser.add_argument("--sysprompt-file", default="sysprompt.txt", help="System prompt file for the LLM.")

    parser.add_argument("--tts-model", default=DEFAULT_TTS_MODEL, help="Replicate Minimax speech model ref.")
    parser.add_argument("--voice-id", default=os.environ.get("MINIMAX_VOICE_ID"), help="Minimax voice_id.")
    parser.add_argument("--speed", type=float, default=0.9, help="Speech speed from 0.5 to 2.0.")
    parser.add_argument("--volume", type=float, default=1.0, help="Relative loudness from 0 to 10.")
    parser.add_argument("--pitch", type=int, default=0, help="Semitone offset from -12 to 12.")
    parser.add_argument("--audio-format", default="wav", choices=["mp3", "wav", "flac", "pcm"])
    parser.add_argument("--language-boost", default="Spanish", choices=LANGUAGE_BOOSTS)

    parser.add_argument("--openrouter-base-url", default=os.environ.get("OPENROUTER_BASE_URL", DEFAULT_OPENROUTER_BASE_URL))
    parser.add_argument("--replicate-base-url", default=os.environ.get("REPLICATE_BASE_URL", DEFAULT_REPLICATE_BASE_URL))
    parser.add_argument("--poll-interval", type=float, default=DEFAULT_POLL_INTERVAL)
    parser.add_argument("--stream", action="store_true", help="Play the generated answer audio after downloading it.")
    parser.add_argument("--print-text", action="store_true", help="Print transcript and LLM answer.")
    args = parser.parse_args()

    if args.batch_size < 1:
        raise ValueError("--batch-size must be at least 1")
    validate_range("poll-interval", args.poll_interval, 0.1, 60)
    validate_range("speed", args.speed, 0.5, 2.0)
    validate_range("volume", args.volume, 0, 10)
    validate_range("pitch", args.pitch, -12, 12)

    api_token = os.environ.get("HACK_CLUB_AI_KEY")
    replicate_token = api_token or os.environ.get("REPLICATE_API_TOKEN")
    openrouter_token = api_token or os.environ.get("OPENROUTER_API_TOKEN")
    if not replicate_token:
        raise RuntimeError("HACK_CLUB_AI_KEY or REPLICATE_API_TOKEN is required.")
    if not openrouter_token:
        raise RuntimeError("HACK_CLUB_AI_KEY or OPENROUTER_API_TOKEN is required.")
    if not args.voice_id:
        raise RuntimeError("--voice-id is required, or set MINIMAX_VOICE_ID in .env.")

    replicate_base_url = normalize_replicate_base_url(args.replicate_base_url)
    sysprompt = read_sysprompt(args.sysprompt, args.sysprompt_file)

    stt_input = {
        "audio": audio_input(args.audio),
        "task": args.task,
        "language": args.language,
        "timestamp": args.timestamp,
        "batch_size": args.batch_size,
        "diarise_audio": args.diarise_audio,
    }
    if args.hf_token:
        stt_input["hf_token"] = args.hf_token

    print("transcribing...")
    transcript = extract_transcript(
        run_prediction(replicate_token, replicate_base_url, args.stt_model, stt_input, args.poll_interval)
    )

    print("generating answer...")
    answer = generate_answer(openrouter_token, args.openrouter_base_url, args.llm_model, transcript, sysprompt)
    if len(answer) > 10000:
        raise ValueError("LLM answer is longer than the 10,000 character TTS limit.")

    tts_input = {
        "text": answer,
        "voice_id": args.voice_id,
        "speed": args.speed,
        "volume": args.volume,
        "pitch": args.pitch,
        "emotion": "auto",
        "english_normalization": False,
        "sample_rate": 32000,
        "bitrate": 128000,
        "audio_format": args.audio_format,
        "channel": "mono",
        "subtitle_enable": False,
        "language_boost": args.language_boost,
    }

    print("synthesizing audio...")
    tts_output = run_prediction(
        replicate_token,
        replicate_base_url,
        args.tts_model,
        tts_input,
        args.poll_interval,
        extra_body={"stream": args.stream},
    )
    output_url = get_output_url(tts_output)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    download_file(output_url, output_path, args.stream)

    if args.print_text:
        print(f"transcript: {transcript}")
        print(f"answer: {answer}")
    print(f"audio_url: {output_url}")
    print(f"saved: {output_path}")


if __name__ == "__main__":
    main()
