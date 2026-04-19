import argparse
import glob
from pathlib import Path

import soundfile as sf
import torch
from qwen_tts import Qwen3TTSModel


DEFAULT_TEXT = (
    "Escucha, saco de carne: soy Bender, doblador, bebedor profesional y robot "
    "superior. ¿Qué quieres? Habla rápido, que mi batería no se va a cargar sola… "
    "y no pienso hacerlo gratis"
)


def resolve_runtime(requested_device: str):
    if requested_device == "auto":
        if torch.cuda.is_available():
            bf16_supported = (
                hasattr(torch.cuda, "is_bf16_supported")
                and torch.cuda.is_bf16_supported()
            )
            return "cuda:0", torch.bfloat16 if bf16_supported else torch.float16
        return "cpu", torch.float32

    if requested_device.startswith("cuda"):
        bf16_supported = (
            hasattr(torch.cuda, "is_bf16_supported") and torch.cuda.is_bf16_supported()
        )
        return requested_device, torch.bfloat16 if bf16_supported else torch.float16

    return requested_device, torch.float32


def resolve_dtype(device: str, requested_dtype: str):
    if requested_dtype == "auto":
        _, dtype = resolve_runtime(device)
        return dtype

    if requested_dtype == "float16":
        return torch.float16

    if requested_dtype == "bfloat16":
        return torch.bfloat16

    return torch.float32


def expand_inputs(inputs: list[str]) -> list[Path]:
    resolved: list[Path] = []
    seen: set[Path] = set()

    for item in inputs:
        matches = [Path(match) for match in glob.glob(item)]
        if not matches:
            matches = [Path(item)]

        for match in matches:
            path = match.expanduser().resolve()
            if path.suffix.lower() != ".wav":
                continue
            if path not in seen:
                resolved.append(path)
                seen.add(path)

    return sorted(resolved)


def chunked(items: list[Path], batch_size: int):
    for index in range(0, len(items), batch_size):
        yield items[index : index + batch_size]


def build_output_path(output_template: str, ref_audio: Path) -> Path:
    filename = ref_audio.name
    stem = ref_audio.stem

    if "{base}" in output_template or "{stem}" in output_template:
        return Path(output_template.format(base=filename, stem=stem))

    output_path = Path(output_template)
    if output_template.endswith(("/", "\\")) or output_path.is_dir():
        return output_path / filename

    if output_path.suffix.lower() == ".wav":
        return output_path.parent / f"{output_path.stem}-{stem}{output_path.suffix}"

    return Path(f"{output_template}{filename}")


def load_ref_text(ref_audio: Path) -> str:
    ref_text_path = ref_audio.with_suffix(".txt")
    if not ref_text_path.is_file():
        raise FileNotFoundError(f"Reference transcript not found: {ref_text_path}")

    ref_text = ref_text_path.read_text(encoding="utf-8").strip()
    if not ref_text:
        raise ValueError(f"Reference transcript is empty: {ref_text_path}")

    return ref_text


def main():
    parser = argparse.ArgumentParser(
        description="Batch voice clone with one model load"
    )
    parser.add_argument(
        "inputs",
        nargs="+",
        help="Reference WAV paths or glob patterns",
    )
    parser.add_argument(
        "--text",
        default=DEFAULT_TEXT,
        help="Text to synthesize for every reference clip",
    )
    parser.add_argument(
        "--output",
        default="testOutputs/test-{base}",
        help="Output template. Supports {base} and {stem}, or use a directory/prefix.",
    )
    parser.add_argument(
        "--language",
        default="Spanish",
        help="Target language",
    )
    parser.add_argument(
        "--model",
        default="./Qwen3-TTS-12Hz-1.7B-Base",
        help="Local model path",
    )
    parser.add_argument(
        "--device",
        default="auto",
        help="Model device: auto, cpu, or cuda:0",
    )
    parser.add_argument(
        "--dtype",
        default="auto",
        choices=["auto", "float16", "bfloat16", "float32"],
        help="Model dtype",
    )
    parser.add_argument(
        "--batch-size",
        type=int,
        default=4,
        help="How many reference clips to synthesize per model call",
    )
    args = parser.parse_args()

    if args.batch_size < 1:
        raise ValueError("--batch-size must be at least 1")

    ref_audios = expand_inputs(args.inputs)
    if not ref_audios:
        raise FileNotFoundError("No WAV files matched the provided inputs")

    missing = [str(path) for path in ref_audios if not path.is_file()]
    if missing:
        raise FileNotFoundError(f"Reference audio not found: {missing[0]}")

    device, auto_dtype = resolve_runtime(args.device)
    dtype = resolve_dtype(device, args.dtype)
    if args.dtype == "auto":
        dtype = auto_dtype

    print(f"Loading model on {device} with dtype {dtype}")
    tts = Qwen3TTSModel.from_pretrained(
        args.model,
        device_map=device,
        dtype=dtype,
    )

    total_written = 0
    for batch_index, batch in enumerate(chunked(ref_audios, args.batch_size), start=1):
        ref_texts = [load_ref_text(path) for path in batch]
        prompt_items = tts.create_voice_clone_prompt(
            ref_audio=[str(path) for path in batch],
            ref_text=ref_texts,
        )
        wavs, sr = tts.generate_voice_clone(
            text=[args.text] * len(batch),
            language=[args.language] * len(batch),
            voice_clone_prompt=prompt_items,
        )

        print(f"Writing batch {batch_index} with {len(batch)} file(s)")
        for ref_audio, wav in zip(batch, wavs):
            output_path = build_output_path(args.output, ref_audio)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            sf.write(output_path, wav, sr)
            total_written += 1
            print(f"Saved {output_path}")

    print(f"Done. Generated {total_written} file(s).")


if __name__ == "__main__":
    main()
