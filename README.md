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

SarcasmOS is designed to work in Spanish first.

The language model prompt, personality, expected user input, and generated responses are all tuned for Spanish. Other languages may work accidentally depending on the upstream model, but they are outside the intended behavior of this project.

## Features

- Listens to speech input.
- Transcribes speech to text.
- Generates Spanish answers with a sarcastic character voice.
- Converts the answer back to speech.
- Syncs audio playback with an animated face.
- Stores memory of past interactions.
- Has a web interface for chat, audio, history and configuration.
- Has integrated speaker and microphone.
- Custom animations for the eyes and mouth.
- _8k mouth display_ (128x64 pixels = 8192 pixels).

## BOM

> It is recommended to use the online [google spreadsheet](https://docs.google.com/spreadsheets/d/1oUawWcxpkl1FgnUkY16h-4gWlRbnDHgNzrJ1ZsR5Gkk/edit?usp=sharing) for the most up to date BOM with links and prices.

<details>
  <summary>Click to expand the BOM table</summary>

| Item                | Desc                   | Qtty needed | Qtty / Pack | Pack Qtty | Price / pack | Source                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            | Extra                                                                                                                | Category           | Already owned | In Cart | Purchased | Item Total | Final Total | Final Total ($) |
| ------------------- | ---------------------- | ----------- | ----------- | --------- | ------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------- | ------------------ | ------------- | ------- | --------- | ---------- | ----------- | --------------- |
| Round Display       | Eyes                   | 2           | 2           | 1         | 6.55€        | https://es.aliexpress.com/item/1005006928550245.html?spm=a2g0o.detail.0.0.3380vPbLvPbLqx&mp=1&pdp_npi=6%40dis%21EUR%21EUR+6.99%21EUR+6.99%21%21EUR+6.85%21%21%21%400b88a95617788756053276236e0e02%2112000045970082989%21ct%21NL%213560368449%21%211%210%21&gatewayAdapt=nld2esp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |                                                                                                                      | Eyes               | FALSE         | TRUE    | FALSE     | 6.55€      | 269.90€     | $309.64         |
| Display Lens        | Make Eyes Bigger       | 2           | 1           | 2         | 19.69€       | https://es.aliexpress.com/item/1005008724449296.html?spm=a2g0o.detail.0.0.6a0760d1eKUq5E&mp=1&pdp_npi=6%40dis%21EUR%21EUR%2020.73%21EUR%2019.69%21%21EUR%2019.69%21%21%21%4021038a8017819666708386757e11aa%2112000046404933342%21ct%21ES%213560368449%21%211%210%21&gatewayAdapt=glo2esp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |                                                                                                                      | Eyes               | FALSE         | FALSE   | FALSE     | 39.38€     |             |                 |
| Flexible LED Matrix | Mouth                  | 1           | 2           | 1         | 39.56€       | https://es.aliexpress.com/item/1005006068032826.html?spm=a2g0o.productlist.main.21.139eA4CxA4Cx4e&algo_pvid=5b077f7f-89aa-40ed-b4d1-d402daa6e2e7&pdp_ext_f=%7B%22order%22%3A%225%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005006068032826%7C_p_origin_prod%3A&gatewayAdapt=nld2esp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                |                                                                                                                      | Mouth              | FALSE         | FALSE   | FALSE     | 39.56€     |             |                 |
| Microphone module   | Fallback PCB           | 1           | 1           | 1         | 6.30€        | https://es.aliexpress.com/item/1005009698505728.html?spm=a2g0o.cart.0.0.6c4d38daLbAysv&mp=1&pdp_npi=6%40dis%21EUR%21EUR+14.93%21EUR+6.24%21%21EUR+6.24%21%21%21%400b8848e317816378509391522e0e93%2112000049889234983%21ct%21ES%213560368449%21%211%210%21&gatewayAdapt=glo2esp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |                                                                                                                      | Brain              | FALSE         | FALSE   | FALSE     | 6.30€      |             |                 |
| Ethernet module     | Fallback PCB           | 1           | 1           | 1         | 4.35€        | https://es.aliexpress.com/item/1005009204216561.html?spm=a2g0o.detail.pcDetailBottomMoreOtherSeller.3.6e6eUfXQUfXQ5C&gps-id=pcDetailBottomMoreOtherSeller&scm=1007.40050.354490.0&scm_id=1007.40050.354490.0&scm-url=1007.40050.354490.0&pvid=3570e6de-c66e-4314-adfb-729cb482450e&_t=gps-id%3ApcDetailBottomMoreOtherSeller%2Cscm-url%3A1007.40050.354490.0%2Cpvid%3A3570e6de-c66e-4314-adfb-729cb482450e%2Ctpp_buckets%3A668%232846%238114%231999&pdp_ext_f=%7B%22order%22%3A%22295%22%2C%22spu_best_type%22%3A%22price%22%2C%22eval%22%3A%221%22%2C%22sceneId%22%3A%2230050%22%2C%22fromPage%22%3A%22recommend%22%7D&pdp_npi=6%40dis%21EUR%214.69%214.69%21%21%215.33%215.33%21%402103973d17801598614361290e0f38%2112000048290449884%21rec%21NL%213560368449%21X%211%210%21n_tag%3A-29919%3Bd%3Adc2a0b43%3Bm03_new_user%3A-29895&utparam-url=scene%3ApcDetailBottomMoreOtherSeller%7Cquery_from%3A%7Cx_object_id%3A1005009204216561%7C_p_origin_prod%3A&gatewayAdapt=nld2esp#nav-specification |                                                                                                                      | Brain              | FALSE         | FALSE   | FALSE     | 4.35€      |             |                 |
| 18650 cells         |                        | 14          | 1           | 14        | 0.00€        | Already owned                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |                                                                                                                      | Brain              | TRUE          | FALSE   | FALSE     | 0          |             |                 |
| cable               |                        | 1           | 1           | 1         | 0.00€        | Already owned                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |                                                                                                                      | Brain              | FALSE         | FALSE   | FALSE     | 0.00€      |             |                 |
| mmWave sensor       |                        | 1           | 1           | 1         | 4.79€        | https://es.aliexpress.com/item/1005006121923099.html?spm=a2g0o.detail.0.0.791e7F4F7F4Fcg&mp=1&pdp_npi=6%40dis%21EUR%21EUR%204.92%21EUR%204.89%21%21EUR%204.89%21%21%21%402103842a17788752854925679e1108%2112000035851121206%21ct%21NL%213560368449%21%211%210%21&gatewayAdapt=nld2esp                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |                                                                                                                      | Brain              | TRUE          | FALSE   | FALSE     | 0          |             |                 |
| Stepper Motor       | Nema17 17HS4023 42BGYH | 1           | 1           | 1         | 5.19€        | https://es.aliexpress.com/item/1005003874936862.html?spm=a2g0o.productlist.main.2.45ac6b6d4Ty51H&algo_pvid=24c1815d-4fc5-48d3-8530-a3506b98d7ee&pdp_ext_f=%7B%22order%22%3A%227968%22%2C%22spu_best_type%22%3A%22price%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005003874936862%7C_p_origin_prod%3A                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |                                                                                                                      | Brain              | FALSE         | FALSE   | FALSE     | 5.19€      |             |                 |
| Antenna             | For esp32              | 1           | 1           | 1         | 1.84€        | https://es.aliexpress.com/item/1005007468291772.html                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |                                                                                                                      | Brain              | FALSE         | FALSE   | FALSE     | 1.84€      |             |                 |
| Speakers            | Speakers               | 1           | 2           | 1         | 5.47€        | https://es.aliexpress.com/item/1005008326457654.html?spm=a2g0o.productlist.main.3.25f622deCmRsMj&algo_pvid=bae0d8d2-8cd9-44f2-b28e-96956b2241be&algo_exp_id=bae0d8d2-8cd9-44f2-b28e-96956b2241be-2&pdp_ext_f=%7B%22order%22%3A%22415%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&pdp_npi=6%40dis%21EUR%215.66%215.49%21%21%2143.88%2142.56%21%402103835c17752449159045745eacb9%2112000044613535197%21sea%21NL%213560368449%21X%211%210%21n_tag%3A-29919%3Bd%3Adc2a0b43%3Bm03_new_user%3A-29895&curPageLogUid=PrDyQ5hqmN9h&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005008326457654%7C_p_origin_prod%3A&gatewayAdapt=nld2esp                                                                                                                                                                                                                                                                                                                                   |                                                                                                                      | Brain              | FALSE         | FALSE   | FALSE     | 5.47€      |             |                 |
| LCSC components     | Components             | 1           | 1           | 1         | 93.44€       | https://www.lcsc.com                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              | https://docs.google.com/spreadsheets/d/1oUawWcxpkl1FgnUkY16h-4gWlRbnDHgNzrJ1ZsR5Gkk/edit?gid=283679690#gid=283679690 | Brain, Mouth, Eyes | FALSE         | FALSE   | FALSE     | 93.44€     |             |                 |
| LCSC Shipping       | Components             | 1           | 1           | 1         | 14.68€       | https://www.lcsc.com                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |                                                                                                                      | Brain, Mouth, Eyes | FALSE         | FALSE   | FALSE     | 14.68€     |             |                 |
| JLCPCB PCBs         | PCBs                   | 1           | 1           | 1         | 22.17€       | http://jlcpcb.com                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |                                                                                                                      | Brain, Mouth, Eyes | FALSE         | FALSE   | FALSE     | 22.17€     |             |                 |
| JLCPCB Shipping     | PCBs                   | 1           | 1           | 1         | 30.97€       | http://jlcpcb.com                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | THIS IS THE CHEAPEST OPTION                                                                                          | Brain, Mouth, Eyes | FALSE         | FALSE   | FALSE     | 30.97€     |             |                 |

</details>

## Current State

SarcasmOS already has a working web demo available [here](https://sarcasmos.pegoku.com/):

- Static web UI in `Code/AI/Workflow/SarcasmOS-web`.
- Local FastAPI backend with chat, audio, history, and status endpoints.
- STT, LLM, and TTS pipeline using configurable external services.
- Console view, full face view, and voice chat view.
- CAD files for the head/body.
- KiCad designs for the eye and mouth boards.
- Voice synthesis/cloning experiments and clip preparation tools.

The hardware firmware is still not finished, but a minimal prototype is already available at [Code/(Brain, Eye, Mouth)](Code) respectively for each PCB.

## Web Demo

For the already setup web demo, visit [https://sarcasmos.pegoku.com/](https://sarcasmos.pegoku.com/).

For the best experience and no ratelimiting, run the local web app.

From `Code/AI/Workflow/SarcasmOS-web`:

```bat
start-all.bat
```

On macOS/Linux:

```bash
./start-all.sh
```

This starts:

- Proxy (combines all services into 1 port for easy access): `http://localhost:9000`
- Frontend: `http://localhost:5173`
- Backend: `http://localhost:8001`

You can also start them separately:

```bash
python -m http.server 5173
python -m uvicorn backend.app:app --host 0.0.0.0 --port 8001
```

## Configuration

The backend reads environment variables from `.env` files, especially:

- `Code/AI/Workflow/SarcasmOS-web/backend/.env`
- `Code/AI/Workflow/.env`

**Important variables**:

- `HACK_CLUB_AI_KEY`, or separate provider keys.
- `OPENROUTER_API_TOKEN` for the language model.
- `REPLICATE_API_TOKEN` for STT/TTS if using Replicate.
- `MINIMAX_VOICE_ID` for the TTS voice.
- `FFMPEG_PATH` if `ffmpeg` is not available on PATH.

More technical detail:

- `Code/AI/Workflow/SarcasmOS-web/README.md`
- `Code/AI/Workflow/SarcasmOS-web/PROJECT_OVERVIEW.md`

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
      +--> LLM: Spanish personality response + tools (e.g. google calendar)
      +--> TTS: spoken answer
      +--> Local JSON history
      |
      v
Audio + animated face + future hardware expressions
```

## Credits

Built as a physical AI assistant project for Hack Club / Fallout.
SarcasmOS is not trying to be polite. It is trying to be memorable.
