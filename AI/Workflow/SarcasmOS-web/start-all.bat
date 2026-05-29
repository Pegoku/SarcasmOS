@echo off
setlocal

set "WEB_DIR=%~dp0"
python "%WEB_DIR%sarcasmos.py" --start --all --ngrok --keepalive %*

endlocal
