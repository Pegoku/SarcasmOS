import argparse
import base64
import datetime as dt
import json
import mimetypes
import os
import shutil
import subprocess
import time
import urllib.parse
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError
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
MAX_TOOL_ROUNDS = 6

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

TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "get_weather",
            "description": "Get current weather for a place. Use for weather, rain, temperature, umbrella, sky, or outdoor clothing questions.",
            "parameters": {
                "type": "object",
                "properties": {
                    "location": {
                        "type": "string",
                        "description": "City or place name, for example Madrid, Spain.",
                    }
                },
                "required": ["location"],
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "get_time",
            "description": "Get the current local time. Use for time, date, day, or clock questions.",
            "parameters": {
                "type": "object",
                "properties": {
                    "timezone": {
                        "type": "string",
                        "description": "IANA timezone such as Europe/Madrid. Defaults to Europe/Madrid.",
                    }
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "eye_look",
            "description": "Emulate robot eye movement in the CLI. This implements commands like eye.look.left, eye.look.right, eye.look.up, eye.look.down, and eye.look.center.",
            "parameters": {
                "type": "object",
                "properties": {
                    "direction": {
                        "type": "string",
                        "enum": ["left", "right", "up", "down", "center"],
                    }
                },
                "required": ["direction"],
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "set_expression",
            "description": "Emulate the robot's eyes and teeth/mouth in the CLI for visual expressions.",
            "parameters": {
                "type": "object",
                "properties": {
                    "expression": {
                        "type": "string",
                        "enum": [
                            "neutral", "sarcastic", "angry", "happy_fake", "suspicious",
                            "tired", "asleep", "surprised", "bored", "dramatic",
                            "watch", "party", "error", "battery_low", "sunny",
                            "rainy", "cloudy", "stormy", "snowy",
                        ],
                    }
                },
                "required": ["expression"],
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "robot_status",
            "description": "Return available robot status. Use for battery, Wi-Fi, sensors, eyes, mouth, microphone, or internal temperature questions.",
            "parameters": {
                "type": "object",
                "properties": {},
                "additionalProperties": False,
            },
        },
    },
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


def render_face(state: str, detail: str = "") -> None:
    faces = {
        "idle": ("o o", "-----"),
        "thinking": ("@ @", "#####"),
        "tool": ("> <", "====="),
        "speaking": ("^ ^", "#####"),
        "left": ("<o <o", "-----"),
        "right": ("o> o>", "-----"),
        "up": ("^ ^", "-----"),
        "down": ("v v", "-----"),
        "center": ("o o", "-----"),
        "neutral": ("o o", "-----"),
        "sarcastic": ("- o", "~~---"),
        "angry": ("> <", "#####"),
        "happy_fake": ("^ ^", "====="),
        "suspicious": ("- -", "--_--"),
        "tired": ("u u", "....."),
        "asleep": ("- -", "_____"),
        "surprised": ("O O", "  O  "),
        "bored": ("- -", "-----"),
        "dramatic": ("* *", "#####"),
        "watch": ("0 0", "====="),
        "party": ("^ *", "\\___/"),
        "error": ("X X", "!!!!!"),
        "battery_low": ("_ _", ".._.."),
        "sunny": ("\\o/ \\o/", "\\___/"),
        "rainy": ("; ;", "~~~~~"),
        "cloudy": ("- -", "(___)"),
        "stormy": ("! !", "ZZZZZ"),
        "snowy": ("* *", "....."),
    }
    eyes, teeth = faces.get(state, faces["idle"])
    suffix = f" {detail}" if detail else ""
    print(f"[face:{state}] eyes[{eyes}] teeth[{teeth}]{suffix}")


def weather_expression(weather_text: str) -> str:
    text = weather_text.lower()
    if any(word in text for word in ("sun", "clear", "soleado", "despejado")):
        return "sunny"
    if any(word in text for word in ("rain", "drizzle", "shower", "lluv")):
        return "rainy"
    if any(word in text for word in ("thunder", "storm", "tormenta")):
        return "stormy"
    if any(word in text for word in ("snow", "sleet", "nieve")):
        return "snowy"
    if any(word in text for word in ("cloud", "overcast", "nube", "cubierto")):
        return "cloudy"
    return "neutral"


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


def get_weather(location: str) -> dict:
    encoded_location = urllib.parse.quote(location)
    response = requests.get(f"https://wttr.in/{encoded_location}", params={"format": "j1"}, timeout=30)
    response.raise_for_status()
    data = response.json()

    current = data["current_condition"][0]
    area = data.get("nearest_area", [{}])[0]
    area_name = area.get("areaName", [{"value": location}])[0]["value"]
    country = area.get("country", [{"value": ""}])[0]["value"]
    condition = current.get("weatherDesc", [{"value": "unknown"}])[0]["value"]
    expression = weather_expression(condition)
    render_face(expression, condition)

    return {
        "location": ", ".join(part for part in (area_name, country) if part),
        "condition": condition,
        "temperature_c": current.get("temp_C"),
        "feels_like_c": current.get("FeelsLikeC"),
        "humidity_percent": current.get("humidity"),
        "wind_kmph": current.get("windspeedKmph"),
        "precipitation_mm": current.get("precipMM"),
        "expression": expression,
    }


def get_time(timezone: str | None = None) -> dict:
    timezone = timezone or "Europe/Madrid"
    try:
        now = dt.datetime.now(ZoneInfo(timezone))
    except ZoneInfoNotFoundError:
        timezone = "Europe/Madrid"
        now = dt.datetime.now(ZoneInfo(timezone))

    return {
        "timezone": timezone,
        "iso": now.isoformat(timespec="seconds"),
        "date": now.strftime("%Y-%m-%d"),
        "time": now.strftime("%H:%M:%S"),
        "weekday": now.strftime("%A"),
    }


def robot_status() -> dict:
    return {
        "battery": "unknown; no real battery sensor is connected",
        "wifi": "unknown; no real Wi-Fi telemetry is connected",
        "sensors": "none connected to this CLI script",
        "eyes": "CLI emulation active",
        "mouth": "CLI teeth emulation active",
        "microphone": "input audio file only; no live microphone sensor",
        "internal_temperature": "unknown; no internal temperature sensor is connected",
    }


def run_tool_call(name: str, arguments: dict) -> object:
    render_face("tool", name)

    if name == "get_weather":
        return get_weather(arguments["location"])

    if name == "get_time":
        return get_time(arguments.get("timezone"))

    if name == "eye_look":
        direction = arguments["direction"]
        render_face(direction, f"eye.look.{direction}")
        return {"ok": True, "command": f"eye.look.{direction}"}

    if name == "set_expression":
        expression = arguments["expression"]
        render_face(expression)
        return {"ok": True, "expression": expression}

    if name == "robot_status":
        render_face("watch", "checking status")
        return robot_status()

    return {"ok": False, "error": f"Unknown tool: {name}"}


def parse_tool_arguments(raw_arguments: str | dict | None) -> dict:
    if raw_arguments is None:
        return {}
    if isinstance(raw_arguments, dict):
        return raw_arguments
    if not raw_arguments:
        return {}
    return json.loads(raw_arguments)


def chat_completion(api_token: str, base_url: str, payload: dict) -> dict:
    response = requests.post(
        f"{base_url.rstrip('/')}/chat/completions",
        headers={
            "Authorization": f"Bearer {api_token}",
            "Content-Type": "application/json",
        },
        json=payload,
        timeout=300,
    )
    response.raise_for_status()
    return response.json()


def generate_answer(api_token: str, base_url: str, model: str, transcript: str, sysprompt: str) -> str:
    tool_prompt = (
        sysprompt
        + "\n\nTOOLS AVAILABLE:\n"
        + "- Use get_weather for real weather instead of guessing.\n"
        + "- Use get_time for current time/date instead of guessing.\n"
        + "- Use eye_look to implement eye.look.left/right/up/down/center in the CLI.\n"
        + "- Use set_expression for robot face states, including weather states like sunny/rainy/cloudy/stormy/snowy.\n"
        + "- Use robot_status for robot state questions; do not invent unavailable sensor readings.\n"
        + "When waiting, thinking, or calling tools, the CLI face will emulate eyes and teeth."
    )
    messages = [
        {"role": "system", "content": tool_prompt},
        {"role": "user", "content": transcript},
    ]
    final_expression = "speaking"

    for _ in range(MAX_TOOL_ROUNDS):
        render_face("thinking", "generating/tool planning")
        data = chat_completion(
            api_token,
            base_url,
            {
                "model": model,
                "messages": messages,
                "tools": TOOLS,
                "tool_choice": "auto",
            },
        )
        message = data["choices"][0]["message"]
        tool_calls = message.get("tool_calls") or []

        if not tool_calls:
            answer = message.get("content")
            if not answer or not answer.strip():
                raise RuntimeError("LLM returned an empty answer.")
            render_face(final_expression, "final answer ready")
            return answer.strip()

        messages.append(
            {
                "role": "assistant",
                "content": message.get("content"),
                "tool_calls": tool_calls,
            }
        )
        for tool_call in tool_calls:
            function = tool_call.get("function", {})
            name = function.get("name", "")
            try:
                arguments = parse_tool_arguments(function.get("arguments"))
                result = run_tool_call(name, arguments)
            except Exception as error:
                render_face("error", name)
                result = {"ok": False, "error": str(error)}
            if isinstance(result, dict) and result.get("expression"):
                final_expression = str(result["expression"])
            messages.append(
                {
                    "role": "tool",
                    "tool_call_id": tool_call["id"],
                    "name": name,
                    "content": json.dumps(result, ensure_ascii=False),
                }
            )

    raise RuntimeError("LLM exceeded maximum tool-call rounds.")


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


def format_duration(seconds: float) -> str:
    minutes, remaining_seconds = divmod(seconds, 60)
    hours, minutes = divmod(int(minutes), 60)
    if hours:
        return f"{hours:d}h {minutes:02d}m {remaining_seconds:05.2f}s"
    if minutes:
        return f"{minutes:d}m {remaining_seconds:05.2f}s"
    return f"{remaining_seconds:.2f}s"


def print_timing(label: str, started_at: float, previous_at: float) -> float:
    now = time.perf_counter()
    print(
        f"{label}: {format_duration(now - previous_at)} "
        f"(total {format_duration(now - started_at)})"
    )
    return now


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
    started_at = time.perf_counter()
    last_timing_at = started_at

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

    render_face("thinking", "listening/transcribing")
    print("transcribing...")
    transcript = extract_transcript(
        run_prediction(replicate_token, replicate_base_url, args.stt_model, stt_input, args.poll_interval)
    )
    last_timing_at = print_timing("transcription time", started_at, last_timing_at)

    print("generating answer...")
    answer = generate_answer(openrouter_token, args.openrouter_base_url, args.llm_model, transcript, sysprompt)
    if len(answer) > 10000:
        raise ValueError("LLM answer is longer than the 10,000 character TTS limit.")
    last_timing_at = print_timing("llm time", started_at, last_timing_at)

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

    render_face("speaking", "synthesizing audio")
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
    last_timing_at = print_timing("tts time", started_at, last_timing_at)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    render_face("speaking", "downloading/playback")
    download_file(output_url, output_path, args.stream)
    print_timing("download/playback time", started_at, last_timing_at)

    if args.print_text:
        print(f"transcript: {transcript}")
        print(f"answer: {answer}")
    print(f"audio_url: {output_url}")
    print(f"saved: {output_path}")


if __name__ == "__main__":
    main()
