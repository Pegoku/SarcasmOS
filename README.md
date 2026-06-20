# SarcasmOS

SarcasmOS is a personality-first voice assistant.
The core idea is simple: take a smart assistant like Alexa or Google Home, then give it a face, a voice, and most importantly, a personality. Instead of answering like a neutral corporate box, SarcasmOS replies in Spanish with dry humor, sarcasm, and animated expressions inspired by [Bender](https://futurama.fandom.com/wiki/Bender).

This project focusses on the software and AI side of the assistant, but it also includes various hardware elements, like multiple PCBs, sensors, motors, and quite a bit of 3D printing.
The goal is to have a memorable, sarcastic, and show-like assistant that can be used both for fun and real tasks like controlling smart home devices, playing music reviewing your agenda, or answering questions.

## Why Build This?

Futurama is a classic tv show we love (and you should too). Bender is the most memorable character, and his sarcastic personality is a perfect fit for a voice assistant.
We wanted to build a voice assistant that is not just a plain, neutral box, but one that has a personality and you won't just use it to get things done, but also to have fun and be entertained 


## Images

![SarcasmOS Poster](media/SarcasmOS.png)



<details>
  <summary>CAD - click to expand</summary>
  
![CAD](media/CAD.png)

</details>

### Brain PCB

![3d brain pcb](media/3d-brain.png)

<details>
  <summary>Brain PCB schematic - click to expand</summary>
  
![Brain PCB schematic](media/brain-schematic.png)

</details>

<details>
  <summary>Brain PCB layout - click to expand</summary>

![Brain PCB layout](media/brain-layout.png)

</details>

### Eye PCB

![3d eye pcb](media/3d-eye.png)

<details>
  <summary>Eye PCB schematic - click to expand</summary>
  
![Eye PCB schematic](media/eye-schematic.png)

</details>

<details>
  <summary>Eye PCB layout - click to expand</summary>

![Eye PCB layout](media/eye-layout.png)

</details>

### Mouth PCB

![3d mouth pcb](media/3d-mouth.png)

<details>
  <summary>Mouth PCB schematic - click to expand</summary>

![Mouth PCB schematic](media/mouth-schematic.png)

</details>

<details>
  <summary>Mouth PCB layout - click to expand</summary>
  
![Mouth PCB layout](media/mouth-layout.png)

</details>


## Language Scope

SarcasmOS is designed to work in Spanish only.

The language model prompt, personality, expected user input, and generated responses are all tuned for Spanish. Other languages may work accidentally depending on the upstream model, but they are outside the intended behavior of this project.

## Features

- Listens to speech input.
- Transcribes speech to text.
- Generates Spanish answers with a sarcastic character voice.
- Converts the answer back to speech.
- Syncs audio playback with an animated face.
- Stores memory of past interactions.
- Has a web interface for chat, audio, history and configuration.

## Current State

SarcasmOS already has a working software demo available [here](https://sarcasmos.pegoku.com/):

- Static web UI in `Code/AI/Workflow/SarcasmOS-web`.
- Local FastAPI backend with chat, audio, history, and status endpoints.
- STT, LLM, and TTS pipeline using configurable external services.
- Console view, full face view, and voice chat view.
- CAD files for the head/body.
- KiCad designs for the eye and mouth boards.
- Voice synthesis/cloning experiments and clip preparation tools.


The hardware firmware is still not finished, but a minimal prototype is already available at [Code/(Brain, Eye, Mouth)](Code) respectively for each PCB.


## Repository Structure

```text
Code/AI/Workflow/SarcasmOS-web/   Main web app + backend
  Piannote/                 Voice clip separation and preparation
  Qwen3-TTS-Bender/         TTS / voice experiments
  xtts/                     Alternative TTS tests

CAD/                        FreeCAD models for the head, base, and parts
PCB/
  eye/                      Eye PCB
  mouth/                    Mouth PCB

animationVisualizer/        Animation prototype
README.md                   This document
```

## Web Demo

The quickest way to try SarcasmOS is to run the local web app.

From `AI/Workflow/SarcasmOS-web`:

```bat
start-all.bat
```

On macOS/Linux:

```bash
./start-all.sh
```

This starts:

- Frontend: `http://localhost:5173`
- Backend: `http://localhost:8001`

You can also start them separately:

```bash
python -m http.server 5173
python -m uvicorn backend.app:app --host 0.0.0.0 --port 8001
```

## Configuration

The backend reads environment variables from `.env` files, especially:

- `AI/Workflow/SarcasmOS-web/backend/.env`
- `AI/Workflow/.env`

Important variables:

- `HACK_CLUB_AI_KEY`, or separate provider keys.
- `OPENROUTER_API_TOKEN` for the language model.
- `REPLICATE_API_TOKEN` for STT/TTS if using Replicate.
- `MINIMAX_VOICE_ID` for the TTS voice.
- `FFMPEG_PATH` if `ffmpeg` is not available on PATH.

More technical detail:

- `AI/Workflow/SarcasmOS-web/README.md`
- `AI/Workflow/SarcasmOS-web/PROJECT_OVERVIEW.md`

## Architecture

```text
Microphone / text
      |
      v
Web frontend
      |
      v
FastAPI backend
      |
      +--> STT: speech to text
      +--> LLM: Spanish personality response
      +--> TTS: spoken answer
      +--> Local JSON history
      |
      v
Audio + animated face + future hardware expressions
```

## Hardware

The physical goal is an expressive robotic head:

- Eyes using round displays or dedicated visual modules.
- Mouth with a separate display/board for animation.
- Enclosure designed in FreeCAD.
- KiCad PCBs for splitting the visual system into manufacturable modules.

PCB production files live in `PCB/*/production`, including BOMs, placement files, netlists, and fabrication ZIPs.

## Development Notes

- The main web app does not use a frontend framework; it is static HTML, CSS, and JS.
- The backend uses Python + FastAPI.
- Generated audio and history are stored locally in `backend/outputs`.
- Uploaded audio is stored in `backend/uploads`.
- Some folders contain experiments, generated audio, and process artifacts rather than stable APIs.

## Roadmap

- Integrate real display control for the eyes and mouth.
- Polish the Spanish personality prompt.
- Improve generated audio and history cleanup.
- Document the hardware assembly.
- Separate source files, generated artifacts, and fabrication backups more cleanly.
- Prepare a reproducible presentation demo.

## Credits

Built as a physical AI assistant experiment for Hack Club / Fallout. SarcasmOS is not trying to be polite. It is trying to be memorable.
