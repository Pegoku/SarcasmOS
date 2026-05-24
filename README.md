# SarcasmOS

SarcasmOS es un asistente de voz con personalidad, construido como proyecto para Hack Club / Fallout. La idea es sencilla: coger un asistente tipo Alexa o Google Home y darle una cara, una voz y una actitud reconocible. En vez de contestar como una caja gris corporativa, responde en español con humor seco, sarcasmo y expresiones animadas inspiradas en Bender.

El proyecto combina software, IA, diseño mecanico y electronica: una cabeza robotica con pantallas para ojos y boca, una interfaz web para probar conversaciones, un backend local para voz/texto, y placas PCB dedicadas para el hardware visual.

## Que Hace

- Escucha o recibe texto desde una interfaz web local.
- Transcribe audio a texto.
- Genera respuestas con una personalidad sarcastica en español.
- Convierte las respuestas a voz.
- Sincroniza reproduccion de audio con una cara animada.
- Guarda historial de chats.
- Incluye comandos y estados pensados para controlar expresiones, ojos y comportamiento del robot.

## Estado Actual

SarcasmOS ya tiene una demo funcional por software:

- Web UI estatica en `AI/Workflow/SarcasmOS-web`.
- Backend FastAPI local con endpoints de chat, audio, historial y estado.
- Pipeline de STT, LLM y TTS usando servicios externos configurables.
- Vista tipo consola, vista de cara completa y vista de chat por voz.
- Archivos CAD para la cabeza/cuerpo.
- Diseños KiCad para las placas de ojos y boca.
- Experimentos de clonacion/sintesis de voz y separacion de clips.

El hardware esta en desarrollo. El repo contiene los archivos de diseño, fabricacion y pruebas, pero no debe tratarse todavia como un producto terminado.

## Estructura Del Repo

```text
AI/
  Workflow/SarcasmOS-web/   App web + backend principal
  Piannote/                 Separacion y preparacion de clips de voz
  Qwen3-TTS-Bender/         Experimentos de TTS / voz
  xtts/                     Pruebas alternativas de TTS

CAD/                        Modelos FreeCAD de cabeza, base y piezas
PCB/
  eye/                      PCB de los ojos
  mouth/                    PCB de la boca

animationVisualizer/        Prototipo visual de animaciones
README.md                   Este documento
```

## Demo Web

La forma mas rapida de probar SarcasmOS es arrancar la app web local.

Desde `AI/Workflow/SarcasmOS-web`:

```bat
start-all.bat
```

En macOS/Linux:

```bash
./start-all.sh
```

Esto levanta:

- Frontend: `http://localhost:5173`
- Backend: `http://localhost:8001`

Tambien se pueden arrancar por separado:

```bash
python -m http.server 5173
python -m uvicorn backend.app:app --host 0.0.0.0 --port 8001
```

## Configuracion

El backend lee variables de entorno desde archivos `.env`, especialmente en:

- `AI/Workflow/SarcasmOS-web/backend/.env`
- `AI/Workflow/.env`

Variables importantes:

- `HACK_CLUB_AI_KEY`, o claves separadas para los proveedores usados.
- `OPENROUTER_API_TOKEN` para el modelo de lenguaje.
- `REPLICATE_API_TOKEN` para STT/TTS si se usa Replicate.
- `MINIMAX_VOICE_ID` para la voz TTS.
- `FFMPEG_PATH` opcional si `ffmpeg` no esta en el PATH.

Mas detalle tecnico en:

- `AI/Workflow/SarcasmOS-web/README.md`
- `AI/Workflow/SarcasmOS-web/PROJECT_OVERVIEW.md`

## Arquitectura

```text
Microfono / texto
      |
      v
Frontend web
      |
      v
FastAPI backend
      |
      +--> STT: audio a texto
      +--> LLM: respuesta con personalidad
      +--> TTS: respuesta hablada
      +--> Historial local en JSON
      |
      v
Audio + cara animada + futuras expresiones hardware
```

## Hardware

El objetivo fisico es una cabeza robotica expresiva:

- Ojos con displays redondos o modulos visuales dedicados.
- Boca con display/placa separada para animacion.
- Carcasa diseñada en FreeCAD.
- PCBs KiCad para separar el sistema visual en modulos fabricables.

Los archivos de produccion de PCB estan en `PCB/*/production`, incluyendo BOM, posiciones, netlists y ZIPs de fabricacion.

## Notas De Desarrollo

- La app web principal no requiere framework frontend; usa HTML, CSS y JS estatico.
- El backend usa Python + FastAPI.
- El historial y los audios generados se guardan localmente en `backend/outputs`.
- Los audios subidos se guardan en `backend/uploads`.
- Algunas carpetas contienen pruebas, audios generados y experimentos que documentan el proceso, no una API estable.

## Roadmap

- Integrar el control real de pantallas de ojos y boca.
- Pulir la personalidad y el prompt del asistente.
- Mejorar limpieza de audios generados e historial.
- Documentar montaje de hardware.
- Separar mejor archivos fuente, artefactos generados y backups de fabricacion.
- Preparar una demo reproducible para presentacion.

## Creditos

Proyecto creado como experimento de asistente fisico con IA para Hack Club / Fallout. SarcasmOS no pretende ser un asistente educado: pretende ser memorable.
