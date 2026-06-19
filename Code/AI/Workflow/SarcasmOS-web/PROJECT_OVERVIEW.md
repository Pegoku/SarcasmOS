# SarcasmOS Web + Backend Overview

This document explains how the SarcasmOS web app works end-to-end: UI views, animations, chat flow, storage, and AI pipeline (STT, LLM, TTS). It is written to help you restart the project or spin up a similar chat app with full context.

## High-level Architecture

- Frontend: static HTML/CSS/JS in this folder.
- Backend: FastAPI server in `backend` exposing REST endpoints.
- Storage: localStorage in the browser + JSON file persistence on the server.
- Audio: uploads are stored on the server and generated TTS audio is served back to the client.

## Running the Project

- Entry point: `start-all.bat` or `start-all.sh` in this folder.
- It creates/uses the repo `.venv`, installs backend requirements, and starts:
  - Backend: `http://localhost:8001`
  - Frontend: `http://localhost:5173`

## Environment and Secrets

Backend reads environment variables from:
- `backend/.env`
- `.env` in this folder
- `AI/Workflow/.env`

Required:
- `MINIMAX_VOICE_ID` for TTS voice.
- `REPLICATE_API_TOKEN` or `HACK_CLUB_AI_KEY` for STT and TTS requests.
- `OPENROUTER_API_TOKEN` or `HACK_CLUB_AI_KEY` for the LLM.

Optional:
- `OPENROUTER_BASE_URL`, `REPLICATE_BASE_URL`, `FFMPEG_PATH`, model overrides.

## Backend: FastAPI API Surface

File: `backend/app.py`

Endpoints:
- `POST /api/chat/audio`
  - Accepts audio upload via multipart.
  - Saves to `backend/uploads`, calls audio pipeline, returns transcript/answer/audio_url.
- `POST /api/chat/text`
  - Sends text to LLM + TTS, returns answer/audio_url.
- `GET /api/audio/{filename}`
  - Serves audio from `backend/outputs`.
- `GET /api/status`
  - Returns robot status (mocked).
- `POST /api/command`
  - Calls eye/expression tools for demo.
- `GET /api/history`
  - Returns chat history from `backend/outputs/chat_history.json`.
- `POST /api/history`
  - Saves chat history.

## Backend: Core Pipeline

File: `backend/bender_core.py`

Flow:
1. `process_audio_file()`
   - Ensure supported audio (convert with ffmpeg if needed).
   - STT via Replicate Whisper.
   - LLM via OpenRouter.
   - TTS via Replicate Minimax.
   - Save output audio to `backend/outputs`.
2. `process_text_message()`
   - LLM + TTS only.

STT:
- Default Whisper model in `DEFAULT_STT_MODEL`.

LLM:
- Default model in `DEFAULT_LLM_MODEL`.
- Tool calls: weather, time, eye look, set expression, robot status.

TTS:
- Default model in `DEFAULT_TTS_MODEL`.
- Uses `MINIMAX_VOICE_ID` and optional parameters.

Audio conversion:
- If the input is not WAV/MP3/etc., ffmpeg converts to 16k mono WAV.

## Frontend: Views

Files:
- `index.html`
- `style.css`
- `app.js`

Views:
1. Main console view
   - Upload audio / record / text input.
   - Shows transcript + answer + audio player.
   - History list.
2. Face view (full-screen)
   - Large SVG Bender face.
   - Record / upload / text input.
   - Transcript/answer blocks + audio player.
3. Voice chat view (full-screen)
   - Same Bender face as face view.
   - Chat bubble layout (user + Bender).
   - Audio playback inside the Bender message card.

## Frontend: Chat Flow

- Audio upload or recording -> `POST /api/chat/audio`.
- Text input -> `POST /api/chat/text`.
- Response returns answer + audio URL.
- Audio URL is stored and played.
- History is updated and persisted to:
  - `localStorage` (client)
  - `backend/outputs/chat_history.json` (server)

## Frontend: History

- Stored in memory as `chatHistory`.
- Save: `persistHistory()`
- Load: `loadHistory()`
- Per-item delete button removes just that entry.

## Frontend: Animations

SVG face:
- Eyes, pupils, and mouth groups are animated via transforms.

States:
- `speaking`: mouth pulses and eyes blink.
- `thinking`: head pulse + glow sweep over controls.
- `thinking-audio`: stronger glow + squint.
- `thinking-long`: hard squint + mouth concentration.

Thinking phrases:
- A background overlay shows filler text while thinking.

Blinking:
- Controlled by JS timer with short eye squints.

Mouth movement:
- Driven by audio amplitude via Web Audio analyzer.
- Pauses when audio pauses.

## Audio Playback Rules

- Only one audio plays at a time.
- When a new audio starts, others pause.
- In voice chat, the latest audio bubble auto-plays and its progress bar updates.

## Face Sync

- Voice chat face is cloned from the face view SVG at page load.
- This keeps appearance consistent across views.

## Files and Folders

- `backend/`
  - `app.py`: FastAPI routes
  - `bender_core.py`: pipeline and tools
  - `uploads/`: incoming audio
  - `outputs/`: generated audio + history JSON
- `index.html`, `style.css`, `app.js`
- Start scripts: `start-web.*`, `start-backend.*`, `start-all.*`

## Known Pitfalls

- Audio conversion requires ffmpeg on PATH or `FFMPEG_PATH`.
- If `MINIMAX_VOICE_ID` is missing, TTS fails.
- Some browsers require a user gesture before audio playback.

## How to Reset / Start Fresh

1. Ensure `.env` has required tokens and voice ID.
2. Run `start-all.bat`.
3. Open `http://localhost:5173`.
4. If audio is silent, check browser console and confirm backend logs.

## Suggested Next Changes

- Add explicit UI for choosing the voice ID or TTS model.
- Provide server-side audio cleanup and history rotation.
- Add an explicit "Tap to enable audio" overlay for strict browsers.
