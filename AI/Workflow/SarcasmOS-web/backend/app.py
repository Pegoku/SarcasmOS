from pathlib import Path
import datetime as dt
import hashlib
import json
import mimetypes
import os
import secrets
from urllib.parse import unquote, urlparse
import uuid

from fastapi import FastAPI, File, Form, HTTPException, UploadFile
from fastapi import Header
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse
from pydantic import BaseModel
import requests

from .bender_core import load_dotenv, process_audio_file, process_text_message, robot_status, service_status


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


class GoogleToolTokenRequest(BaseModel):
    accessToken: str
    expiresIn: int | None = None
    scope: str | None = None


DEFAULT_ADMIN_EMAILS = {"sarcasmosmail@gmail.com"}
AUTH_SESSION_TTL = dt.timedelta(days=7)
AUTHORIZED_WEEKLY_CHAT_LIMIT = 5
GOOGLE_CALENDAR_CHECK_URL = "https://www.googleapis.com/calendar/v3/users/me/calendarList"


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


def error_response(message: str, status_code: int = 400) -> JSONResponse:
    return JSONResponse(status_code=status_code, content={"error": message})


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


def auth_sessions_path() -> Path:
    output_root = outputs_dir()
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root / "auth_sessions.json"


def google_tools_path() -> Path:
    output_root = outputs_dir()
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root / "google_tools.json"


def admin_emails() -> set[str]:
    load_public_env()
    raw = os.environ.get("ADMIN_EMAILS", "")
    emails = {email.strip().lower() for email in raw.split(",") if email.strip()}
    return emails | DEFAULT_ADMIN_EMAILS


def load_auth_users() -> dict:
    path = auth_users_path()
    if not path.is_file():
        return {"users": {}}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {"users": {}}
    if not isinstance(data, dict) or not isinstance(data.get("users"), dict):
        return {"users": {}}
    return data


def save_auth_users(data: dict) -> None:
    auth_users_path().write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def load_auth_sessions() -> dict:
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
    auth_sessions_path().write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def load_google_tools() -> dict:
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
    google_tools_path().write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def load_usage() -> dict:
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
        "createdAt": user.get("createdAt", ""),
        "lastLoginAt": user.get("lastLoginAt", ""),
    }


def week_key(now: dt.datetime | None = None) -> str:
    current = (now or dt.datetime.now(dt.UTC)).date()
    year, week, _ = current.isocalendar()
    return f"{year}-W{week:02d}"


def next_week_reset_at(now: dt.datetime | None = None) -> str:
    current = now or dt.datetime.now(dt.UTC)
    start_of_today = dt.datetime.combine(current.date(), dt.time(), tzinfo=dt.UTC)
    days_until_next_week = 7 - current.date().weekday()
    return (start_of_today + dt.timedelta(days=days_until_next_week)).isoformat(timespec="seconds")


def chat_quota_for_user(user: dict) -> dict:
    if user.get("isAdmin"):
        return {
            "limited": False,
            "limit": None,
            "used": 0,
            "remaining": None,
            "period": week_key(),
            "resetAt": next_week_reset_at(),
        }
    email = str(user.get("email", "")).strip().lower()
    period = week_key()
    usage = load_usage().get("users", {}).get(email, {})
    used = int(usage.get(period, 0)) if isinstance(usage, dict) else 0
    return {
        "limited": True,
        "limit": AUTHORIZED_WEEKLY_CHAT_LIMIT,
        "used": used,
        "remaining": max(0, AUTHORIZED_WEEKLY_CHAT_LIMIT - used),
        "period": period,
        "resetAt": next_week_reset_at(),
    }


def consume_chat_quota(user: dict) -> dict:
    if not user.get("authorized"):
        raise HTTPException(status_code=403, detail="Your account is not authorized yet.")
    quota = chat_quota_for_user(user)
    if not quota["limited"]:
        return quota
    if quota["remaining"] <= 0:
        raise HTTPException(
            status_code=429,
            detail=(
                "Se te han acabado los mensajes disponibles hasta la proxima semana. "
                "Pide a un admin que te reactive 5 mensajes si necesitas seguir usando el chat ahora."
            ),
        )
    email = str(user.get("email", "")).strip().lower()
    data = load_usage()
    users = data.setdefault("users", {})
    user_usage = users.setdefault(email, {})
    period = week_key()
    user_usage[period] = int(user_usage.get(period, 0)) + 1
    save_usage(data)
    return chat_quota_for_user(user)


def reset_chat_quota(email: str) -> dict:
    normalized_email = email.strip().lower()
    data = load_usage()
    user_usage = data.setdefault("users", {}).setdefault(normalized_email, {})
    user_usage[week_key()] = 0
    save_usage(data)
    return chat_quota_for_user({"email": normalized_email, "authorized": True, "isAdmin": False})


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
    now = dt.datetime.now(dt.UTC).isoformat(timespec="seconds")
    normalized_email = email.strip().lower()
    admins = admin_emails()
    data = load_auth_users()
    users = data.setdefault("users", {})
    existing = users.get(normalized_email, {})
    is_bootstrap_admin = normalized_email in admins
    user = {
        "email": normalized_email,
        "name": name or existing.get("name") or normalized_email,
        "picture": picture or existing.get("picture", ""),
        "authorized": bool(existing.get("authorized")) or is_bootstrap_admin,
        "isAdmin": bool(existing.get("isAdmin")) or is_bootstrap_admin,
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
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        quota = consume_chat_quota(user)
        suffix = Path(audio.filename or "").suffix
        if not suffix:
            extension = mimetypes.guess_extension(audio.content_type or "")
            suffix = extension or ".dat"

        upload_name = f"upload-{uuid.uuid4().hex}{suffix}"
        upload_path = uploads_dir() / upload_name
        upload_path.parent.mkdir(parents=True, exist_ok=True)
        contents = await audio.read()
        upload_path.write_bytes(contents)

        result = process_audio_file(
            str(upload_path),
            {
                "context": best_chat_context(parse_context_payload(context), chatId, user["email"]),
                "tool_context": tool_context_for_user(user),
                "synthesize_audio": synthesizeAudio,
            },
        )
        output_name = Path(result["audio_path"]).name if result.get("audio_path") else ""
        return JSONResponse(
            content={
                "transcript": result.get("transcript", ""),
                "answer": result.get("answer", ""),
                "audio_url": f"/api/audio/{output_name}" if output_name else "",
                "quota": quota,
            }
        )
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.post("/api/chat/text")
async def chat_text(payload: TextRequest, authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        if not payload.message.strip():
            raise HTTPException(status_code=400, detail="Message cannot be empty.")
        quota = consume_chat_quota(user)
        result = process_text_message(
            payload.message,
            {
                "context": best_chat_context(payload.context, payload.chatId, user["email"]),
                "tool_context": tool_context_for_user(user),
                "synthesize_audio": payload.synthesizeAudio,
            },
        )
        output_name = Path(result["audio_path"]).name if result.get("audio_path") else ""
        return JSONResponse(
            content={
                "answer": result.get("answer", ""),
                "audio_url": f"/api/audio/{output_name}" if output_name else "",
                "quota": quota,
            }
        )
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


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
        }
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
        data = load_google_tools()
        user_tools = data.setdefault("users", {}).setdefault(email, {})
        if isinstance(user_tools, dict):
            user_tools.pop("calendar", None)
        save_google_tools(data)
        return JSONResponse(content=google_tool_status(email))
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.get("/api/admin/users")
async def admin_list_users(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        require_admin_user(authorization)
        users = load_auth_users().get("users", {})
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
        return JSONResponse(content={"users": enriched})
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


@app.patch("/api/admin/users/{email}")
async def admin_update_user(
    email: str,
    payload: UserUpdateRequest,
    authorization: str | None = Header(default=None),
) -> JSONResponse:
    try:
        admin = require_admin_user(authorization)
        normalized_email = email.strip().lower()
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
        users[normalized_email] = user
        save_auth_users(data)
        return JSONResponse(content={"user": public_user(user)})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


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
