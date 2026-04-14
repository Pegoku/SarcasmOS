import argparse
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
            bf16_supported = hasattr(torch.cuda, "is_bf16_supported") and torch.cuda.is_bf16_supported()
            return "cuda:0", torch.bfloat16 if bf16_supported else torch.float16
        return "cpu", torch.float32

    if requested_device.startswith("cuda"):
        bf16_supported = hasattr(torch.cuda, "is_bf16_supported") and torch.cuda.is_bf16_supported()
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


def main():
    parser = argparse.ArgumentParser(
        description="Clone speech using a reference audio file and matching transcript"
    )
    parser.add_argument(
        "ref_audio",
        help="Reference audio path. The transcript must exist as the same filename with .txt",
    )
    parser.add_argument(
        "--text",
        default=DEFAULT_TEXT,
        help="Text to synthesize",
    )
    parser.add_argument(
        "--output",
        default="bender-es.wav",
        help="Output WAV filename",
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
    args = parser.parse_args()

    ref_audio = Path(args.ref_audio)
    ref_text_path = ref_audio.with_suffix(".txt")

    if not ref_audio.is_file():
        raise FileNotFoundError(f"Reference audio not found: {ref_audio}")

    if not ref_text_path.is_file():
        raise FileNotFoundError(f"Reference transcript not found: {ref_text_path}")

    ref_text = ref_text_path.read_text(encoding="utf-8").strip()
    if not ref_text:
        raise ValueError(f"Reference transcript is empty: {ref_text_path}")

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

    wavs, sr = tts.generate_voice_clone(
        text=args.text,
        language=args.language,
        ref_audio=str(ref_audio),
        ref_text=ref_text,
    )

    sf.write(args.output, wavs[0], sr)
    print(f"Saved to {args.output}")


if __name__ == "__main__":
    main()
