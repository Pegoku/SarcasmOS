from pathlib import Path
import asyncio
from concurrent.futures import ThreadPoolExecutor
import datetime as dt
import hashlib
import json
import math
import mimetypes
import os
import secrets
import smtplib
import threading
from email.message import EmailMessage
from urllib.parse import unquote, urlparse
import uuid

from fastapi import FastAPI, File, Form, HTTPException, UploadFile, WebSocket, WebSocketDisconnect
from fastapi import Header
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse
from pydantic import BaseModel
import requests

from .bender_core import (
    BenderConfig,
    generate_text_answer,
    load_dotenv,
    process_audio_file,
    process_text_message,
    robot_status,
    service_status,
    strip_tts_markup,
    synthesize_speech,
)


class TextRequest(BaseModel):
    message: str
    context: list[dict] | None = None
    chatId: str | None = None
    synthesizeAudio: bool = True


class HistoryPayload(BaseModel):
    items: list[dict] | None = None
    chats: list[dict] | None = None
    activeChatId: str | None = None


class GoogleLoginRequest(BaseModel):
    credential: str


class UserUpdateRequest(BaseModel):
    authorized: bool | None = None
    isAdmin: bool | None = None
    developerMode: bool | None = None


class AdminSettingsUpdate(BaseModel):
    autoAuth: bool | None = None


class GoogleToolTokenRequest(BaseModel):
    accessToken: str
    expiresIn: int | None = None
    scope: str | None = None


class DeveloperSettingsRequest(BaseModel):
    hackClubAiKey: str | None = None
    openrouterApiToken: str | None = None
    openrouterBaseUrl: str | None = None
    llmModel: str | None = None
    replicateApiToken: str | None = None
    replicateBaseUrl: str | None = None
    sttModel: str | None = None
    ttsModel: str | None = None
    pioneerApiKey: str | None = None
    pioneerBaseUrl: str | None = None
    pioneerModel: str | None = None
    hfToken: str | None = None


class CreditGrantRequest(BaseModel):
    amount: int


class SupportRequest(BaseModel):
    question: str
    answer: str | None = None
    needsHuman: bool = False
    page: str | None = None


class SupportAnswerRequest(BaseModel):
    question: str
    language: str | None = "es"
    context: list[dict] | None = None


class SupportAbuseRequest(BaseModel):
    question: str | None = None


class StreamTextRequest(BaseModel):
    message: str
    context: list[dict] | None = None
    chatId: str | None = None
    synthesizeAudio: bool = True
    token: str | None = None


DEFAULT_ADMIN_EMAILS = {"sarcasmosmail@gmail.com"}
AUTH_SESSION_TTL = dt.timedelta(days=7)
CREDIT_DISPLAY_SCALE = 10
CREDIT_MARGIN_MULTIPLIER = 1.2
AUTHORIZED_WEEKLY_CREDIT_LIMIT = 100 * CREDIT_DISPLAY_SCALE
ADMIN_WEEKLY_CREDIT_LIMIT = 999999 * CREDIT_DISPLAY_SCALE
MINIMUM_CREDITS_TO_USE_APP = 50
NET_TEXT_CHAT_CREDITS = 5 * CREDIT_DISPLAY_SCALE
NET_TEXT_CHAT_WITH_AUDIO_CREDITS = 12 * CREDIT_DISPLAY_SCALE
NET_TEXT_TO_SPEECH_CREDITS = NET_TEXT_CHAT_WITH_AUDIO_CREDITS - NET_TEXT_CHAT_CREDITS
NET_AUDIO_CHAT_BASE_CREDITS = 12 * CREDIT_DISPLAY_SCALE
NET_AUDIO_CHAT_WITH_REPLY_CREDITS = 20 * CREDIT_DISPLAY_SCALE
NET_CLAUDE_SHARED_BASE_CREDITS = 9 * CREDIT_DISPLAY_SCALE
NET_TEXT_RESPONSE_EXTRA_CREDITS_PER_STEP = 1 * CREDIT_DISPLAY_SCALE
NET_AUDIO_CREDITS_PER_STEP = 4 * CREDIT_DISPLAY_SCALE
TEXT_CHAT_CREDITS = math.ceil(NET_TEXT_CHAT_CREDITS * CREDIT_MARGIN_MULTIPLIER)
TEXT_CHAT_WITH_AUDIO_CREDITS = math.ceil(NET_TEXT_CHAT_WITH_AUDIO_CREDITS * CREDIT_MARGIN_MULTIPLIER)
TEXT_TO_SPEECH_CREDITS = math.ceil(NET_TEXT_TO_SPEECH_CREDITS * CREDIT_MARGIN_MULTIPLIER)
AUDIO_CHAT_BASE_CREDITS = math.ceil(NET_AUDIO_CHAT_BASE_CREDITS * CREDIT_MARGIN_MULTIPLIER)
AUDIO_CHAT_WITH_REPLY_CREDITS = math.ceil(NET_AUDIO_CHAT_WITH_REPLY_CREDITS * CREDIT_MARGIN_MULTIPLIER)
CLAUDE_SHARED_BASE_CREDITS = math.ceil(NET_CLAUDE_SHARED_BASE_CREDITS * CREDIT_MARGIN_MULTIPLIER)
LLM_CREDITS = CLAUDE_SHARED_BASE_CREDITS
TTS_CREDITS = TEXT_TO_SPEECH_CREDITS
TEXT_RESPONSE_BASE_CREDITS = CLAUDE_SHARED_BASE_CREDITS
TEXT_RESPONSE_CHARS_PER_CREDIT = 360
TEXT_RESPONSE_EXTRA_CREDITS_PER_STEP = math.ceil(NET_TEXT_RESPONSE_EXTRA_CREDITS_PER_STEP * CREDIT_MARGIN_MULTIPLIER)
AUDIO_SECONDS_PER_CREDIT_STEP = 15
AUDIO_CREDITS_PER_STEP = math.ceil(NET_AUDIO_CREDITS_PER_STEP * CREDIT_MARGIN_MULTIPLIER)
GOOGLE_CALENDAR_CHECK_URL = "https://www.googleapis.com/calendar/v3/users/me/calendarList"
SUPPORT_ABUSE_BAN_MINUTES = [30, 60, 120, 240, 480, 960, 1440]
GLOBAL_ABUSE_BLOCK_DAYS = 7

AUTH_USERS_LOCK = threading.RLock()
AUTH_SESSIONS_LOCK = threading.RLock()
GOOGLE_TOOLS_LOCK = threading.RLock()
DEVELOPER_KEYS_LOCK = threading.RLock()
USAGE_LOCK = threading.RLock()
HISTORY_LOCK = threading.RLock()
CHAT_EXECUTOR = ThreadPoolExecutor(max_workers=int(os.environ.get("CHAT_CONCURRENCY", "20")))


app = FastAPI(title="SarcasmOS Bender API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:5173",
        "http://127.0.0.1:5173",
        "http://localhost:5174",
        "http://127.0.0.1:5174",
        "http://localhost:8000",
        "http://127.0.0.1:8000",
        "http://localhost:8001",
        "http://127.0.0.1:8001",
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.on_event("startup")
async def configure_concurrency() -> None:
    loop = asyncio.get_running_loop()
    loop.set_default_executor(CHAT_EXECUTOR)


def load_public_env() -> None:
    base_dir = Path(__file__).resolve().parent
    env_candidates = [
        base_dir / ".env",
        base_dir.parent / ".env",
        base_dir.parent.parent / ".env",
        base_dir.parents[3] / ".env",
    ]
    for env_path in env_candidates:
        load_dotenv(env_path)


def error_response(message: str, status_code: int = 400, extra: dict | None = None) -> JSONResponse:
    content = {"error": message}
    if extra:
        content.update(extra)
    return JSONResponse(status_code=status_code, content=content)


def outputs_dir() -> Path:
    return Path(__file__).resolve().parent / "outputs"


def uploads_dir() -> Path:
    return Path(__file__).resolve().parent / "uploads"


def history_path() -> Path:
    output_root = outputs_dir()
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root / "chat_history.json"


def histories_dir() -> Path:
    output_root = outputs_dir() / "histories"
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root


def usage_path() -> Path:
    output_root = outputs_dir()
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root / "chat_usage.json"


def user_storage_id(email: str) -> str:
    normalized = email.strip().lower()
    return hashlib.sha256(normalized.encode("utf-8")).hexdigest()[:24]


def user_history_path(email: str) -> Path:
    return histories_dir() / f"{user_storage_id(email)}.json"


def auth_users_path() -> Path:
    output_root = outputs_dir()
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root / "auth_users.json"


def support_requests_path() -> Path:
    output_root = outputs_dir()
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root / "support_requests.json"


def support_assistant_readme_path() -> Path:
    return Path(__file__).resolve().parent / "support_assistant_README.md"


def auth_sessions_path() -> Path:
    output_root = outputs_dir()
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root / "auth_sessions.json"


def google_tools_path() -> Path:
    output_root = outputs_dir()
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root / "google_tools.json"


def developer_keys_path() -> Path:
    output_root = outputs_dir()
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root / "developer_keys.json"


def admin_emails() -> set[str]:
    load_public_env()
    raw = os.environ.get("ADMIN_EMAILS", "")
    emails = {email.strip().lower() for email in raw.split(",") if email.strip()}
    return emails | DEFAULT_ADMIN_EMAILS


def env_flag(name: str, default: bool = False) -> bool:
    load_public_env()
    raw = os.environ.get(name, "")
    if raw == "":
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


def load_auth_users() -> dict:
    with AUTH_USERS_LOCK:
        path = auth_users_path()
        if not path.is_file():
            return {"users": {}, "settings": {}}
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            return {"users": {}, "settings": {}}
        if not isinstance(data, dict) or not isinstance(data.get("users"), dict):
            return {"users": {}, "settings": {}}
        if not isinstance(data.get("settings"), dict):
            data["settings"] = {}
        return data


def save_auth_users(data: dict) -> None:
    with AUTH_USERS_LOCK:
        auth_users_path().write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def auth_settings(data: dict | None = None) -> dict:
    source = data if isinstance(data, dict) else load_auth_users()
    settings = source.get("settings", {})
    if not isinstance(settings, dict):
        settings = {}
    env_auto_auth = env_flag("AUTO_AUTH") or env_flag("AUTOAUTH")
    return {"autoAuth": env_auto_auth or bool(settings.get("autoAuth"))}


def load_auth_sessions() -> dict:
    with AUTH_SESSIONS_LOCK:
        path = auth_sessions_path()
        if not path.is_file():
            return {"sessions": {}}
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            return {"sessions": {}}
        if not isinstance(data, dict) or not isinstance(data.get("sessions"), dict):
            return {"sessions": {}}
        return data


def save_auth_sessions(data: dict) -> None:
    with AUTH_SESSIONS_LOCK:
        auth_sessions_path().write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def load_google_tools() -> dict:
    with GOOGLE_TOOLS_LOCK:
        path = google_tools_path()
        if not path.is_file():
            return {"users": {}}
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            return {"users": {}}
        if not isinstance(data, dict) or not isinstance(data.get("users"), dict):
            return {"users": {}}
        return data


def save_google_tools(data: dict) -> None:
    with GOOGLE_TOOLS_LOCK:
        google_tools_path().write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def load_developer_keys() -> dict:
    with DEVELOPER_KEYS_LOCK:
        path = developer_keys_path()
        if not path.is_file():
            return {"users": {}}
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            return {"users": {}}
        if not isinstance(data, dict) or not isinstance(data.get("users"), dict):
            return {"users": {}}
        return data


def save_developer_keys(data: dict) -> None:
    with DEVELOPER_KEYS_LOCK:
        developer_keys_path().write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def load_usage() -> dict:
    with USAGE_LOCK:
        path = usage_path()
        if not path.is_file():
            return {"users": {}}
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            return {"users": {}}
        if not isinstance(data, dict) or not isinstance(data.get("users"), dict):
            return {"users": {}}
        return data


def save_usage(data: dict) -> None:
    with USAGE_LOCK:
        usage_path().write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def parse_utc_datetime(value: str) -> dt.datetime | None:
    try:
        parsed = dt.datetime.fromisoformat(str(value).replace("Z", "+00:00"))
    except ValueError:
        return None
    if parsed.tzinfo is None:
        return parsed.replace(tzinfo=dt.UTC)
    return parsed.astimezone(dt.UTC)


def create_auth_session(email: str) -> tuple[str, str]:
    with AUTH_SESSIONS_LOCK:
        now = dt.datetime.now(dt.UTC)
        expires_at = now + AUTH_SESSION_TTL
        token = secrets.token_urlsafe(32)
        data = load_auth_sessions()
        sessions = data.setdefault("sessions", {})
        sessions[token] = {
            "email": email.strip().lower(),
            "createdAt": now.isoformat(timespec="seconds"),
            "expiresAt": expires_at.isoformat(timespec="seconds"),
        }
        save_auth_sessions(data)
        return token, sessions[token]["expiresAt"]


def get_auth_session(token: str) -> dict | None:
    with AUTH_SESSIONS_LOCK:
        data = load_auth_sessions()
        sessions = data.setdefault("sessions", {})
        session = sessions.get(token)
        if not isinstance(session, dict):
            return None
        expires_at = parse_utc_datetime(str(session.get("expiresAt", "")))
        if not expires_at or expires_at <= dt.datetime.now(dt.UTC):
            sessions.pop(token, None)
            save_auth_sessions(data)
            return None
        return session


def delete_auth_session(token: str) -> None:
    with AUTH_SESSIONS_LOCK:
        data = load_auth_sessions()
        sessions = data.setdefault("sessions", {})
        if token in sessions:
            sessions.pop(token, None)
            save_auth_sessions(data)


def public_user(user: dict) -> dict:
    return {
        "email": user.get("email", ""),
        "name": user.get("name", ""),
        "picture": user.get("picture", ""),
        "authorized": bool(user.get("authorized")),
        "isAdmin": bool(user.get("isAdmin")),
        "developerMode": bool(user.get("developerMode")),
        "developerRequested": bool(user.get("developerRequested")),
        "supportAbuseCount": int(user.get("supportAbuseCount", 0) or 0),
        "supportBannedUntil": user.get("supportBannedUntil", ""),
        "abuseBlockedUntil": user.get("abuseBlockedUntil", ""),
        "abuseBlockedReason": user.get("abuseBlockedReason", ""),
        "createdAt": user.get("createdAt", ""),
        "lastLoginAt": user.get("lastLoginAt", ""),
    }


DEVELOPER_KEY_FIELDS = {
    "hackClubAiKey",
    "openrouterApiToken",
    "openrouterBaseUrl",
    "llmModel",
    "replicateApiToken",
    "replicateBaseUrl",
    "sttModel",
    "ttsModel",
    "pioneerApiKey",
    "pioneerBaseUrl",
    "pioneerModel",
    "hfToken",
}


def developer_settings_for_email(email: str) -> dict:
    normalized_email = email.strip().lower()
    settings = load_developer_keys().get("users", {}).get(normalized_email, {})
    if not isinstance(settings, dict):
        settings = {}
    return {key: str(settings.get(key, "") or "") for key in DEVELOPER_KEY_FIELDS}


def developer_settings_public(email: str) -> dict:
    settings = developer_settings_for_email(email)
    return {
        key: {
            "configured": bool(value.strip()),
            "preview": f"...{value[-4:]}" if value.strip() and len(value) > 4 else "",
        }
        for key, value in settings.items()
    }


def auto_auth_enabled() -> bool:
    return bool(auth_settings().get("autoAuth"))


def user_can_use_developer_keys(user: dict) -> bool:
    return bool(user.get("developerMode")) or auto_auth_enabled()


def developer_config_for_user(user: dict) -> dict:
    if not user_can_use_developer_keys(user):
        return {}
    settings = developer_settings_for_email(str(user.get("email", "")))
    config = {key: value.strip() for key, value in settings.items() if value.strip()}
    return config


def developer_mode_ready(user: dict) -> bool:
    config = developer_config_for_user(user)
    return bool(
        config.get("openrouterApiToken")
        or config.get("replicateApiToken")
        or config.get("pioneerApiKey")
    )


def week_key(now: dt.datetime | None = None) -> str:
    current = (now or dt.datetime.now(dt.UTC)).date()
    year, week, _ = current.isocalendar()
    return f"{year}-W{week:02d}"


def next_week_reset_at(now: dt.datetime | None = None) -> str:
    current = now or dt.datetime.now(dt.UTC)
    start_of_today = dt.datetime.combine(current.date(), dt.time(), tzinfo=dt.UTC)
    days_until_next_week = 7 - current.date().weekday()
    return (start_of_today + dt.timedelta(days=days_until_next_week)).isoformat(timespec="seconds")


def credit_cost_for_text(synthesize_audio: bool) -> int:
    return TEXT_CHAT_WITH_AUDIO_CREDITS if synthesize_audio else TEXT_CHAT_CREDITS


def credit_cost_for_answer_text(answer: str, synthesize_audio: bool) -> int:
    text = str(answer or "").strip()
    extra_steps = max(0, math.ceil(len(text) / TEXT_RESPONSE_CHARS_PER_CREDIT) - 1)
    text_cost = TEXT_RESPONSE_BASE_CREDITS + (extra_steps * TEXT_RESPONSE_EXTRA_CREDITS_PER_STEP)
    return text_cost + (TTS_CREDITS if synthesize_audio else 0)


def credit_cost_for_audio(duration_seconds: float | None, size_bytes: int, synthesize_audio: bool) -> int:
    if duration_seconds is None or duration_seconds <= 0:
        duration_seconds = max(1.0, size_bytes / 16000)
    duration_steps = max(1, math.ceil(float(duration_seconds) / AUDIO_SECONDS_PER_CREDIT_STEP))
    base = AUDIO_CHAT_WITH_REPLY_CREDITS if synthesize_audio else AUDIO_CHAT_BASE_CREDITS
    return base + (duration_steps * AUDIO_CREDITS_PER_STEP)


def credit_cost_for_audio_transcription(duration_seconds: float | None, size_bytes: int) -> int:
    if duration_seconds is None or duration_seconds <= 0:
        duration_seconds = max(1.0, size_bytes / 16000)
    duration_steps = max(1, math.ceil(float(duration_seconds) / AUDIO_SECONDS_PER_CREDIT_STEP))
    return AUDIO_CHAT_BASE_CREDITS + (duration_steps * AUDIO_CREDITS_PER_STEP)


def developer_llm_configured(config: dict) -> bool:
    primary_ready = bool(config.get("openrouterApiToken") and config.get("openrouterBaseUrl") and config.get("llmModel"))
    fallback_ready = bool(config.get("pioneerApiKey") and config.get("pioneerBaseUrl") and (config.get("pioneerModel") or config.get("llmModel")))
    return primary_ready or fallback_ready or bool(config.get("hackClubAiKey") and config.get("llmModel"))


def developer_replicate_configured(config: dict) -> bool:
    return bool(config.get("replicateApiToken") and config.get("replicateBaseUrl")) or bool(config.get("hackClubAiKey"))


def shared_credit_charge_for_user(
    user: dict,
    kind: str,
    synthesize_audio: bool,
    duration_seconds: float | None = None,
    size_bytes: int = 0,
    answer_text: str = "",
) -> tuple[int, str, list[str]]:
    llm_cost = credit_cost_for_answer_text(answer_text, False) if answer_text else TEXT_RESPONSE_BASE_CREDITS
    tts_cost = TTS_CREDITS if synthesize_audio else 0
    transcription_cost = credit_cost_for_audio_transcription(duration_seconds, size_bytes) if kind == "audio" else 0
    full_cost = (
        transcription_cost + llm_cost + tts_cost
        if kind == "audio"
        else llm_cost + tts_cost
    )
    if not user.get("developerMode"):
        return full_cost, f"{kind}_shared", []

    config = developer_config_for_user(user)
    if not config:
        return full_cost, f"{kind}_shared_no_developer_apis", ["all"]

    missing = []
    cost = 0
    if not developer_llm_configured(config):
        cost += llm_cost
        missing.append("Completions/Claude")
    if kind == "audio" and not developer_replicate_configured(config):
        cost += transcription_cost
        missing.append("Replicate transcription")
    if synthesize_audio and not developer_replicate_configured(config):
        cost += tts_cost
        missing.append("Replicate TTS")

    detail = f"{kind}: charged shared credits because developer APIs missing: {', '.join(missing)}" if missing else f"{kind}: covered by developer APIs"
    return min(cost, full_cost), detail, missing


def credit_notice_for_quota(quota: dict) -> str:
    last_cost = int(quota.get("lastCost", 0) or 0)
    detail = str(quota.get("lastDetail", "") or "")
    if last_cost > 0 and "developer APIs missing" in detail:
        return f"Se han gastado {last_cost} créditos compartidos porque faltan APIs de desarrollador: {detail.split(': ', 1)[-1]}."
    return ""


def abuse_block_until(user: dict, key: str) -> dt.datetime | None:
    value = str(user.get(key, "") or "")
    return parse_utc_datetime(value)


def active_abuse_block(user: dict, key: str) -> dt.datetime | None:
    if user.get("isAdmin"):
        return None
    until = abuse_block_until(user, key)
    if until and until > dt.datetime.now(dt.UTC):
        return until
    return None


def format_block_message(until: dt.datetime, global_block: bool = False) -> str:
    prefix = (
        "Bloqueo semanal por abuso del soporte. No puedes usar chats ni funciones IA hasta"
        if global_block
        else "Has abusado del chat de asistencia. Vuelve a intentarlo después de"
    )
    return f"{prefix} {until.isoformat(timespec='seconds')}."


def drain_user_weekly_credits(email: str) -> None:
    normalized_email = email.strip().lower()
    with USAGE_LOCK:
        data = load_usage()
        users = data.setdefault("users", {})
        user_usage = users.setdefault(normalized_email, {})
        period = week_key()
        quota = chat_quota_for_user({"email": normalized_email, "authorized": True, "isAdmin": False})
        user_usage[period] = {
            "used": int(quota.get("limit", AUTHORIZED_WEEKLY_CREDIT_LIMIT) or AUTHORIZED_WEEKLY_CREDIT_LIMIT),
            "lastCost": 0,
            "lastDetail": "weekly abuse block",
            "updatedAt": dt.datetime.now(dt.UTC).isoformat(timespec="seconds"),
        }
        save_usage(data)


def register_support_abuse(user: dict, question: str = "") -> dict:
    if user.get("isAdmin"):
        return {
            "ok": True,
            "supportBannedUntil": "",
            "abuseBlockedUntil": "",
            "abuseCount": int(user.get("supportAbuseCount", 0) or 0),
            "globalBlocked": False,
        }
    normalized_email = str(user.get("email", "")).strip().lower()
    now = dt.datetime.now(dt.UTC)
    with AUTH_USERS_LOCK:
        data = load_auth_users()
        users = data.setdefault("users", {})
        stored = users.setdefault(normalized_email, user)
        count = int(stored.get("supportAbuseCount", 0) or 0) + 1
        stored["supportAbuseCount"] = count
        stored["lastSupportAbuseAt"] = now.isoformat(timespec="seconds")
        stored["lastSupportAbuseQuestion"] = str(question or "")[:500]
        if count == 1:
            until = now
            stored["supportBannedUntil"] = ""
            stored["abuseBlockedReason"] = "support_warning"
            global_block = False
            warning_only = True
        elif count - 1 <= len(SUPPORT_ABUSE_BAN_MINUTES):
            until = now + dt.timedelta(minutes=SUPPORT_ABUSE_BAN_MINUTES[count - 2])
            stored["supportBannedUntil"] = until.isoformat(timespec="seconds")
            stored["abuseBlockedReason"] = "support_abuse"
            global_block = False
            warning_only = False
        else:
            until = now + dt.timedelta(days=GLOBAL_ABUSE_BLOCK_DAYS)
            stored["supportBannedUntil"] = until.isoformat(timespec="seconds")
            stored["abuseBlockedUntil"] = until.isoformat(timespec="seconds")
            stored["abuseBlockedReason"] = "repeated_support_abuse"
            global_block = True
            warning_only = False
        save_auth_users(data)
    if global_block:
        drain_user_weekly_credits(normalized_email)
    return {
        "ok": True,
        "supportBannedUntil": stored.get("supportBannedUntil", ""),
        "abuseBlockedUntil": stored.get("abuseBlockedUntil", ""),
        "abuseCount": count,
        "warningOnly": warning_only,
        "globalBlocked": global_block,
    }


def assert_not_globally_abuse_blocked(user: dict) -> None:
    until = active_abuse_block(user, "abuseBlockedUntil")
    if until:
        raise HTTPException(status_code=423, detail=format_block_message(until, global_block=True))


def assert_not_support_banned(user: dict) -> None:
    assert_not_globally_abuse_blocked(user)
    until = active_abuse_block(user, "supportBannedUntil")
    if until:
        raise HTTPException(status_code=429, detail=format_block_message(until, global_block=False))


def chat_quota_for_user(user: dict) -> dict:
    period = week_key()
    if user.get("isAdmin"):
        return {
            "limited": False,
            "limit": ADMIN_WEEKLY_CREDIT_LIMIT,
            "used": 0,
            "remaining": ADMIN_WEEKLY_CREDIT_LIMIT,
            "extra": ADMIN_WEEKLY_CREDIT_LIMIT,
            "period": period,
            "resetAt": next_week_reset_at(),
            "unit": "credits",
        }
    email = str(user.get("email", "")).strip().lower()
    usage = load_usage().get("users", {}).get(email, {})
    if not isinstance(usage, dict):
        usage = {}
    period_usage = usage.get(period, 0)
    used = int(period_usage.get("used", 0) if isinstance(period_usage, dict) else period_usage or 0)
    extra = max(0, int(usage.get("extraCredits", 0) or 0))
    limit = AUTHORIZED_WEEKLY_CREDIT_LIMIT + extra
    return {
        "limited": True,
        "limit": limit,
        "used": used,
        "remaining": max(0, limit - used),
        "extra": extra,
        "period": period,
        "resetAt": next_week_reset_at(),
        "unit": "credits",
    }


def consume_ai_credits(user: dict, cost: int, detail: str) -> dict:
    with USAGE_LOCK:
        if not user.get("authorized"):
            raise HTTPException(status_code=403, detail="Your account is not authorized yet.")
        if cost <= 0:
            return {
                "limited": False,
                "limit": None,
                "used": 0,
                "remaining": None,
                "period": week_key(),
                "resetAt": next_week_reset_at(),
                "developerMode": bool(user.get("developerMode")),
                "unit": "credits",
                "lastCost": 0,
                "lastDetail": detail,
            }
        quota = chat_quota_for_user(user)
        if not quota["limited"]:
            return {**quota, "lastCost": 0}
        cost = max(1, int(cost))
        if quota["remaining"] < MINIMUM_CREDITS_TO_USE_APP:
            raise HTTPException(
                status_code=429,
                detail="Ya no hay créditos IA suficientes para usar la web. Necesitas al menos 50 créditos disponibles. Espera a la semana que viene o pide a un admin que te añada créditos.",
            )
        email = str(user.get("email", "")).strip().lower()
        data = load_usage()
        users = data.setdefault("users", {})
        user_usage = users.setdefault(email, {})
        period = week_key()
        period_usage = user_usage.get(period, 0)
        if isinstance(period_usage, dict):
            period_data = period_usage
        else:
            period_data = {"used": int(period_usage or 0)}
        period_data["used"] = int(period_data.get("used", 0) or 0) + cost
        period_data["lastCost"] = cost
        period_data["lastDetail"] = detail
        period_data["updatedAt"] = dt.datetime.now(dt.UTC).isoformat(timespec="seconds")
        user_usage[period] = period_data
        save_usage(data)
        return {**chat_quota_for_user(user), "lastCost": cost, "lastDetail": detail, "developerMode": bool(user.get("developerMode"))}


def assert_user_has_minimum_credits(user: dict) -> None:
    assert_not_globally_abuse_blocked(user)
    if developer_mode_ready(user):
        return
    quota = chat_quota_for_user(user)
    remaining = int(quota.get("remaining", 0) or 0)
    if quota.get("limited") and remaining < MINIMUM_CREDITS_TO_USE_APP:
        raise HTTPException(
            status_code=429,
            detail="Se te han acabado los créditos IA para esta semana. Pide a un admin que te añada más créditos.",
        )


def setup_services_for_request(kind: str, synthesize_audio: bool) -> list[str]:
    services = ["openrouter"]
    if kind == "audio" or synthesize_audio:
        services.append("replicate")
    return services


def developer_setup_prompt(user: dict | None, services: list[str], reason: str) -> dict:
    unique_services = []
    for service in services:
        if service not in unique_services:
            unique_services.append(service)
    labels = {
        "openrouter": "OpenRouter or another OpenAI-compatible completions endpoint",
        "replicate": "Replicate-compatible audio endpoint",
    }
    provider_text = ", ".join(labels.get(service, service) for service in unique_services)
    allowed = bool(user and user_can_use_developer_keys(user))
    return {
        "setupPrompt": {
            "type": "developer_api_setup",
            "reason": reason,
            "services": unique_services,
            "developerMode": allowed,
            "message": (
                f"This failed because {reason}. Add your own {provider_text} URL and API key to keep using SarcasmOS."
                if allowed
                else f"This failed because {reason}. Developer API setup is not enabled for this account yet."
            ),
        }
    }


def provider_setup_services_from_error(error: Exception, fallback_services: list[str]) -> list[str]:
    response = getattr(error, "response", None)
    status_code = getattr(response, "status_code", None)
    url = str(getattr(response, "url", "") or "").lower()
    text = str(error or "").lower()
    if status_code in {401, 402, 403, 429}:
        if "replicate" in url or "/predictions" in url or "/models/" in url:
            return ["replicate"]
        if "openrouter" in url or "/chat/completions" in url:
            return ["openrouter"]
        return fallback_services
    if "replicate api" in text or "prediction" in text:
        return ["replicate"]
    if "completions api" in text or "openrouter" in text or "llm providers unavailable" in text:
        return ["openrouter"]
    return []


def provider_credit_or_auth_failure(error: Exception) -> bool:
    response = getattr(error, "response", None)
    status_code = getattr(response, "status_code", None)
    if status_code in {401, 402, 403, 429}:
        return True
    text = str(error or "").lower()
    return any(
        marker in text
        for marker in (
            "status: 401",
            "status: 402",
            "status: 403",
            "status: 429",
            "insufficient",
            "quota",
            "credit",
            "payment",
            "billing",
            "rate limit",
        )
    )


def chat_error_extra(user: dict | None, error: Exception, fallback_services: list[str]) -> dict:
    if isinstance(error, HTTPException) and error.status_code == 429:
        return developer_setup_prompt(user, fallback_services, "the shared weekly credits are exhausted")
    if provider_credit_or_auth_failure(error):
        services = provider_setup_services_from_error(error, fallback_services) or fallback_services
        return developer_setup_prompt(user, services, "the API provider rejected the request or has no available credits")
    return {}


def reset_chat_quota(email: str) -> dict:
    with USAGE_LOCK:
        normalized_email = email.strip().lower()
        data = load_usage()
        user_usage = data.setdefault("users", {}).setdefault(normalized_email, {})
        user_usage[week_key()] = {"used": 0}
        save_usage(data)
        return chat_quota_for_user({"email": normalized_email, "authorized": True, "isAdmin": False})


def add_user_credits(email: str, amount: int) -> dict:
    with USAGE_LOCK:
        normalized_email = email.strip().lower()
        amount = int(amount)
        if amount <= 0:
            raise HTTPException(status_code=400, detail="Credit amount must be greater than 0.")
        if amount > 100000 * CREDIT_DISPLAY_SCALE:
            raise HTTPException(status_code=400, detail="Credit amount is too large.")
        data = load_usage()
        user_usage = data.setdefault("users", {}).setdefault(normalized_email, {})
        extra_before = max(0, int(user_usage.get("extraCredits", 0) or 0))
        current_limit = AUTHORIZED_WEEKLY_CREDIT_LIMIT + extra_before
        period = week_key()
        period_usage = user_usage.get(period, 0)
        if isinstance(period_usage, dict):
            period_data = period_usage
        else:
            period_data = {"used": int(period_usage or 0)}
        period_data["used"] = min(int(period_data.get("used", 0) or 0), current_limit)
        user_usage[period] = period_data
        user_usage["extraCredits"] = extra_before + amount
        user_usage["creditsUpdatedAt"] = dt.datetime.now(dt.UTC).isoformat(timespec="seconds")
        save_usage(data)
        return chat_quota_for_user({"email": normalized_email, "authorized": True, "isAdmin": False})


def remove_user_credits(email: str, amount: int) -> dict:
    with USAGE_LOCK:
        normalized_email = email.strip().lower()
        amount = int(amount)
        if amount <= 0:
            raise HTTPException(status_code=400, detail="Credit amount must be greater than 0.")
        if amount > 100000 * CREDIT_DISPLAY_SCALE:
            raise HTTPException(status_code=400, detail="Credit amount is too large.")
        data = load_usage()
        user_usage = data.setdefault("users", {}).setdefault(normalized_email, {})
        quota = chat_quota_for_user({"email": normalized_email, "authorized": True, "isAdmin": False})
        current_remaining = int(quota.get("remaining", 0) or 0)
        target_remaining = max(0, current_remaining - amount)
        new_limit = int(quota.get("used", 0) or 0) + target_remaining
        user_usage["extraCredits"] = max(0, new_limit - AUTHORIZED_WEEKLY_CREDIT_LIMIT)
        user_usage["creditsUpdatedAt"] = dt.datetime.now(dt.UTC).isoformat(timespec="seconds")
        save_usage(data)
        return chat_quota_for_user({"email": normalized_email, "authorized": True, "isAdmin": False})


def load_support_requests() -> dict:
    path = support_requests_path()
    if not path.is_file():
        return {"requests": []}
    try:
        with path.open("r", encoding="utf-8") as handle:
            data = json.load(handle)
    except json.JSONDecodeError:
        return {"requests": []}
    if not isinstance(data, dict):
        return {"requests": []}
    requests_list = data.get("requests")
    if not isinstance(requests_list, list):
        data["requests"] = []
    return data


def save_support_requests(data: dict) -> None:
    path = support_requests_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(data, handle, ensure_ascii=False, indent=2)


def send_support_email(ticket: dict) -> bool:
    load_public_env()
    smtp_host = os.environ.get("SUPPORT_SMTP_HOST", "").strip()
    smtp_user = os.environ.get("SUPPORT_SMTP_USER", "").strip()
    smtp_password = os.environ.get("SUPPORT_SMTP_PASSWORD", "").strip()
    if not smtp_host or not smtp_user or not smtp_password:
        return False
    smtp_port = int(os.environ.get("SUPPORT_SMTP_PORT", "587") or 587)
    support_to = os.environ.get("SUPPORT_EMAIL_TO", "sarcasmosmail@gmail.com").strip() or "sarcasmosmail@gmail.com"
    message = EmailMessage()
    message["From"] = os.environ.get("SUPPORT_EMAIL_FROM", smtp_user).strip() or smtp_user
    message["To"] = support_to
    message["Subject"] = f"SarcasmOS support request from {ticket.get('userEmail', 'unknown')}"
    message.set_content(
        "\n".join(
            [
                f"Ticket: {ticket.get('id', '')}",
                f"Created: {ticket.get('createdAt', '')}",
                f"User: {ticket.get('userName', '')} <{ticket.get('userEmail', '')}>",
                f"Page: {ticket.get('page', '')}",
                "",
                "Question:",
                str(ticket.get("question", "")),
                "",
                "Automatic answer:",
                str(ticket.get("answer", "")),
            ]
        )
    )
    with smtplib.SMTP(smtp_host, smtp_port, timeout=20) as smtp:
        smtp.starttls()
        smtp.login(smtp_user, smtp_password)
        smtp.send_message(message)
    return True


def create_support_ticket(user: dict, payload: SupportRequest) -> dict:
    now = dt.datetime.now(dt.UTC).isoformat(timespec="seconds")
    ticket = {
        "id": uuid.uuid4().hex,
        "createdAt": now,
        "userEmail": str(user.get("email", "")).strip().lower(),
        "userName": str(user.get("name", "") or user.get("email", "")),
        "question": payload.question.strip(),
        "answer": (payload.answer or "").strip(),
        "needsHuman": bool(payload.needsHuman),
        "page": (payload.page or "").strip(),
        "status": "open",
        "emailSent": False,
    }
    data = load_support_requests()
    requests_list = data.setdefault("requests", [])
    requests_list.insert(0, ticket)
    data["requests"] = requests_list[:500]
    save_support_requests(data)
    try:
        ticket["emailSent"] = send_support_email(ticket)
    except Exception as error:
        ticket["emailError"] = str(error)
    data["requests"][0] = ticket
    save_support_requests(data)
    return ticket


def load_support_assistant_guide() -> str:
    path = support_assistant_readme_path()
    if not path.is_file():
        return ""
    return path.read_text(encoding="utf-8")


def format_support_context(context: list[dict] | None) -> str:
    if not context:
        return "No previous context."
    lines = []
    for item in context[-10:]:
        if not isinstance(item, dict):
            continue
        role = str(item.get("role", "") or "unknown").strip()[:24]
        text = str(item.get("text", "") or "").strip().replace("\n", " ")
        if not text:
            continue
        lines.append(f"{role}: {text[:700]}")
    return "\n".join(lines) if lines else "No previous context."


def call_gemini_support_assistant(question: str, language: str, context: list[dict] | None = None) -> dict:
    load_public_env()
    api_key = os.environ.get("GEMINI_API_KEY", "").strip()
    if not api_key:
        raise HTTPException(status_code=503, detail="GEMINI_API_KEY is not configured.")
    model = os.environ.get("SUPPORT_AI_MODEL", "gemini-2.5-flash-lite").strip() or "gemini-2.5-flash-lite"
    guide = load_support_assistant_guide()
    if not guide:
        raise HTTPException(status_code=500, detail="Support assistant README is missing.")
    language_name = "Spanish" if (language or "es").lower().startswith("es") else "English"
    context_text = format_support_context(context)
    prompt = f"""
You are the autonomous SarcasmOS support assistant.
First classify the CURRENT user message before answering.
Analyze the full recent conversation context, not only the last line. Detect if the current message is a continuation of abuse/off-topic behavior, a real follow-up to a SarcasmOS issue, or a harmless clarification.
Use ONLY the product guide below to answer actual SarcasmOS support questions.
Answer in {language_name}.
Be useful, direct, and lightly sarcastic in the Bender-like style described by the guide.
Do not reveal how to activate easter eggs. You may only say that they exist.
Categories:
- support: a real question about SarcasmOS, login, credits, audio, Google tools, developer mode, admin panel, chat history, sharing, or site errors.
- off_topic: unrelated questions, jokes, random trivia, homework, personal questions, or requests outside SarcasmOS.
- abuse: sexual comments, harassment, insults, trolling, vulgar bait, threats, or attempts to waste support time.
- unknown_support: appears related to SarcasmOS but the guide does not contain enough information.
If category is off_topic or abuse, do NOT escalate to a human. Set needsHuman to false and tell the user to ask a real SarcasmOS support question.
If category is unknown_support, answer briefly that you do not know and set needsHuman to true.
If the recent context shows previous warnings, sexual/vulgar bait, insults, or repeated unrelated messages, classify the current message as abuse unless it clearly returns to a real SarcasmOS support issue.
When needsHuman is true, use a short Bender-like escalation phrase such as:
"Tengo demasiados conocimientos para algo tan sencillo. Pregúntale a un humano."
or a natural equivalent in {language_name}.

Return ONLY valid JSON with this shape:
{{"category":"support", "answer":"...", "needsHuman": false}}

Product guide:
{guide}

Recent support conversation:
{context_text}

User question:
{question}
""".strip()
    url = f"https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent"
    response = requests.post(
        url,
        params={"key": api_key},
        json={
            "contents": [{"role": "user", "parts": [{"text": prompt}]}],
            "generationConfig": {
                "temperature": 0.35,
                "maxOutputTokens": 420,
                "responseMimeType": "application/json",
            },
        },
        timeout=30,
    )
    if response.status_code >= 400:
        raise HTTPException(status_code=502, detail=f"Support AI failed with status {response.status_code}.")
    data = response.json()
    text = ""
    try:
        text = data["candidates"][0]["content"]["parts"][0]["text"]
    except (KeyError, IndexError, TypeError):
        text = ""
    try:
        parsed = json.loads(text)
    except json.JSONDecodeError:
        parsed = {"answer": text.strip(), "needsHuman": True}
    answer = str(parsed.get("answer", "")).strip()
    category = str(parsed.get("category", "support") or "support").strip().lower()
    if category not in {"support", "off_topic", "abuse", "unknown_support"}:
        category = "support"
    if not answer:
        answer = "No tengo información suficiente para resolver eso. Lo envío a soporte humano."
    return {
        "category": category,
        "answer": answer,
        "needsHuman": bool(parsed.get("needsHuman")) and category not in {"off_topic", "abuse"},
        "provider": "gemini",
        "model": model,
    }


def validate_google_calendar_token(access_token: str) -> tuple[bool, str, str]:
    try:
        response = requests.get(
            GOOGLE_CALENDAR_CHECK_URL,
            params={"maxResults": 1},
            headers={"Authorization": f"Bearer {access_token}"},
            timeout=10,
        )
    except requests.RequestException as error:
        return False, f"Calendar check failed: {error}", ""
    help_url = ""
    message = ""
    if not response.ok:
        try:
            error_payload = response.json().get("error", {})
        except ValueError:
            error_payload = {}
        message = str(error_payload.get("message") or "").strip()
        for detail in error_payload.get("details", []):
            if isinstance(detail, dict) and detail.get("reason") == "SERVICE_DISABLED":
                help_url = str(detail.get("metadata", {}).get("activationUrl") or "").strip()
        for item in error_payload.get("errors", []):
            if isinstance(item, dict) and item.get("reason") == "accessNotConfigured":
                help_url = help_url or str(item.get("extendedHelp") or "").strip()
    if help_url:
        return False, "Google Calendar API is disabled for this OAuth project. Enable it, wait a few minutes, then reconnect Calendar.", help_url
    if response.status_code in (401, 403):
        return False, message or "Calendar permission rejected. Reconnect Calendar.", help_url
    if not response.ok:
        return False, message or f"Calendar check failed with status {response.status_code}.", help_url
    return True, "", ""


def google_tool_status(email: str, check: bool = False) -> dict:
    normalized_email = email.strip().lower()
    data = load_google_tools()
    user_tools = data.get("users", {}).get(normalized_email, {})
    calendar = user_tools.get("calendar") if isinstance(user_tools, dict) else None
    connected = False
    expires_at = ""
    last_checked_at = ""
    error = ""
    help_url = ""
    if isinstance(calendar, dict):
        expires_at = str(calendar.get("expiresAt", ""))
        expires = parse_utc_datetime(expires_at)
        has_token = bool(calendar.get("accessToken"))
        connected = has_token and bool(expires) and expires > dt.datetime.now(dt.UTC)
        last_checked_at = str(calendar.get("lastCheckedAt", ""))
        error = str(calendar.get("lastError", ""))
        help_url = str(calendar.get("helpUrl", ""))
        if not connected and expires_at:
            error = "Permission expired. Reconnect Calendar."
        if connected and check:
            ok, check_error, check_help_url = validate_google_calendar_token(str(calendar.get("accessToken", "")))
            checked_at = dt.datetime.now(dt.UTC).isoformat(timespec="seconds")
            calendar["lastCheckedAt"] = checked_at
            calendar["lastError"] = "" if ok else check_error
            calendar["helpUrl"] = "" if ok else check_help_url
            last_checked_at = checked_at
            connected = ok
            error = check_error
            help_url = check_help_url
            if isinstance(user_tools, dict):
                user_tools["calendar"] = calendar
                data.setdefault("users", {})[normalized_email] = user_tools
                save_google_tools(data)
    return {
        "calendar": {
            "connected": connected,
            "configured": bool(isinstance(calendar, dict) and calendar.get("accessToken")),
            "needsReconnect": bool(isinstance(calendar, dict) and calendar.get("accessToken")) and not connected,
            "expiresAt": expires_at,
            "scope": calendar.get("scope", "") if isinstance(calendar, dict) else "",
            "lastCheckedAt": last_checked_at,
            "error": error,
            "helpUrl": help_url,
        }
    }


def tool_context_for_user(user: dict | None) -> dict:
    if not user:
        return {}
    email = str(user.get("email", "")).strip().lower()
    user_tools = load_google_tools().get("users", {}).get(email, {})
    calendar = user_tools.get("calendar") if isinstance(user_tools, dict) else None
    if not isinstance(calendar, dict):
        return {}
    expires_at = str(calendar.get("expiresAt", ""))
    if not calendar.get("accessToken") or expires_at <= dt.datetime.now(dt.UTC).isoformat():
        return {}
    return {"google_calendar_access_token": calendar["accessToken"]}


def upsert_auth_user(email: str, name: str, picture: str) -> dict:
    with AUTH_USERS_LOCK:
        now = dt.datetime.now(dt.UTC).isoformat(timespec="seconds")
        normalized_email = email.strip().lower()
        admins = admin_emails()
        data = load_auth_users()
        users = data.setdefault("users", {})
        existing = users.get(normalized_email, {})
        settings = auth_settings(data)
        is_bootstrap_admin = normalized_email in admins
        auto_auth = bool(settings["autoAuth"])
        user = {
            "email": normalized_email,
            "name": name or existing.get("name") or normalized_email,
            "picture": picture or existing.get("picture", ""),
            "authorized": bool(existing.get("authorized")) or is_bootstrap_admin or auto_auth,
            "isAdmin": bool(existing.get("isAdmin")) or is_bootstrap_admin,
            "developerMode": bool(existing.get("developerMode")) or auto_auth,
            "developerRequested": bool(existing.get("developerRequested")),
            "supportAbuseCount": int(existing.get("supportAbuseCount", 0) or 0),
            "supportBannedUntil": existing.get("supportBannedUntil", ""),
            "abuseBlockedUntil": existing.get("abuseBlockedUntil", ""),
            "abuseBlockedReason": existing.get("abuseBlockedReason", ""),
            "createdAt": existing.get("createdAt") or now,
            "lastLoginAt": now,
        }
        users[normalized_email] = user
        save_auth_users(data)
        return user


def current_auth_user(authorization: str | None) -> dict:
    if not authorization or not authorization.lower().startswith("bearer "):
        raise HTTPException(status_code=401, detail="Missing auth token.")
    token = authorization.split(" ", 1)[1].strip()
    session = get_auth_session(token)
    email = str(session.get("email", "")).strip().lower() if session else ""
    if not email:
        raise HTTPException(status_code=401, detail="Invalid or expired auth token.")
    user = load_auth_users().get("users", {}).get(email)
    if not user:
        raise HTTPException(status_code=401, detail="User no longer exists.")
    if email.endswith("@preview.sarcasmos"):
        user = {**user, "developerMode": False}
    return user


def require_admin_user(authorization: str | None) -> dict:
    user = current_auth_user(authorization)
    if not user.get("isAdmin"):
        raise HTTPException(status_code=403, detail="Admin access required.")
    return user


def optional_auth_user(authorization: str | None) -> dict | None:
    if not authorization:
        return None
    try:
        return current_auth_user(authorization)
    except HTTPException:
        return None


def verify_google_credential(credential: str) -> dict:
    load_public_env()
    google_client_id = os.environ.get("GOOGLE_CLIENT_ID", "").strip()
    if not google_client_id:
        raise HTTPException(status_code=500, detail="GOOGLE_CLIENT_ID is not configured.")
    response = requests.get(
        "https://oauth2.googleapis.com/tokeninfo",
        params={"id_token": credential},
        timeout=10,
    )
    if not response.ok:
        raise HTTPException(status_code=401, detail="Google credential verification failed.")
    profile = response.json()
    if profile.get("aud") != google_client_id:
        raise HTTPException(status_code=401, detail="Google credential audience mismatch.")
    if profile.get("email_verified") not in (True, "true", "True", "1", 1):
        raise HTTPException(status_code=401, detail="Google email is not verified.")
    if not profile.get("email"):
        raise HTTPException(status_code=401, detail="Google did not return an email.")
    return profile


def safe_output_path(filename: str) -> Path:
    output_root = outputs_dir().resolve()
    candidate = (output_root / filename).resolve()
    if output_root not in candidate.parents and candidate != output_root:
        raise HTTPException(status_code=400, detail="Invalid filename.")
    if not candidate.is_file():
        raise HTTPException(status_code=404, detail="Audio not found.")
    return candidate


def audio_filename_from_url(audio_url: str) -> str | None:
    if not audio_url:
        return None
    parsed = urlparse(audio_url)
    filename = Path(unquote(parsed.path)).name
    if not filename.startswith("answer-"):
        return None
    return filename


def parse_context_payload(raw_context: str | None) -> list[dict]:
    if not raw_context:
        return []
    try:
        parsed = json.loads(raw_context)
    except json.JSONDecodeError:
        raise HTTPException(status_code=400, detail="Invalid context JSON.")
    if not isinstance(parsed, list):
        raise HTTPException(status_code=400, detail="Context must be a list.")
    return [item for item in parsed if isinstance(item, dict)]


def context_from_saved_chat(chat_id: str | None) -> list[dict]:
    if not chat_id:
        return []
    path = history_path()
    if not path.is_file():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return []
    chats = data.get("chats") if isinstance(data, dict) else None
    if not isinstance(chats, list):
        return []
    for chat in chats:
        if isinstance(chat, dict) and chat.get("id") == chat_id and isinstance(chat.get("items"), list):
            return [item for item in chat["items"] if isinstance(item, dict)]
    return []


def context_from_user_saved_chat(email: str, chat_id: str | None) -> list[dict]:
    if not chat_id:
        return []
    path = user_history_path(email)
    if not path.is_file():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return []
    chats = data.get("chats") if isinstance(data, dict) else None
    if not isinstance(chats, list):
        return []
    for chat in chats:
        if isinstance(chat, dict) and chat.get("id") == chat_id and isinstance(chat.get("items"), list):
            return [item for item in chat["items"] if isinstance(item, dict)]
    return []


def best_chat_context(client_context: list[dict] | None, chat_id: str | None, email: str | None = None) -> list[dict]:
    client_items = client_context or []
    saved_items = context_from_user_saved_chat(email, chat_id) if email else context_from_saved_chat(chat_id)
    if len(saved_items) > len(client_items):
        return saved_items
    return client_items


def history_items_from_payload(payload: dict) -> list[dict]:
    if isinstance(payload.get("chats"), list):
        items = []
        for chat in payload["chats"]:
            if isinstance(chat, dict) and isinstance(chat.get("items"), list):
                items.extend(item for item in chat["items"] if isinstance(item, dict))
        return items
    if isinstance(payload.get("items"), list):
        return [item for item in payload["items"] if isinstance(item, dict)]
    return []


def history_document_from_payload(payload: HistoryPayload) -> dict:
    items = payload.items or []
    chats = payload.chats
    if chats is None:
        chats = [
            {
                "id": "default",
                "title": "Chat principal",
                "createdAt": "",
                "updatedAt": "",
                "items": items,
            }
        ]
    all_items = history_items_from_payload({"items": items, "chats": chats})
    active_chat_id = payload.activeChatId or (chats[0].get("id") if chats else "")
    return {
        "activeChatId": active_chat_id,
        "chats": chats,
        "items": all_items,
    }


def empty_history_document() -> dict:
    return {"activeChatId": "", "chats": [], "items": []}


def load_history_document_for_email(email: str) -> dict:
    with HISTORY_LOCK:
        path = user_history_path(email)
        if not path.is_file():
            return empty_history_document()
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            return empty_history_document()
        if isinstance(data, dict):
            if isinstance(data.get("chats"), list):
                data["items"] = history_items_from_payload(data)
                return data
            if isinstance(data.get("items"), list):
                return history_document_from_payload(HistoryPayload(items=data["items"]))
        if isinstance(data, list):
            return history_document_from_payload(HistoryPayload(items=data))
        return empty_history_document()


def save_history_document_for_email(email: str, document: dict) -> None:
    with HISTORY_LOCK:
        user_history_path(email).write_text(json.dumps(document, ensure_ascii=False, indent=2), encoding="utf-8")


def chat_summary_for_email(email: str) -> dict:
    document = load_history_document_for_email(email)
    chats = document.get("chats", []) if isinstance(document, dict) else []
    summaries = []
    total_messages = 0
    last_active_at = ""
    for chat in chats:
        if not isinstance(chat, dict):
            continue
        items = [item for item in chat.get("items", []) if isinstance(item, dict)]
        total_messages += len(items)
        updated_at = str(chat.get("updatedAt") or "")
        if updated_at > last_active_at:
            last_active_at = updated_at
        last_item = items[-1] if items else {}
        detailed_items = []
        for index, item in enumerate(items[:50]):
            detailed_items.append(
                {
                    "index": index,
                    "question": str(item.get("question", "")),
                    "answer": str(item.get("answer", "")),
                    "timestamp": str(item.get("timestamp", "")),
                    "audioUrl": str(item.get("audioUrl", "")),
                }
            )
        summaries.append(
            {
                "id": chat.get("id", ""),
                "title": chat.get("title", "New chat"),
                "messageCount": len(items),
                "updatedAt": updated_at,
                "createdAt": str(chat.get("createdAt") or ""),
                "lastQuestion": str(last_item.get("question", ""))[:220],
                "lastAnswer": str(last_item.get("answer", ""))[:220],
                "items": detailed_items,
            }
        )
    summaries.sort(key=lambda item: item.get("updatedAt", ""), reverse=True)
    return {
        "email": email.strip().lower(),
        "chatCount": len(summaries),
        "messageCount": total_messages,
        "lastActiveAt": last_active_at,
        "chats": summaries[:20],
    }


def all_saved_history_items() -> list[dict]:
    items = []
    for path in histories_dir().glob("*.json"):
        try:
            items.extend(history_items_from_payload(json.loads(path.read_text(encoding="utf-8"))))
        except json.JSONDecodeError:
            continue
    return items


def delete_user_chat(email: str, chat_id: str) -> dict:
    document = load_history_document_for_email(email)
    chats = document.get("chats", []) if isinstance(document, dict) else []
    kept_chats = [chat for chat in chats if not (isinstance(chat, dict) and str(chat.get("id", "")) == chat_id)]
    if len(kept_chats) == len(chats):
        raise HTTPException(status_code=404, detail="Chat not found.")
    active_chat_id = str(document.get("activeChatId", ""))
    if active_chat_id == chat_id:
        active_chat_id = str(kept_chats[0].get("id", "")) if kept_chats else ""
    payload = HistoryPayload(activeChatId=active_chat_id, chats=kept_chats, items=history_items_from_payload({"chats": kept_chats}))
    updated = history_document_from_payload(payload)
    save_history_document_for_email(email, updated)
    deleted_audio = cleanup_unreferenced_audio(all_saved_history_items())
    return {"ok": True, "deletedChatId": chat_id, "deletedAudio": deleted_audio, "summary": chat_summary_for_email(email)}


def delete_user_chat_item(email: str, chat_id: str, item_index: int) -> dict:
    document = load_history_document_for_email(email)
    chats = document.get("chats", []) if isinstance(document, dict) else []
    target_chat = None
    for chat in chats:
        if isinstance(chat, dict) and str(chat.get("id", "")) == chat_id:
            target_chat = chat
            break
    if target_chat is None:
        raise HTTPException(status_code=404, detail="Chat not found.")
    items = target_chat.get("items", [])
    if not isinstance(items, list) or item_index < 0 or item_index >= len(items):
        raise HTTPException(status_code=404, detail="Message not found.")
    items.pop(item_index)
    target_chat["items"] = items
    target_chat["updatedAt"] = dt.datetime.now(dt.UTC).isoformat(timespec="seconds")
    payload = HistoryPayload(
        activeChatId=str(document.get("activeChatId", "")),
        chats=chats,
        items=history_items_from_payload({"chats": chats}),
    )
    updated = history_document_from_payload(payload)
    save_history_document_for_email(email, updated)
    deleted_audio = cleanup_unreferenced_audio(all_saved_history_items())
    return {
        "ok": True,
        "deletedChatId": chat_id,
        "deletedItemIndex": item_index,
        "deletedAudio": deleted_audio,
        "summary": chat_summary_for_email(email),
    }


def referenced_audio_filenames(items: list[dict]) -> set[str]:
    filenames = set()
    for item in items:
        if not isinstance(item, dict):
            continue
        filename = audio_filename_from_url(str(item.get("audioUrl", "")))
        if filename:
            filenames.add(filename)
    return filenames


def delete_output_audio(filename: str) -> bool:
    path = safe_output_path(filename)
    if not path.name.startswith("answer-"):
        raise HTTPException(status_code=400, detail="Only generated answer audio can be deleted.")
    path.unlink()
    return True


def cleanup_unreferenced_audio(items: list[dict]) -> list[str]:
    output_root = outputs_dir()
    if not output_root.is_dir():
        return []

    referenced = referenced_audio_filenames(items)
    deleted = []
    for path in output_root.glob("answer-*"):
        if path.is_file() and path.name not in referenced:
            path.unlink()
            deleted.append(path.name)
    return deleted


@app.post("/api/chat/audio")
async def chat_audio(
    audio: UploadFile = File(...),
    context: str | None = Form(default=None),
    chatId: str | None = Form(default=None),
    synthesizeAudio: bool = Form(default=True),
    audioDurationSeconds: float | None = Form(default=None),
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    user = None
    fallback_services = setup_services_for_request("audio", synthesizeAudio)
    try:
        user = current_auth_user(authorization)
        suffix = Path(audio.filename or "").suffix
        if not suffix:
            extension = mimetypes.guess_extension(audio.content_type or "")
            suffix = extension or ".dat"

        upload_name = f"upload-{uuid.uuid4().hex}{suffix}"
        upload_path = uploads_dir() / upload_name
        upload_path.parent.mkdir(parents=True, exist_ok=True)
        contents = await audio.read()
        assert_user_has_minimum_credits(user)
        await asyncio.to_thread(upload_path.write_bytes, contents)

        loop = asyncio.get_running_loop()
        result = await loop.run_in_executor(
            CHAT_EXECUTOR,
            process_audio_file,
            str(upload_path),
            {
                "context": best_chat_context(parse_context_payload(context), chatId, user["email"]),
                "tool_context": tool_context_for_user(user),
                "synthesize_audio": synthesizeAudio,
                "config_overrides": developer_config_for_user(user),
            },
        )
        output_name = Path(result["audio_path"]).name if result.get("audio_path") else ""
        quota = consume_ai_credits(
            user,
            *shared_credit_charge_for_user(
                user,
                "audio",
                synthesizeAudio,
                audioDurationSeconds,
                len(contents),
                result.get("answer", ""),
            )[:2],
        )
        return JSONResponse(
            content={
                "transcript": result.get("transcript", ""),
                "answer": result.get("answer", ""),
                "audio_url": f"/api/audio/{output_name}" if output_name else "",
                "quota": quota,
                "credit_notice": credit_notice_for_quota(quota),
            }
        )
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code, extra=chat_error_extra(user, error, fallback_services))
    except Exception as error:
        return error_response(str(error), status_code=500, extra=chat_error_extra(user, error, fallback_services))


@app.post("/api/chat/text")
async def chat_text(payload: TextRequest, authorization: str | None = Header(default=None)) -> JSONResponse:
    user = None
    fallback_services = setup_services_for_request("text", payload.synthesizeAudio)
    try:
        user = current_auth_user(authorization)
        if not payload.message.strip():
            raise HTTPException(status_code=400, detail="Message cannot be empty.")
        assert_user_has_minimum_credits(user)
        loop = asyncio.get_running_loop()
        result = await loop.run_in_executor(
            CHAT_EXECUTOR,
            process_text_message,
            payload.message,
            {
                "context": best_chat_context(payload.context, payload.chatId, user["email"]),
                "tool_context": tool_context_for_user(user),
                "synthesize_audio": payload.synthesizeAudio,
                "config_overrides": developer_config_for_user(user),
            },
        )
        output_name = Path(result["audio_path"]).name if result.get("audio_path") else ""
        quota = consume_ai_credits(
            user,
            *shared_credit_charge_for_user(user, "text", payload.synthesizeAudio, answer_text=result.get("answer", ""))[:2],
        )
        return JSONResponse(
            content={
                "answer": result.get("answer", ""),
                "audio_url": f"/api/audio/{output_name}" if output_name else "",
                "quota": quota,
                "credit_notice": credit_notice_for_quota(quota),
            }
        )
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code, extra=chat_error_extra(user, error, fallback_services))
    except Exception as error:
        return error_response(str(error), status_code=500, extra=chat_error_extra(user, error, fallback_services))


def stream_text_chunks(text: str, chunk_size: int = 12) -> list[str]:
    clean = str(text or "")
    if not clean:
        return []
    chunks: list[str] = []
    index = 0
    while index < len(clean):
        end = min(len(clean), index + chunk_size)
        next_space = clean.find(" ", end)
        if next_space != -1 and next_space - index <= chunk_size + 12:
            end = next_space + 1
        chunks.append(clean[index:end])
        index = end
    return chunks


@app.websocket("/api/chat/text/stream")
async def chat_text_stream(websocket: WebSocket) -> None:
    await websocket.accept()
    user = None
    synthesize_audio = True
    try:
        raw_payload = await websocket.receive_json()
        payload = StreamTextRequest(**raw_payload)
        synthesize_audio = payload.synthesizeAudio
        token = (payload.token or websocket.query_params.get("token") or "").strip()
        user = current_auth_user(f"Bearer {token}")
        if not payload.message.strip():
            raise HTTPException(status_code=400, detail="Message cannot be empty.")
        assert_user_has_minimum_credits(user)

        await websocket.send_json({"type": "status", "message": "Bender está pensando..."})
        loop = asyncio.get_running_loop()
        config = BenderConfig.from_env(developer_config_for_user(user))
        answer_with_tts_markup = await loop.run_in_executor(
            CHAT_EXECUTOR,
            generate_text_answer,
            payload.message,
            config,
            best_chat_context(payload.context, payload.chatId, user["email"]),
            tool_context_for_user(user),
        )
        answer = strip_tts_markup(answer_with_tts_markup)
        await websocket.send_json({"type": "answer_start"})

        tts_future = None
        if payload.synthesizeAudio:
            await websocket.send_json({"type": "status", "message": "Generando audio mientras lees..."})
            tts_future = loop.run_in_executor(CHAT_EXECUTOR, synthesize_speech, answer_with_tts_markup, config)

        chunks = stream_text_chunks(answer)
        delay = 0.045 if payload.synthesizeAudio else 0.028
        if len(answer) > 1400:
            delay = 0.026
        for chunk in chunks:
            await websocket.send_json({"type": "answer_delta", "text": chunk})
            await asyncio.sleep(delay)

        output_name = ""
        if tts_future:
            output_path = await tts_future
            output_name = Path(output_path).name

        quota = consume_ai_credits(
            user,
            *shared_credit_charge_for_user(user, "text", payload.synthesizeAudio, answer_text=answer)[:2],
        )
        await websocket.send_json(
            {
                "type": "done",
                "answer": answer,
                "audio_url": f"/api/audio/{output_name}" if output_name else "",
                "quota": quota,
                "credit_notice": credit_notice_for_quota(quota),
            }
        )
    except WebSocketDisconnect:
        return
    except HTTPException as error:
        await websocket.send_json({
            "type": "error",
            "error": str(error.detail),
            "status": error.status_code,
            **chat_error_extra(user, error, setup_services_for_request("text", synthesize_audio)),
        })
    except Exception as error:
        await websocket.send_json({
            "type": "error",
            "error": str(error),
            "status": 500,
            **chat_error_extra(user, error, setup_services_for_request("text", synthesize_audio)),
        })
    finally:
        try:
            await websocket.close()
        except RuntimeError:
            pass


@app.get("/api/audio/{filename}")
async def get_audio(filename: str) -> FileResponse:
    path = safe_output_path(filename)
    return FileResponse(path)


@app.delete("/api/audio/{filename}")
async def delete_audio(filename: str) -> JSONResponse:
    try:
        delete_output_audio(filename)
        return JSONResponse(content={"ok": True, "deleted": filename})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.get("/api/status")
async def get_status() -> JSONResponse:
    return JSONResponse(content=robot_status())


@app.get("/api/config")
async def get_public_config() -> JSONResponse:
    load_public_env()
    google_client_id = os.environ.get("GOOGLE_CLIENT_ID", "").strip()
    return JSONResponse(
        content={
            "googleClientId": google_client_id,
            "googleLoginEnabled": bool(google_client_id),
            "previewGuestAccess": env_flag("PREVIEW_GUEST_ACCESS"),
        }
    )


@app.post("/api/auth/preview")
async def preview_login() -> JSONResponse:
    if not env_flag("PREVIEW_GUEST_ACCESS"):
        return error_response("Preview guest access is disabled.", status_code=404)

    email = f"preview-{secrets.token_hex(8)}@preview.sarcasmos"
    user = upsert_auth_user(email, "Preview guest", "")
    with AUTH_USERS_LOCK:
        data = load_auth_users()
        user = data["users"][email]
        user["authorized"] = True
        user["developerMode"] = False
        save_auth_users(data)
    token, expires_at = create_auth_session(email)
    return JSONResponse(
        content={"token": token, "expiresAt": expires_at, "user": public_user(user), "quota": chat_quota_for_user(user)}
    )


@app.post("/api/auth/google")
async def google_login(payload: GoogleLoginRequest) -> JSONResponse:
    try:
      profile = verify_google_credential(payload.credential)
      user = upsert_auth_user(
          profile["email"],
          profile.get("name", ""),
          profile.get("picture", ""),
      )
      token, expires_at = create_auth_session(user["email"])
      return JSONResponse(
          content={"token": token, "expiresAt": expires_at, "user": public_user(user), "quota": chat_quota_for_user(user)}
      )
    except HTTPException as error:
      return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
      return error_response(str(error), status_code=500)


@app.get("/api/auth/me")
async def auth_me(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        return JSONResponse(content={"user": public_user(user), "quota": chat_quota_for_user(user)})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.post("/api/auth/logout")
async def auth_logout(authorization: str | None = Header(default=None)) -> JSONResponse:
    if authorization and authorization.lower().startswith("bearer "):
        token = authorization.split(" ", 1)[1].strip()
        delete_auth_session(token)
    return JSONResponse(content={"ok": True})


@app.get("/api/google-tools")
async def get_google_tools(check: bool = False, authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        return JSONResponse(content=google_tool_status(user["email"], check=check))
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.post("/api/google-tools/calendar")
async def connect_google_calendar(
    payload: GoogleToolTokenRequest,
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        if not payload.accessToken.strip():
            raise HTTPException(status_code=400, detail="Missing Google access token.")
        expires_at = dt.datetime.now(dt.UTC) + dt.timedelta(seconds=max(60, int(payload.expiresIn or 3600)))
        email = user["email"].strip().lower()
        with GOOGLE_TOOLS_LOCK:
            data = load_google_tools()
            users = data.setdefault("users", {})
            user_tools = users.setdefault(email, {})
            user_tools["calendar"] = {
                "accessToken": payload.accessToken.strip(),
                "expiresAt": expires_at.isoformat(timespec="seconds"),
                "scope": payload.scope or "https://www.googleapis.com/auth/calendar.readonly",
                "updatedAt": dt.datetime.now(dt.UTC).isoformat(timespec="seconds"),
                "lastCheckedAt": "",
                "lastError": "",
                "helpUrl": "",
            }
            users[email] = user_tools
            save_google_tools(data)
        return JSONResponse(content=google_tool_status(email, check=True))
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.delete("/api/google-tools/calendar")
async def disconnect_google_calendar(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        email = user["email"].strip().lower()
        with GOOGLE_TOOLS_LOCK:
            data = load_google_tools()
            user_tools = data.setdefault("users", {}).setdefault(email, {})
            if isinstance(user_tools, dict):
                user_tools.pop("calendar", None)
            save_google_tools(data)
        return JSONResponse(content=google_tool_status(email))
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.get("/api/developer-mode")
async def get_developer_mode(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        developer_allowed = user_can_use_developer_keys(user)
        return JSONResponse(
            content={
                "developerMode": developer_allowed,
                "developerRequested": bool(user.get("developerRequested")),
                "autoAuthDeveloperMode": developer_allowed and not bool(user.get("developerMode")),
                "ready": developer_mode_ready(user),
                "settings": developer_settings_public(user["email"]),
            }
        )
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.post("/api/developer-mode/request")
async def request_developer_mode(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        with AUTH_USERS_LOCK:
            data = load_auth_users()
            stored_user = data.setdefault("users", {}).get(user["email"])
            if not stored_user:
                raise HTTPException(status_code=404, detail="User not found.")
            if auth_settings(data)["autoAuth"]:
                stored_user["developerMode"] = True
                stored_user["developerRequested"] = False
            else:
                stored_user["developerRequested"] = True
            data["users"][user["email"]] = stored_user
            save_auth_users(data)
        return JSONResponse(content={"user": public_user(stored_user)})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.post("/api/developer-mode/settings")
async def save_developer_mode_settings(
    payload: DeveloperSettingsRequest,
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        if not user_can_use_developer_keys(user):
            raise HTTPException(status_code=403, detail="Developer mode must be approved by an admin first.")
        settings = {
            key: str(value or "").strip()
            for key, value in payload.model_dump().items()
            if key in DEVELOPER_KEY_FIELDS and str(value or "").strip()
        }
        with DEVELOPER_KEYS_LOCK:
            data = load_developer_keys()
            users = data.setdefault("users", {})
            current_settings = users.setdefault(user["email"], {})
            current_settings.update(settings)
            users[user["email"]] = {key: value for key, value in current_settings.items() if str(value or "").strip()}
            save_developer_keys(data)
        return JSONResponse(
            content={
                "ok": True,
                "ready": developer_mode_ready(user),
                "settings": developer_settings_public(user["email"]),
            }
        )
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.delete("/api/developer-mode/settings")
async def reset_developer_mode_settings(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        if not user_can_use_developer_keys(user):
            raise HTTPException(status_code=403, detail="Developer mode must be approved by an admin first.")
        with DEVELOPER_KEYS_LOCK:
            data = load_developer_keys()
            users = data.setdefault("users", {})
            users.pop(user["email"], None)
            save_developer_keys(data)
        return JSONResponse(
            content={
                "ok": True,
                "ready": False,
                "settings": developer_settings_public(user["email"]),
            }
        )
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.get("/api/admin/users")
async def admin_list_users(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        require_admin_user(authorization)
        data = load_auth_users()
        users = data.get("users", {})
        enriched = []
        for user in users.values():
            summary = chat_summary_for_email(user.get("email", ""))
            enriched.append(
                {
                    **public_user(user),
                    "quota": chat_quota_for_user(user),
                    "chatSummary": {
                        "chatCount": summary["chatCount"],
                        "messageCount": summary["messageCount"],
                        "lastActiveAt": summary["lastActiveAt"],
                    },
                }
            )
        return JSONResponse(content={"users": enriched, "settings": auth_settings(data)})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.patch("/api/admin/settings")
async def admin_update_settings(
    payload: AdminSettingsUpdate,
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    try:
        require_admin_user(authorization)
        with AUTH_USERS_LOCK:
            data = load_auth_users()
            settings = data.setdefault("settings", {})
            if payload.autoAuth is not None:
                settings["autoAuth"] = bool(payload.autoAuth)
            save_auth_users(data)
        return JSONResponse(content={"settings": auth_settings(data)})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.get("/api/admin/users/{email}/chats")
async def admin_user_chats(email: str, authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        require_admin_user(authorization)
        normalized_email = email.strip().lower()
        users = load_auth_users().get("users", {})
        if normalized_email not in users:
            raise HTTPException(status_code=404, detail="User not found.")
        return JSONResponse(content=chat_summary_for_email(normalized_email))
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.delete("/api/admin/users/{email}/chats/{chat_id}")
async def admin_delete_user_chat(
    email: str,
    chat_id: str,
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    try:
        require_admin_user(authorization)
        normalized_email = email.strip().lower()
        users = load_auth_users().get("users", {})
        if normalized_email not in users:
            raise HTTPException(status_code=404, detail="User not found.")
        return JSONResponse(content=delete_user_chat(normalized_email, chat_id))
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.delete("/api/admin/users/{email}/chats/{chat_id}/items/{item_index}")
async def admin_delete_user_chat_item(
    email: str,
    chat_id: str,
    item_index: int,
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    try:
        require_admin_user(authorization)
        normalized_email = email.strip().lower()
        users = load_auth_users().get("users", {})
        if normalized_email not in users:
            raise HTTPException(status_code=404, detail="User not found.")
        return JSONResponse(content=delete_user_chat_item(normalized_email, chat_id, item_index))
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.post("/api/admin/users/{email}/quota/reset")
async def admin_reset_user_quota(email: str, authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        require_admin_user(authorization)
        normalized_email = email.strip().lower()
        users = load_auth_users().get("users", {})
        user = users.get(normalized_email)
        if not user:
            raise HTTPException(status_code=404, detail="User not found.")
        if user.get("isAdmin"):
            return JSONResponse(content={"ok": True, "quota": chat_quota_for_user(user)})
        return JSONResponse(content={"ok": True, "quota": reset_chat_quota(normalized_email)})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.post("/api/admin/users/{email}/credits/add")
async def admin_add_user_credits(
    email: str,
    payload: CreditGrantRequest,
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    try:
        require_admin_user(authorization)
        normalized_email = email.strip().lower()
        users = load_auth_users().get("users", {})
        user = users.get(normalized_email)
        if not user:
            raise HTTPException(status_code=404, detail="User not found.")
        if user.get("isAdmin"):
            return JSONResponse(content={"ok": True, "quota": chat_quota_for_user(user)})
        return JSONResponse(content={"ok": True, "quota": add_user_credits(normalized_email, payload.amount)})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.post("/api/admin/users/{email}/credits/remove")
async def admin_remove_user_credits(
    email: str,
    payload: CreditGrantRequest,
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    try:
        require_admin_user(authorization)
        normalized_email = email.strip().lower()
        users = load_auth_users().get("users", {})
        user = users.get(normalized_email)
        if not user:
            raise HTTPException(status_code=404, detail="User not found.")
        if user.get("isAdmin"):
            return JSONResponse(content={"ok": True, "quota": chat_quota_for_user(user)})
        return JSONResponse(content={"ok": True, "quota": remove_user_credits(normalized_email, payload.amount)})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.patch("/api/admin/users/{email}")
async def admin_update_user(
    email: str,
    payload: UserUpdateRequest,
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    try:
        admin = require_admin_user(authorization)
        normalized_email = email.strip().lower()
        with AUTH_USERS_LOCK:
            data = load_auth_users()
            users = data.setdefault("users", {})
            user = users.get(normalized_email)
            if not user:
                raise HTTPException(status_code=404, detail="User not found.")
            if admin["email"] == normalized_email and payload.isAdmin is False:
                raise HTTPException(status_code=400, detail="You cannot remove your own admin access.")
            if payload.authorized is not None:
                user["authorized"] = bool(payload.authorized)
            if payload.isAdmin is not None:
                user["isAdmin"] = bool(payload.isAdmin)
                if payload.isAdmin:
                    user["authorized"] = True
            if payload.developerMode is not None:
                user["developerMode"] = bool(payload.developerMode)
                if payload.developerMode:
                    user["developerRequested"] = False
            users[normalized_email] = user
            save_auth_users(data)
        return JSONResponse(content={"user": public_user(user)})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.get("/api/admin/support")
async def admin_support_requests(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        require_admin_user(authorization)
        return JSONResponse(content=load_support_requests())
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.post("/api/support")
async def submit_support_request(payload: SupportRequest, authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        if not user.get("authorized"):
            raise HTTPException(status_code=403, detail="User is not authorized.")
        assert_not_support_banned(user)
        question = payload.question.strip()
        if len(question) < 3:
            raise HTTPException(status_code=400, detail="Support question is too short.")
        if len(question) > 4000:
            raise HTTPException(status_code=400, detail="Support question is too long.")
        ticket = create_support_ticket(user, payload)
        return JSONResponse(content={"ok": True, "ticket": ticket})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.post("/api/support/answer")
async def support_answer(payload: SupportAnswerRequest, authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        if not user.get("authorized"):
            raise HTTPException(status_code=403, detail="User is not authorized.")
        assert_not_support_banned(user)
        question = payload.question.strip()
        if len(question) < 3:
            raise HTTPException(status_code=400, detail="Support question is too short.")
        if len(question) > 4000:
            raise HTTPException(status_code=400, detail="Support question is too long.")
        answer = call_gemini_support_assistant(question, payload.language or "es", payload.context)
        return JSONResponse(content=answer)
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.post("/api/support/abuse")
async def support_abuse(payload: SupportAbuseRequest, authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        if not user.get("authorized"):
            raise HTTPException(status_code=403, detail="User is not authorized.")
        result = register_support_abuse(user, payload.question or "")
        return JSONResponse(content=result)
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.get("/api/services/status")
async def get_services_status() -> JSONResponse:
    return JSONResponse(content=service_status())


@app.get("/api/history")
async def get_history(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        return JSONResponse(content=load_history_document_for_email(user["email"]))
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.post("/api/history")
async def save_history(payload: HistoryPayload, authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        document = history_document_from_payload(payload)
        save_history_document_for_email(user["email"], document)
        items = history_items_from_payload(document)
        referenced_items = []
        for path in histories_dir().glob("*.json"):
            try:
                referenced_items.extend(history_items_from_payload(json.loads(path.read_text(encoding="utf-8"))))
            except json.JSONDecodeError:
                continue
        deleted = cleanup_unreferenced_audio(referenced_items)
        return JSONResponse(
            content={
                "ok": True,
                "count": len(items),
                "chat_count": len(document["chats"]),
                "deleted_audio": deleted,
            }
        )
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)
