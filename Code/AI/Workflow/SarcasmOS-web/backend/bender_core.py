import base64
import datetime as dt
import json
import mimetypes
import os
import re
import shutil
import subprocess
import time
import urllib.parse
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Callable
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

import requests


DEFAULT_OPENROUTER_BASE_URL = "https://ai.hackclub.com/proxy/v1"
DEFAULT_REPLICATE_BASE_URL = "https://ai.hackclub.com/proxy/v1/replicate"
DEFAULT_FALLBACK_LLM_BASE_URL = "https://api.pioneer.ai/v1"
DEFAULT_FALLBACK_LLM_MODEL = "claude-sonnet-4-6"
DEFAULT_STT_MODEL = (
    "vaibhavs10/incredibly-fast-whisper:"
    "3ab86df6c8f54c11309d4d1f930ac292bad43ace52d10c80d87eb258b3c9f79c"
)
DEFAULT_LLM_MODEL = "~anthropic/claude-sonnet-latest"
DEFAULT_TTS_MODEL = "minimax/speech-2.8-turbo"
DEFAULT_POLL_INTERVAL = 1.0
MAX_TOOL_ROUNDS = 6
TTS_EXPRESSION_TAG_RE = re.compile(
    r"\((?:laughs|chuckle|coughs|clear-throat|groans|breath|pant|inhale|exhale|gasps|sniffs|sighs|snorts|burps|lip-smacking|humming|hissing|emm|whistles|sneezes|crying|applause)\)",
    re.IGNORECASE,
)
TTS_PAUSE_TAG_RE = re.compile(r"<#\s*\d+(?:\.\d+)?\s*#>")
MULTISPACE_RE = re.compile(r"[ \t]{2,}")
MULTILINE_SPACE_RE = re.compile(r"\n{3,}")

WorkflowEventCallback = Callable[[dict], None]

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

GOOGLE_CALENDAR_TOOL = {
    "type": "function",
    "function": {
        "name": "google_calendar_search",
        "description": (
            "Search the signed-in user's Google Calendar events. Use for appointments, "
            "meetings, plans, availability, or questions about a specific date."
        ),
        "parameters": {
            "type": "object",
            "properties": {
                "time_min": {
                    "type": "string",
                    "description": "Inclusive ISO 8601 start datetime, for example 2026-05-25T00:00:00+02:00.",
                },
                "time_max": {
                    "type": "string",
                    "description": "Exclusive ISO 8601 end datetime, for example 2026-05-26T00:00:00+02:00.",
                },
                "query": {
                    "type": "string",
                    "description": "Optional text search inside calendar events.",
                },
                "max_results": {
                    "type": "integer",
                    "description": "Maximum events to return. Defaults to 10, max 20.",
                },
            },
            "required": ["time_min", "time_max"],
            "additionalProperties": False,
        },
    },
}


@dataclass
class BenderConfig:
    openrouter_token: str
    replicate_token: str
    voice_id: str
    openrouter_base_url: str
    fallback_llm_token: str
    fallback_llm_base_url: str
    fallback_llm_model: str
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
    def from_env(cls, overrides: dict | None = None) -> "BenderConfig":
        overrides = overrides or {}
        strict_developer_mode = bool(overrides.get("_strictDeveloperMode"))
        base_dir = Path(__file__).resolve().parent
        env_candidates = [
            base_dir / ".env",
            base_dir.parent / ".env",
            base_dir.parent.parent / ".env",
            base_dir.parents[3] / ".env",
        ]
        for env_path in env_candidates:
            load_dotenv(env_path)

        if strict_developer_mode:
            api_token = overrides.get("hackClubAiKey")
            replicate_token = api_token or overrides.get("replicateApiToken", "")
            openrouter_token = api_token or overrides.get("openrouterApiToken", "")
            fallback_llm_token = str(overrides.get("pioneerApiKey", "")).strip()
            fallback_llm_base_url = overrides.get("pioneerBaseUrl") or ""
            fallback_llm_model = overrides.get("pioneerModel") or overrides.get("llmModel") or ""
            openrouter_base_url = overrides.get("openrouterBaseUrl") or ""
            llm_model = overrides.get("llmModel") or ""
        else:
            api_token = overrides.get("hackClubAiKey") or os.environ.get("HACK_CLUB_AI_KEY")
            replicate_token = api_token or overrides.get("replicateApiToken") or os.environ.get("REPLICATE_API_TOKEN")
            openrouter_token = api_token or overrides.get("openrouterApiToken") or os.environ.get("OPENROUTER_API_TOKEN")
            fallback_llm_token = (overrides.get("pioneerApiKey") or os.environ.get("PIONEER_API_KEY", "")).strip()
            fallback_llm_base_url = overrides.get("pioneerBaseUrl") or os.environ.get("PIONEER_BASE_URL", DEFAULT_FALLBACK_LLM_BASE_URL)
            fallback_llm_model = overrides.get("pioneerModel") or os.environ.get("PIONEER_MODEL", DEFAULT_FALLBACK_LLM_MODEL)
            openrouter_base_url = overrides.get("openrouterBaseUrl") or os.environ.get("OPENROUTER_BASE_URL", DEFAULT_OPENROUTER_BASE_URL)
            llm_model = overrides.get("llmModel") or os.environ.get("LLM_MODEL", DEFAULT_LLM_MODEL)
        if not openrouter_token and fallback_llm_token:
            openrouter_token = fallback_llm_token
            openrouter_base_url = fallback_llm_base_url
            llm_model = fallback_llm_model

        if not openrouter_token:
            raise RuntimeError("Developer mode requires the user's Completions API KEY or Fallback API Key.")
        if not openrouter_base_url:
            raise RuntimeError("Developer mode requires the user's Completions API URL or Fallback Completions API URL.")
        if not llm_model:
            raise RuntimeError("Developer mode requires the user's LLM model.")

        voice_id = os.environ.get("MINIMAX_VOICE_ID")
        replicate_base_url = normalize_replicate_base_url(
            (overrides.get("replicateBaseUrl") if strict_developer_mode else None)
            or os.environ.get("REPLICATE_BASE_URL", DEFAULT_REPLICATE_BASE_URL)
        )

        sysprompt = read_sysprompt(None, str(base_dir / "sysprompt.txt"))

        return cls(
            openrouter_token=openrouter_token,
            replicate_token=replicate_token,
            voice_id=voice_id,
            openrouter_base_url=openrouter_base_url,
            fallback_llm_token=fallback_llm_token,
            fallback_llm_base_url=fallback_llm_base_url,
            fallback_llm_model=fallback_llm_model,
            replicate_base_url=replicate_base_url,
            stt_model=overrides.get("sttModel") or os.environ.get("STT_MODEL", DEFAULT_STT_MODEL),
            llm_model=llm_model,
            tts_model=(overrides.get("ttsModel") if strict_developer_mode else None) or os.environ.get("TTS_MODEL", DEFAULT_TTS_MODEL),
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
            hf_token=overrides.get("hfToken") or os.environ.get("HF_TOKEN"),
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
    timezone = str(timezone).strip().strip("\"'`")
    if not timezone:
        timezone = "Europe/Madrid"
    try:
        now = dt.datetime.now(ZoneInfo(timezone))
    except (ZoneInfoNotFoundError, ValueError):
        timezone = "local"
        now = dt.datetime.now().astimezone()

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


def service_status() -> dict:
    try:
        config = BenderConfig.from_env()
    except Exception as error:
        return {
            "ok": False,
            "error": str(error),
            "services": {
                "stt": {"ok": False, "detail": "Configuration failed before STT could be checked."},
                "llm": {"ok": False, "detail": "Configuration failed before LLM could be checked."},
                "tts": {"ok": False, "detail": "Configuration failed before TTS could be checked."},
            },
        }

    stt_ok = bool(config.replicate_token and config.replicate_base_url and config.stt_model)
    llm_ok = bool(config.openrouter_token and config.openrouter_base_url and config.llm_model)
    tts_ok = bool(config.replicate_token and config.replicate_base_url and config.tts_model and config.voice_id)

    return {
        "ok": stt_ok and llm_ok and tts_ok,
        "services": {
            "stt": {
                "ok": stt_ok,
                "name": "STT",
                "model": config.stt_model,
                "base_url": config.replicate_base_url,
                "detail": "Configured" if stt_ok else "Missing Replicate/Hack Club token, base URL, or STT model.",
            },
            "llm": {
                "ok": llm_ok,
                "name": "LLM",
                "model": config.llm_model,
                "base_url": config.openrouter_base_url,
                "detail": "Configured" if llm_ok else "Missing OpenRouter/Hack Club token, base URL, or LLM model.",
            },
            "tts": {
                "ok": tts_ok,
                "name": "TTS",
                "model": config.tts_model,
                "base_url": config.replicate_base_url,
                "voice_id": config.voice_id,
                "detail": "Configured" if tts_ok else "Missing Replicate/Hack Club token, base URL, TTS model, or voice ID.",
            },
        },
    }


def google_calendar_search(
    access_token: str,
    time_min: str,
    time_max: str,
    query: str = "",
    max_results: int = 10,
) -> dict:
    max_results = max(1, min(int(max_results or 10), 20))
    params = {
        "timeMin": time_min,
        "timeMax": time_max,
        "singleEvents": "true",
        "orderBy": "startTime",
        "maxResults": str(max_results),
    }
    if query:
        params["q"] = query
    response = requests.get(
        "https://www.googleapis.com/calendar/v3/calendars/primary/events",
        headers={"Authorization": f"Bearer {access_token}"},
        params=params,
        timeout=30,
    )
    if response.status_code in {401, 403}:
        return {
            "ok": False,
            "error": "Google Calendar permission expired or is not connected. Ask the user to reconnect Calendar.",
        }
    response.raise_for_status()
    events = []
    for item in response.json().get("items", []):
        start = item.get("start", {})
        end = item.get("end", {})
        events.append(
            {
                "summary": item.get("summary", "(no title)"),
                "start": start.get("dateTime") or start.get("date"),
                "end": end.get("dateTime") or end.get("date"),
                "location": item.get("location", ""),
                "description": item.get("description", ""),
                "htmlLink": item.get("htmlLink", ""),
            }
        )
    return {"ok": True, "count": len(events), "events": events}


def run_tool_call(name: str, arguments: dict, tool_context: dict | None = None) -> object:
    render_face("tool", name)
    tool_context = tool_context or {}

    if name == "get_weather":
        return get_weather(arguments["location"])

    if name == "get_time":
        return get_time(arguments.get("timezone"))

    if name == "robot_status":
        render_face("watch", "checking status")
        return robot_status()

    if name == "google_calendar_search":
        access_token = str(tool_context.get("google_calendar_access_token") or "")
        if not access_token:
            return {"ok": False, "error": "Google Calendar is not connected for this user."}
        return google_calendar_search(
            access_token=access_token,
            time_min=arguments["time_min"],
            time_max=arguments["time_max"],
            query=str(arguments.get("query") or ""),
            max_results=int(arguments.get("max_results") or 10),
        )

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


def fallback_plain_messages(messages: list[dict]) -> list[dict]:
    plain_messages = []
    for message in messages:
        role = message.get("role", "user")
        content = message.get("content") or ""
        if role == "tool":
            tool_name = message.get("name", "tool")
            plain_messages.append(
                {
                    "role": "user",
                    "content": f"Tool result from {tool_name}: {content}",
                }
            )
            continue
        tool_calls = message.get("tool_calls") or []
        if tool_calls:
            call_names = []
            for call in tool_calls:
                function = call.get("function", {}) if isinstance(call, dict) else {}
                call_names.append(str(function.get("name", "tool")))
            content = (content + "\n" if content else "") + "Tool calls requested: " + ", ".join(call_names)
        if role not in {"system", "user", "assistant"}:
            role = "user"
        plain_messages.append({"role": role, "content": content})
    return plain_messages


def fallback_plain_payload(payload: dict, fallback_model: str) -> dict:
    return {
        "model": fallback_model,
        "messages": fallback_plain_messages(payload.get("messages", [])),
    }


class ToolContinuationRejected(RuntimeError):
    pass


def chat_completion_with_fallback(
    api_token: str,
    base_url: str,
    payload: dict,
    fallback_token: str = "",
    fallback_base_url: str = "",
    fallback_model: str = "",
) -> dict:
    try:
        return chat_completion(api_token, base_url, payload)
    except requests.RequestException as primary_error:
        if not fallback_token or not fallback_base_url or not fallback_model:
            raise
        fallback_payload = dict(payload)
        fallback_payload["model"] = fallback_model
        print(f"Primary LLM failed; retrying fallback model {fallback_model}: {primary_error}")
        try:
            return chat_completion(fallback_token, fallback_base_url, fallback_payload)
        except requests.RequestException as fallback_error:
            if any(message.get("role") == "tool" for message in payload.get("messages", [])):
                try:
                    print("Fallback LLM rejected tool continuation; retrying with plain tool results.")
                    return chat_completion(
                        fallback_token,
                        fallback_base_url,
                        fallback_plain_payload(payload, fallback_model),
                    )
                except requests.RequestException as plain_fallback_error:
                    primary_status = getattr(primary_error.response, "status_code", None)
                    fallback_status = getattr(plain_fallback_error.response, "status_code", None)
                    raise ToolContinuationRejected(
                        "LLM tool continuation unavailable. "
                        f"Primary status: {primary_status or 'network/error'}; "
                        f"fallback status: {fallback_status or 'network/error'}."
                    ) from plain_fallback_error
            primary_status = getattr(primary_error.response, "status_code", None)
            fallback_status = getattr(fallback_error.response, "status_code", None)
            raise RuntimeError(
                "LLM providers unavailable. "
                f"Primary status: {primary_status or 'network/error'}; "
                f"fallback status: {fallback_status or 'network/error'}."
            ) from fallback_error


def context_messages_from_history(history: list[dict] | None) -> list[dict]:
    if not history:
        return []

    lines = []
    total_chars = 0
    for item in history[-24:]:
        if not isinstance(item, dict):
            continue
        question = str(item.get("question", "")).strip()
        answer = str(item.get("answer", "")).strip()
        if not question and not answer:
            continue
        entry = []
        if question:
            entry.append(f"Usuario: {question}")
        if answer:
            entry.append(f"Bender: {answer}")
        content = "\n".join(entry)
        if len(content) > 2800:
            content = f"{content[:2800]}..."
        total_chars += len(content)
        if total_chars > 18000:
            break
        lines.append(content)

    if not lines:
        return []
    memory = (
        "HISTORIAL REAL DEL CHAT ACTUAL, antes del último mensaje del usuario:\n\n"
        + "\n\n---\n\n".join(lines)
        + "\n\nUsa este historial como memoria real de esta conversación. "
        + "Si el usuario pide un resumen, resume estos mensajes previos."
    )
    return [{"role": "system", "content": memory}]


def local_tool_answer(tool_results: list[tuple[str, object]]) -> str:
    if not tool_results:
        return "No he podido terminar la respuesta con las APIs disponibles."
    name, result = tool_results[-1]
    if not isinstance(result, dict):
        return f"Resultado de {name}: {result}"
    if not result.get("ok", True):
        return str(result.get("error") or "La herramienta no devolvió un resultado válido.")
    if name == "get_time":
        return f"Ahora es {result.get('time')} del {result.get('date')} ({result.get('timezone')})."
    if name == "get_weather":
        location = result.get("location") or "esa zona"
        summary = result.get("summary") or result.get("condition") or "sin resumen"
        temp = result.get("temperature_c")
        feels = result.get("feels_like_c")
        parts = [f"En {location}: {summary}"]
        if temp is not None:
            parts.append(f"{temp} C")
        if feels is not None:
            parts.append(f"sensación {feels} C")
        return ", ".join(parts) + "."
    if name == "google_calendar_search":
        events = result.get("events") or []
        if not events:
            return "No tienes eventos en ese rango."
        lines = []
        for event in events[:5]:
            lines.append(f"{event.get('start')}: {event.get('summary')}")
        return "Eventos encontrados: " + "; ".join(lines)
    if name == "robot_status":
        return json.dumps(result, ensure_ascii=False)
    return json.dumps(result, ensure_ascii=False)


def looks_like_tool_json(answer: str) -> bool:
    text = answer.strip()
    if not (text.startswith("{") and text.endswith("}")):
        return False
    try:
        payload = json.loads(text)
    except json.JSONDecodeError:
        return False
    return isinstance(payload, dict) and (
        "expression" in payload
        or "command" in payload
        or set(payload.keys()).issubset({"ok", "expression", "command"})
    )


def force_human_answer(transcript: str, answer: str) -> str:
    if not looks_like_tool_json(answer):
        return answer.strip()
    user_text = transcript.strip() or "eso"
    return (
        f"Te respondo sin numeritos raros: sobre \"{user_text}\", necesito una pregunta un poco más concreta "
        "para darte algo útil. Si querías una reacción, aquí va: sorprendido estoy, pero no tanto como mi "
        "procesador cuando le pidieron sentido común."
    )


def emit_workflow_event(
    callback: WorkflowEventCallback | None, event_type: str, **payload: object
) -> None:
    if callback:
        callback({"type": event_type, **payload})


def tool_progress_message(tool_name: str, model_message: object) -> str:
    message = str(model_message or "").strip()
    if message:
        return strip_tts_markup(message)[:300]
    fallbacks = {
        "get_weather": "Voy a mirar el tiempo. Espero que las nubes sepan usar una API.",
        "get_time": "Voy a comprobar la hora, porque mirar un reloj era demasiado fácil.",
        "robot_status": "Voy a revisar mis sistemas. Intenta no tocar nada mientras conservo la dignidad.",
        "google_calendar_search": "Voy a mirar tu calendario. Seguro que está lleno de cosas importantísimas.",
    }
    return fallbacks.get(
        tool_name,
        "Tengo que consultar otra cosa. Qué emoción: más llamadas y menos siesta.",
    )


def generate_answer(
    api_token: str,
    base_url: str,
    model: str,
    transcript: str,
    sysprompt: str,
    context: list[dict] | None = None,
    tool_context: dict | None = None,
    fallback_token: str = "",
    fallback_base_url: str = "",
    fallback_model: str = "",
    event_callback: WorkflowEventCallback | None = None,
    run_metadata: dict | None = None,
) -> str:
    tool_context = tool_context or {}
    tools = list(TOOLS)
    if tool_context.get("google_calendar_access_token"):
        tools.append(GOOGLE_CALENDAR_TOOL)
    tool_prompt = (
        sysprompt
        + "\n\nCONVERSATION MEMORY:\n"
        + "- Previous messages from the current chat may be provided before the latest user message.\n"
        + "- Treat that context as memory for this chat only.\n"
        + "- If the user asks for a summary, what they asked before, or references earlier turns, use that chat context.\n"
        + "- Do not claim to remember messages that are not present in the provided context.\n"
        + "\n\nTOOLS AVAILABLE:\n"
        + "- Use get_weather for real weather instead of guessing.\n"
        + "- Use get_time for current time/date instead of guessing.\n"
        + "- Use robot_status for robot state questions; do not invent unavailable sensor readings.\n"
        + "- Use google_calendar_search for the user's connected Google Calendar. "
        + "For relative dates, call get_time first if needed, then search the relevant date range.\n"
        + "\n\nANSWERING STYLE:\n"
        + "- Always return a normal textual answer for the user. Never return raw JSON, tool names, commands, or internal state.\n"
        + "- When you call a tool, put one short Spanish Bender-style sentence in the assistant content explaining what you are about to check. "
        + "It will be spoken while the tool runs, so make it directly related to that tool and do not claim the result is known yet.\n"
        + "- If the answer is uncomfortable, direct, or harsh, still answer clearly and honestly, with a small touch of Bender-style humor.\n"
        + "- If the user gives only a vague fragment, say what is missing and make a useful guess instead of staying silent."
    )
    messages = [
        {"role": "system", "content": tool_prompt},
    ]
    messages.extend(context_messages_from_history(context))
    messages.append({"role": "user", "content": transcript})
    final_expression = "speaking"
    tool_results: list[tuple[str, object]] = []
    metadata = run_metadata if run_metadata is not None else {}
    metadata["tools"] = []
    metadata["expression"] = final_expression

    for round_index in range(MAX_TOOL_ROUNDS):
        render_face("thinking", "generating/tool planning")
        try:
            data = chat_completion_with_fallback(
                api_token,
                base_url,
                {
                    "model": model,
                    "messages": messages,
                    "tools": tools,
                    "tool_choice": "auto",
                },
                fallback_token,
                fallback_base_url,
                fallback_model,
            )
        except ToolContinuationRejected:
            answer = local_tool_answer(tool_results)
            metadata["expression"] = final_expression
            render_face(final_expression, "local tool answer ready")
            return answer
        message = data["choices"][0]["message"]
        tool_calls = message.get("tool_calls") or []

        if not tool_calls:
            answer = message.get("content")
            if not answer or not answer.strip():
                raise RuntimeError("LLM returned an empty answer.")
            answer = force_human_answer(transcript, answer)
            metadata["expression"] = final_expression
            render_face(final_expression, "final answer ready")
            return answer

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
            progress = tool_progress_message(name, message.get("content"))
            emit_workflow_event(
                event_callback,
                "tool_start",
                tool=name,
                round=round_index + 1,
                message=progress,
            )
            try:
                arguments = parse_tool_arguments(function.get("arguments"))
                result = run_tool_call(name, arguments, tool_context)
            except Exception as error:
                render_face("error", name)
                result = {"ok": False, "error": str(error)}
            tool_results.append((name, result))
            metadata["tools"].append({"name": name, "result": result})
            if isinstance(result, dict) and result.get("expression"):
                final_expression = str(result["expression"])
                metadata["expression"] = final_expression
            emit_workflow_event(
                event_callback,
                "tool_result",
                tool=name,
                round=round_index + 1,
                result=result,
                expression=final_expression,
            )
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
    if not config.replicate_token:
        raise RuntimeError("Replicate API key is required for audio transcription.")
    if not config.replicate_base_url:
        raise RuntimeError("Replicate API URL is required for audio transcription.")
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


def generate_text_answer(
    transcript: str,
    config: BenderConfig,
    context: list[dict] | None = None,
    tool_context: dict | None = None,
    event_callback: WorkflowEventCallback | None = None,
    run_metadata: dict | None = None,
) -> str:
    render_face("thinking", "generating answer")
    answer = generate_answer(
        config.openrouter_token,
        config.openrouter_base_url,
        config.llm_model,
        transcript,
        config.sysprompt,
        context,
        tool_context,
        config.fallback_llm_token,
        config.fallback_llm_base_url,
        config.fallback_llm_model,
        event_callback,
        run_metadata,
    )
    if len(answer) > 10000:
        raise RuntimeError("LLM answer is longer than the 10,000 character TTS limit.")
    return answer


def strip_tts_markup(text: str) -> str:
    cleaned = TTS_EXPRESSION_TAG_RE.sub("", str(text or ""))
    cleaned = TTS_PAUSE_TAG_RE.sub("", cleaned)
    cleaned = MULTISPACE_RE.sub(" ", cleaned)
    cleaned = re.sub(r"\s+([,.!?;:])", r"\1", cleaned)
    cleaned = MULTILINE_SPACE_RE.sub("\n\n", cleaned)
    return cleaned.strip()


def synthesize_speech(text: str, config: BenderConfig) -> Path:
    if not config.replicate_token:
        raise RuntimeError("Replicate API key is required for TTS audio.")
    if not config.replicate_base_url:
        raise RuntimeError("Replicate API URL is required for TTS audio.")
    if not config.tts_model:
        raise RuntimeError("TTS model is required for TTS audio.")
    if not config.voice_id:
        raise RuntimeError("MINIMAX_VOICE_ID is required for TTS audio.")
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
    options = options or {}
    config = BenderConfig.from_env(options.get("config_overrides"))
    if options.get("force_wav"):
        config.audio_format = "wav"
    event_callback = options.get("event_callback")
    emit_workflow_event(event_callback, "stage", stage="transcribing")
    transcript = transcribe_audio(audio_path, config)
    emit_workflow_event(
        event_callback, "transcript", transcript=transcript
    )
    metadata: dict = {}
    answer = generate_text_answer(
        transcript,
        config,
        options.get("context"),
        options.get("tool_context"),
        event_callback,
        metadata,
    )
    emit_workflow_event(event_callback, "stage", stage="synthesizing")
    output_path = synthesize_speech(answer, config) if options.get("synthesize_audio", True) else None
    return {
        "transcript": transcript,
        "answer": strip_tts_markup(answer),
        "tts_answer": answer,
        "audio_path": str(output_path) if output_path else "",
        "expression": metadata.get("expression", "speaking"),
        "tools": metadata.get("tools", []),
    }


def process_text_message(message: str, options: dict | None = None) -> dict:
    options = options or {}
    config = BenderConfig.from_env(options.get("config_overrides"))
    if options.get("force_wav"):
        config.audio_format = "wav"
    event_callback = options.get("event_callback")
    metadata: dict = {}
    answer = generate_text_answer(
        message,
        config,
        options.get("context"),
        options.get("tool_context"),
        event_callback,
        metadata,
    )
    emit_workflow_event(event_callback, "stage", stage="synthesizing")
    output_path = synthesize_speech(answer, config) if options.get("synthesize_audio", True) else None
    return {
        "answer": strip_tts_markup(answer),
        "tts_answer": answer,
        "audio_path": str(output_path) if output_path else "",
        "expression": metadata.get("expression", "speaking"),
        "tools": metadata.get("tools", []),
    }
