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


def get_gpu_dtype(requested_dtype: str):
    if not torch.cuda.is_available():
        raise RuntimeError("GPU not available. Start the ROCm container with /dev/kfd and /dev/dri exposed.")

    if requested_dtype == "float16":
        return torch.float16

    if requested_dtype == "bfloat16":
        return torch.bfloat16

    if requested_dtype == "float32":
        return torch.float32

    if hasattr(torch.cuda, "is_bf16_supported") and torch.cuda.is_bf16_supported():
        return torch.bfloat16

    return torch.float16


def main():
    parser = argparse.ArgumentParser(
        description="Clone speech using AMD GPU acceleration via ROCm"
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
        "--dtype",
        default="float16",
        choices=["auto", "float16", "bfloat16", "float32"],
        help="Model dtype. float16 is the safest default on AMD.",
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

    dtype = get_gpu_dtype(args.dtype)
    print(f"Loading model on cuda:0 with dtype {dtype}")

    tts = Qwen3TTSModel.from_pretrained(
        args.model,
        device_map="cuda:0",
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
