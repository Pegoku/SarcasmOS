from pathlib import Path
import json
import mimetypes
from urllib.parse import unquote, urlparse
import uuid

from fastapi import FastAPI, File, HTTPException, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse
from pydantic import BaseModel

from .bender_core import process_audio_file, process_text_message, robot_status, run_tool_call


class TextRequest(BaseModel):
    message: str


class CommandRequest(BaseModel):
    command: str


class HistoryPayload(BaseModel):
    items: list[dict] | None = None
    chats: list[dict] | None = None
    activeChatId: str | None = None


app = FastAPI(title="SarcasmOS Bender API")

app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:5173",
        "http://127.0.0.1:5173",
        "http://localhost:8000",
        "http://127.0.0.1:8000",
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


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
async def chat_audio(audio: UploadFile = File(...)) -> JSONResponse:
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

        result = process_audio_file(str(upload_path))
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
        result = process_text_message(payload.message)
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
