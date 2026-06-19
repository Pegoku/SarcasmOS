@echo off
setlocal

set "WEB_DIR=%~dp0"
if "%~1"=="" (
  python "%WEB_DIR%sarcasmos.py" --start --all --ngrok --keepalive
) else (
  python "%WEB_DIR%sarcasmos.py" %*
)

endlocal
