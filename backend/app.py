from pathlib import Path
import json
import mimetypes
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
    items: list[dict]


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
    if not isinstance(data, list):
        return JSONResponse(content={"items": []})
    return JSONResponse(content={"items": data})


@app.post("/api/history")
async def save_history(payload: HistoryPayload) -> JSONResponse:
    try:
        path = history_path()
        path.write_text(json.dumps(payload.items, ensure_ascii=False, indent=2), encoding="utf-8")
        return JSONResponse(content={"ok": True, "count": len(payload.items)})
    except Exception as error:
        return error_response(str(error), status_code=500)
