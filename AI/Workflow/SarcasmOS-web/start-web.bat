@echo off
setlocal

set "WEB_DIR=%~dp0"

echo Starting SarcasmOS web on http://localhost:5173
echo Press Ctrl+C to stop.
python -m http.server 5173 -d "%WEB_DIR%"

endlocal
