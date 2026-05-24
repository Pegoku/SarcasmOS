from pathlib import Path
import datetime as dt
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

from .bender_core import load_dotenv, process_audio_file, process_text_message, robot_status, run_tool_call, service_status


class TextRequest(BaseModel):
    message: str
    context: list[dict] | None = None
    chatId: str | None = None


class CommandRequest(BaseModel):
    command: str


class HistoryPayload(BaseModel):
    items: list[dict] | None = None
    chats: list[dict] | None = None
    activeChatId: str | None = None


class GoogleLoginRequest(BaseModel):
    credential: str


class UserUpdateRequest(BaseModel):
    authorized: bool | None = None
    isAdmin: bool | None = None


DEFAULT_ADMIN_EMAILS = {"sarcasmosmail@gmail.com"}
AUTH_SESSIONS: dict[str, str] = {}


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


def auth_users_path() -> Path:
    output_root = outputs_dir()
    output_root.mkdir(parents=True, exist_ok=True)
    return output_root / "auth_users.json"


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
    email = AUTH_SESSIONS.get(token)
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


def best_chat_context(client_context: list[dict] | None, chat_id: str | None) -> list[dict]:
    client_items = client_context or []
    saved_items = context_from_saved_chat(chat_id)
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


def handle_command(command: str) -> dict:
    command = command.strip()
    if command.startswith("eye.look."):
        direction = command.split(".")[-1]
        return run_tool_call("eye_look", {"direction": direction})
    if command.startswith("expression."):
        expression = command.split(".")[-1]
        return run_tool_call("set_expression", {"expression": expression})
    if command == "robot.status":
        return run_tool_call("robot_status", {})
    return {"ok": False, "error": f"Unknown command: {command}"}


@app.post("/api/chat/audio")
async def chat_audio(
    audio: UploadFile = File(...),
    context: str | None = Form(default=None),
    chatId: str | None = Form(default=None),
) -> JSONResponse:
    try:
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
            {"context": best_chat_context(parse_context_payload(context), chatId)},
        )
        output_name = Path(result["audio_path"]).name
        return JSONResponse(
            content={
                "transcript": result.get("transcript", ""),
                "answer": result.get("answer", ""),
                "audio_url": f"/api/audio/{output_name}",
            }
        )
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.post("/api/chat/text")
async def chat_text(payload: TextRequest) -> JSONResponse:
    try:
        if not payload.message.strip():
            raise HTTPException(status_code=400, detail="Message cannot be empty.")
        result = process_text_message(
            payload.message,
            {"context": best_chat_context(payload.context, payload.chatId)},
        )
        output_name = Path(result["audio_path"]).name
        return JSONResponse(
            content={
                "answer": result.get("answer", ""),
                "audio_url": f"/api/audio/{output_name}",
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
      token = secrets.token_urlsafe(32)
      AUTH_SESSIONS[token] = user["email"]
      return JSONResponse(content={"token": token, "user": public_user(user)})
    except HTTPException as error:
      return error_response(str(error.detail), status_code=error.status_code)
    except Exception as error:
      return error_response(str(error), status_code=500)


@app.get("/api/auth/me")
async def auth_me(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        user = current_auth_user(authorization)
        return JSONResponse(content={"user": public_user(user)})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


@app.post("/api/auth/logout")
async def auth_logout(authorization: str | None = Header(default=None)) -> JSONResponse:
    if authorization and authorization.lower().startswith("bearer "):
        token = authorization.split(" ", 1)[1].strip()
        AUTH_SESSIONS.pop(token, None)
    return JSONResponse(content={"ok": True})


@app.get("/api/admin/users")
async def admin_list_users(authorization: str | None = Header(default=None)) -> JSONResponse:
    try:
        require_admin_user(authorization)
        users = load_auth_users().get("users", {})
        return JSONResponse(content={"users": [public_user(user) for user in users.values()]})
    except HTTPException as error:
        return error_response(str(error.detail), status_code=error.status_code)


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


@app.post("/api/command")
async def command(payload: CommandRequest) -> JSONResponse:
    try:
        result = handle_command(payload.command)
        return JSONResponse(content=result)
    except Exception as error:
        return error_response(str(error), status_code=500)


@app.get("/api/history")
async def get_history() -> JSONResponse:
    path = history_path()
    if not path.is_file():
        return JSONResponse(content={"items": []})
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return JSONResponse(content={"items": []})
    if isinstance(data, dict):
        if isinstance(data.get("chats"), list):
            data["items"] = history_items_from_payload(data)
            return JSONResponse(content=data)
        if isinstance(data.get("items"), list):
            return JSONResponse(content=data)
    if isinstance(data, list):
        return JSONResponse(content={"items": data})
    return JSONResponse(content={"items": []})


@app.post("/api/history")
async def save_history(payload: HistoryPayload) -> JSONResponse:
    try:
        path = history_path()
        document = history_document_from_payload(payload)
        path.write_text(json.dumps(document, ensure_ascii=False, indent=2), encoding="utf-8")
        items = history_items_from_payload(document)
        deleted = cleanup_unreferenced_audio(items)
        return JSONResponse(
            content={
                "ok": True,
                "count": len(items),
                "chat_count": len(document["chats"]),
                "deleted_audio": deleted,
            }
        )
    except Exception as error:
        return error_response(str(error), status_code=500)
