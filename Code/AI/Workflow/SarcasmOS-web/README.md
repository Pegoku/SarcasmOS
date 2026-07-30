# SarcasmOS Web

Static web console and local FastAPI backend for SarcasmOS.

## Run Everything Through One Port

The recommended entrypoint is the process manager:

```bash
python sarcasmos.py --start --all
```

Then open:

```text
http://localhost:9000
```

The proxy listens on `9000`, sends `/api/*` to the backend on `8001`, and sends everything else to the web UI on `5173`.

Windows:

```bat
start.bat
```

macOS/Linux:

```bash
./start.sh
```

To expose it with ngrok, install and authenticate `ngrok`, then run:

```bash
python sarcasmos.py --start --all --ngrok
```

Useful process commands:

```bash
python sarcasmos.py --status --all --ngrok
python sarcasmos.py --restart --backend
python sarcasmos.py --stop --backend
python sarcasmos.py --stop --all --ngrok
python sarcasmos.py --start --proxy
```

With `start.bat` or `start.sh`, no flags means `--start --all`; flags are passed through:

```bat
start.bat -restart -backend
start.bat -stop --all --ngrok
```

## Run The Web UI Only

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

These now start the backend, web server, and reverse proxy, then return control to your terminal. Logs and PID files live in `.sarcasmos-run`.

## Setup

The backend uses the shared repo virtual environment at `../../../.venv`.

Required environment variables go in `backend/.env`, copied from `backend/.env.example`:

- `HACK_CLUB_AI_KEY` or `REPLICATE_API_TOKEN` / `OPENROUTER_API_TOKEN`
- `MINIMAX_VOICE_ID`
- `GOOGLE_CLIENT_ID` for Google Sign-In. Create a Web OAuth client in Google Cloud and add `http://localhost:5173` as an authorized JavaScript origin.
- `ADMIN_EMAILS` comma-separated Google accounts that start as authorized admins.
- `AUTO_AUTH=true` to authorize every Google sign-in automatically and let users configure their own API endpoints/keys.
- Optional: `OPENROUTER_BASE_URL`, `REPLICATE_BASE_URL`, `HF_TOKEN`

For Google Tools such as Calendar, add these JavaScript origins to the same Google OAuth client:

```text
http://localhost:5173
http://localhost:9000
https://your-ngrok-domain.ngrok-free.dev
```

Calendar uses browser OAuth with the read-only scope:

```text
https://www.googleapis.com/auth/calendar.readonly
```
