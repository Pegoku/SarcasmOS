# sarcasmos

Personal assistant characterized by Bender from Futurama (In spanish)

SascarmOS is a personality-driven operating system that blends artificial intelligence with expressive hardware to create a truly interactive assistant.

The name reflects its core idea: “sarcasm” defines the character, while “OS” represents the intelligent system behind it. Instead of a neutral assistant, SarcasmOS delivers responses with attitude,humor, and a distinct personality inspired by Bender.

The device takes the form of a robotic head with built-in displays in the eyes and the mouth, allowing real-time animations and expressions. This gives the illusion that the character is alive, reacting visually and emotionally to every interaction.

Functionally, it works like a smart assistant similar to Google Home or Alexa handling voice commands, answering questions, and controlling smart devices. The difference is in how it communicates: SarcasmOS doesn’t just respond, it reacts.

In Short, SarcasmOS turns a standard AI assistant into an engaging character where technology meets personality.

## Web App

This workspace includes a lightweight web UI plus FastAPI backend for audio upload, recording, and TTS playback.

### Setup

```bash
python -m venv .venv
# Windows
.venv\Scripts\activate
# macOS/Linux
source .venv/bin/activate
pip install -r backend/requirements.txt
```

Create a `.env` file in `backend/` based on the example:

```bash
copy backend\.env.example backend\.env
```

### Required environment variables

- `HACK_CLUB_AI_KEY` or `REPLICATE_API_TOKEN` / `OPENROUTER_API_TOKEN`
- `MINIMAX_VOICE_ID`
- Optional: `OPENROUTER_BASE_URL`, `REPLICATE_BASE_URL`, `HF_TOKEN`

### Run the backend

```bash
uvicorn backend.app:app --reload --host 0.0.0.0 --port 8000
```

### Run the frontend

Open `AI/Workflow/SarcasmOS-web/index.html` directly in the browser, or serve it locally:

```bash
python -m http.server 5173 -d AI/Workflow/SarcasmOS-web
```

### Audio conversion

If you record in the browser, the audio is often `webm/opus`. If the STT model rejects it, the backend will convert to wav. Install `ffmpeg` and ensure it is on your PATH.

### Notes

- Uploaded audio is stored in `backend/uploads/`.
- Generated TTS audio is stored in `backend/outputs/`.
- API keys remain in the backend only.

