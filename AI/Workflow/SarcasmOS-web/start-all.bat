@echo off
setlocal

set "WEB_DIR=%~dp0"
set "ROOT_DIR=%WEB_DIR%..\..\.."
set "VENV_DIR=%ROOT_DIR%\.venv"
set "PYTHON_BIN=%VENV_DIR%\Scripts\python.exe"
set "BACKEND_ENV=%WEB_DIR%backend\.env"

if not exist "%PYTHON_BIN%" (
  python -m venv "%VENV_DIR%"
)

"%PYTHON_BIN%" -m pip install -r "%WEB_DIR%backend\requirements.txt"
"%PYTHON_BIN%" -m pip check >nul
if errorlevel 1 (
  "%PYTHON_BIN%" -m pip install --ignore-installed -r "%WEB_DIR%backend\requirements.txt"
)

if not exist "%BACKEND_ENV%" (
  copy "%WEB_DIR%backend\.env.example" "%BACKEND_ENV%" >nul
  echo Created backend\.env from backend\.env.example. Add your API keys before using chat/TTS.
)

cd /d "%WEB_DIR%"
echo Starting backend on http://localhost:8001
start "SarcasmOS Backend" cmd /k ""%PYTHON_BIN%" -m uvicorn backend.app:app --host 0.0.0.0 --port 8001"

echo Starting web on http://localhost:5173
"%PYTHON_BIN%" -m http.server 5173 -d "%WEB_DIR%"

endlocal
