import base64
import datetime as dt
import json
import mimetypes
import os
import shutil
import subprocess
import time
import urllib.parse
import uuid
from dataclasses import dataclass
from pathlib import Path
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

import requests


DEFAULT_OPENROUTER_BASE_URL = "https://ai.hackclub.com/proxy/v1"
DEFAULT_REPLICATE_BASE_URL = "https://ai.hackclub.com/proxy/v1/replicate"
DEFAULT_STT_MODEL = (
    "vaibhavs10/incredibly-fast-whisper:"
    "3ab86df6c8f54c11309d4d1f930ac292bad43ace52d10c80d87eb258b3c9f79c"
)
DEFAULT_LLM_MODEL = "~anthropic/claude-sonnet-latest"
DEFAULT_TTS_MODEL = "minimax/speech-02-turbo"
DEFAULT_POLL_INTERVAL = 1.0
MAX_TOOL_ROUNDS = 6

SUPPORTED_INPUT_EXTENSIONS = {
    ".wav",
    ".mp3",
    ".flac",
    ".m4a",
    ".ogg",
    ".opus",
    ".aac",
}

SUPPORTED_OUTPUT_EXTENSIONS = {
    "wav",
    "mp3",
    "flac",
    "m4a",
    "ogg",
    "opus",
    "aac",
}

_CACHED_FFMPEG: str | None = None

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


@dataclass
class BenderConfig:
    openrouter_token: str
    replicate_token: str
    voice_id: str
    openrouter_base_url: str
    replicate_base_url: str
    stt_model: str
    llm_model: str
    tts_model: str
    language: str = "spanish"
    task: str = "transcribe"
    timestamp: str = "chunk"
    batch_size: int = 24
    poll_interval: float = 1.0
    speed: float = 0.9
    volume: float = 1.0
    pitch: int = 0
    audio_format: str = "wav"
    language_boost: str = "Spanish"
    sysprompt: str = "Answer the user's transcribed audio clearly and briefly in the same language."
    hf_token: str | None = None
    base_dir: Path | None = None
    uploads_dir: Path | None = None
    outputs_dir: Path | None = None

    @classmethod
    def from_env(cls) -> "BenderConfig":
        base_dir = Path(__file__).resolve().parent
        env_candidates = [
            base_dir / ".env",
            base_dir.parent / ".env",
            base_dir.parent / "AI" / "Workflow" / ".env",
        ]
        for env_path in env_candidates:
            load_dotenv(env_path)

        api_token = os.environ.get("HACK_CLUB_AI_KEY")
        replicate_token = api_token or os.environ.get("REPLICATE_API_TOKEN")
        openrouter_token = api_token or os.environ.get("OPENROUTER_API_TOKEN")
        if not replicate_token:
            raise RuntimeError("HACK_CLUB_AI_KEY or REPLICATE_API_TOKEN is required.")
        if not openrouter_token:
            raise RuntimeError("HACK_CLUB_AI_KEY or OPENROUTER_API_TOKEN is required.")

        voice_id = os.environ.get("MINIMAX_VOICE_ID")
        if not voice_id:
            raise RuntimeError("MINIMAX_VOICE_ID is required.")

        replicate_base_url = normalize_replicate_base_url(
            os.environ.get("REPLICATE_BASE_URL", DEFAULT_REPLICATE_BASE_URL)
        )
        openrouter_base_url = os.environ.get("OPENROUTER_BASE_URL", DEFAULT_OPENROUTER_BASE_URL)

        sysprompt = read_sysprompt(None, str(base_dir / "sysprompt.txt"))

        return cls(
            openrouter_token=openrouter_token,
            replicate_token=replicate_token,
            voice_id=voice_id,
            openrouter_base_url=openrouter_base_url,
            replicate_base_url=replicate_base_url,
            stt_model=os.environ.get("STT_MODEL", DEFAULT_STT_MODEL),
            llm_model=os.environ.get("LLM_MODEL", DEFAULT_LLM_MODEL),
            tts_model=os.environ.get("TTS_MODEL", DEFAULT_TTS_MODEL),
            language=os.environ.get("WHISPER_LANGUAGE", "spanish"),
            task=os.environ.get("WHISPER_TASK", "transcribe"),
            timestamp=os.environ.get("WHISPER_TIMESTAMP", "chunk"),
            batch_size=int(os.environ.get("WHISPER_BATCH_SIZE", "24")),
            poll_interval=float(os.environ.get("POLL_INTERVAL", str(DEFAULT_POLL_INTERVAL))),
            speed=float(os.environ.get("MINIMAX_SPEED", "0.9")),
            volume=float(os.environ.get("MINIMAX_VOLUME", "1.0")),
            pitch=int(os.environ.get("MINIMAX_PITCH", "0")),
            audio_format=os.environ.get("MINIMAX_AUDIO_FORMAT", "wav"),
            language_boost=os.environ.get("MINIMAX_LANGUAGE_BOOST", "Spanish"),
            sysprompt=sysprompt,
            hf_token=os.environ.get("HF_TOKEN"),
            base_dir=base_dir,
            uploads_dir=base_dir / "uploads",
            outputs_dir=base_dir / "outputs",
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
        current = os.environ.get(key)
        if current is None or current == "":
            os.environ[key] = value


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
        "cloudy": ("- -", "(____)"),
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

    raise RuntimeError(
        "Could not extract transcript from Whisper output: "
        f"{json.dumps(output, ensure_ascii=False)[:500]}"
    )


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
        "sensors": "none connected to this script",
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


def detect_audio_extension(file_url: str, fallback: str) -> str:
    parsed = urllib.parse.urlparse(file_url)
    suffix = Path(parsed.path).suffix.lower().lstrip(".")
    if suffix in SUPPORTED_OUTPUT_EXTENSIONS:
        return suffix
    return fallback


def resolve_stream_player(audio_path: Path) -> list[str]:
    if shutil.which("ffplay"):
        return ["ffplay", "-hide_banner", "-loglevel", "error", "-autoexit", "-nodisp", str(audio_path)]

    if shutil.which("vlc"):
        return ["vlc", "--play-and-exit", str(audio_path)]

    raise RuntimeError("--stream requires ffplay or vlc to be installed.")


def download_file(file_url: str, output_path: Path, stream_audio: bool) -> None:
    response = requests.get(file_url, stream=True, timeout=120)
    response.raise_for_status()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("wb") as file:
        for chunk in response.iter_content(chunk_size=65536):
            if chunk:
                file.write(chunk)

    if stream_audio:
        subprocess.run(resolve_stream_player(output_path), check=True)


def read_sysprompt(sysprompt: str | None, sysprompt_file: str | None) -> str:
    if sysprompt:
        return sysprompt
    if sysprompt_file:
        return Path(sysprompt_file).read_text(encoding="utf-8")
    return "Answer the user's transcribed audio clearly and briefly in the same language."


def resolve_ffmpeg() -> str | None:
    global _CACHED_FFMPEG
    if _CACHED_FFMPEG is not None:
        return _CACHED_FFMPEG

    env_path = os.environ.get("FFMPEG_PATH")
    if env_path and Path(env_path).is_file():
        _CACHED_FFMPEG = env_path
        return _CACHED_FFMPEG

    which = shutil.which("ffmpeg")
    if which:
        _CACHED_FFMPEG = which
        return _CACHED_FFMPEG

    local_appdata = Path(os.environ.get("LOCALAPPDATA", ""))
    program_files = Path(os.environ.get("ProgramFiles", "C:\\Program Files"))
    candidates = [
        program_files / "ffmpeg" / "bin" / "ffmpeg.exe",
        program_files / "FFmpeg" / "bin" / "ffmpeg.exe",
        program_files / "Gyan" / "ffmpeg" / "bin" / "ffmpeg.exe",
        program_files / "Gyan" / "FFmpeg" / "bin" / "ffmpeg.exe",
        local_appdata / "Programs" / "ffmpeg" / "bin" / "ffmpeg.exe",
        local_appdata / "Programs" / "FFmpeg" / "bin" / "ffmpeg.exe",
    ]
    for candidate in candidates:
        if candidate.is_file():
            _CACHED_FFMPEG = str(candidate)
            return _CACHED_FFMPEG

    capcut_root = local_appdata / "CapCut" / "Apps"
    if capcut_root.is_dir():
        matches = sorted(capcut_root.glob("*/ffmpeg.exe"))
        if matches:
            _CACHED_FFMPEG = str(matches[-1])
            return _CACHED_FFMPEG

    _CACHED_FFMPEG = None
    return None


def ensure_supported_audio(input_path: Path) -> Path:
    if input_path.suffix.lower() in SUPPORTED_INPUT_EXTENSIONS:
        return input_path

    output_path = input_path.with_name(f"{input_path.stem}-converted.wav")
    ffmpeg_path = resolve_ffmpeg()
    if not ffmpeg_path:
        raise RuntimeError("ffmpeg is required to convert audio files to wav.")

    command = [
        ffmpeg_path,
        "-y",
        "-i",
        str(input_path),
        "-ac",
        "1",
        "-ar",
        "16000",
        str(output_path),
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        error_text = result.stderr.strip() or "ffmpeg conversion failed"
        raise RuntimeError(error_text)
    return output_path


def transcribe_audio(audio_path: str, config: BenderConfig) -> str:
    source_path = ensure_supported_audio(Path(audio_path))
    stt_input = {
        "audio": audio_input(str(source_path)),
        "task": config.task,
        "language": config.language,
        "timestamp": config.timestamp,
        "batch_size": config.batch_size,
        "diarise_audio": False,
    }
    if config.hf_token:
        stt_input["hf_token"] = config.hf_token

    render_face("thinking", "listening/transcribing")
    output = run_prediction(
        config.replicate_token,
        config.replicate_base_url,
        config.stt_model,
        stt_input,
        config.poll_interval,
    )
    return extract_transcript(output)


def generate_text_answer(transcript: str, config: BenderConfig) -> str:
    render_face("thinking", "generating answer")
    answer = generate_answer(
        config.openrouter_token,
        config.openrouter_base_url,
        config.llm_model,
        transcript,
        config.sysprompt,
    )
    if len(answer) > 10000:
        raise RuntimeError("LLM answer is longer than the 10,000 character TTS limit.")
    return answer


def synthesize_speech(text: str, config: BenderConfig) -> Path:
    tts_input = {
        "text": text,
        "voice_id": config.voice_id,
        "speed": config.speed,
        "volume": config.volume,
        "pitch": config.pitch,
        "emotion": "auto",
        "english_normalization": False,
        "sample_rate": 32000,
        "bitrate": 128000,
        "audio_format": config.audio_format,
        "channel": "mono",
        "subtitle_enable": False,
        "language_boost": config.language_boost,
    }

    render_face("speaking", "synthesizing audio")
    tts_output = run_prediction(
        config.replicate_token,
        config.replicate_base_url,
        config.tts_model,
        tts_input,
        config.poll_interval,
        extra_body={"stream": False},
    )
    output_url = get_output_url(tts_output)
    extension = detect_audio_extension(output_url, config.audio_format)

    output_dir = config.outputs_dir or Path(__file__).resolve().parent / "outputs"
    output_dir.mkdir(parents=True, exist_ok=True)
    filename = f"answer-{uuid.uuid4().hex}.{extension}"
    output_path = output_dir / filename
    download_file(output_url, output_path, stream_audio=False)
    if output_path.stat().st_size < 1024:
        raise RuntimeError("TTS returned an empty audio file.")
    return output_path


def process_audio_file(audio_path: str, options: dict | None = None) -> dict:
    config = BenderConfig.from_env()
    transcript = transcribe_audio(audio_path, config)
    answer = generate_text_answer(transcript, config)
    output_path = synthesize_speech(answer, config)
    return {
        "transcript": transcript,
        "answer": answer,
        "audio_path": str(output_path),
    }


def process_text_message(message: str, options: dict | None = None) -> dict:
    config = BenderConfig.from_env()
    answer = generate_text_answer(message, config)
    output_path = synthesize_speech(answer, config)
    return {
        "answer": answer,
        "audio_path": str(output_path),
    }
