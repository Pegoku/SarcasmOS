# SarcasmOS Web

Static web console and local FastAPI backend for SarcasmOS.

## Run The Web UI

From this folder:

```bash
python -m http.server 5173
```

Then open:

```text
http://localhost:5173
```

The UI expects the SarcasmOS backend at:

```text
http://localhost:8001
```

## Run The Backend

From this folder:

```bash
python -m uvicorn backend.app:app --host 0.0.0.0 --port 8001
```

Backend data lives here:

```text
backend/uploads
backend/outputs
```

On Windows you can also run:

```bat
start-web.bat
```

On macOS/Linux:

```bash
./start-web.sh
```

## Run Everything

On Windows:

```bat
start-all.bat
```

On macOS/Linux:

```bash
./start-all.sh
```

## Setup

The backend uses the shared repo virtual environment at `../../../.venv`.

Required environment variables go in `backend/.env`, copied from `backend/.env.example`:

- `HACK_CLUB_AI_KEY` or `REPLICATE_API_TOKEN` / `OPENROUTER_API_TOKEN`
- `MINIMAX_VOICE_ID`
- `GOOGLE_CLIENT_ID` for Google Sign-In. Create a Web OAuth client in Google Cloud and add `http://localhost:5173` as an authorized JavaScript origin.
- `ADMIN_EMAILS` comma-separated Google accounts that start as authorized admins.
- Optional: `OPENROUTER_BASE_URL`, `REPLICATE_BASE_URL`, `HF_TOKEN`
