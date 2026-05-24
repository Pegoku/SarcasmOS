@echo off
setlocal

set "ROOT_DIR=%~dp0"
set "VENV_DIR=%ROOT_DIR%.venv"
set "BACKEND_ENV=%ROOT_DIR%backend\.env"
set "PYTHON_BIN=%VENV_DIR%\Scripts\python.exe"

cd /d "%ROOT_DIR%"

if not exist "%VENV_DIR%\Scripts\python.exe" (
  python -m venv "%VENV_DIR%"
)

"%PYTHON_BIN%" -m pip install -r "%ROOT_DIR%backend\requirements.txt"
"%PYTHON_BIN%" -m pip check >nul
if errorlevel 1 (
  "%PYTHON_BIN%" -m pip install --ignore-installed -r "%ROOT_DIR%backend\requirements.txt"
)

if not exist "%BACKEND_ENV%" (
  copy "%ROOT_DIR%backend\.env.example" "%BACKEND_ENV%" >nul
  echo Created backend\.env from backend\.env.example. Add your API keys before using chat/TTS.
)

echo Starting backend on http://localhost:8001
start "SarcasmOS Backend" cmd /k ""%PYTHON_BIN%" -m uvicorn backend.app:app --host 0.0.0.0 --port 8001"

echo Starting frontend on http://localhost:5173
echo Open http://localhost:5173 in your browser. Press Ctrl+C to stop this window.
"%PYTHON_BIN%" -m http.server 5173 -d "%ROOT_DIR%AI\Workflow\SarcasmOS-web"

endlocal
