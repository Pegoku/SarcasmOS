import argparse
import importlib.util
from pathlib import Path

import soundfile as sf
import torch
from qwen_tts import Qwen3TTSModel


DEFAULT_TEXT = (
    "Escucha, saco de carne: soy Bender, doblador, bebedor profesional y robot "
    "superior. ¿Qué quieres? Habla rápido, que mi batería no se va a cargar sola… "
    "y no pienso hacerlo gratis"
)


def get_gpu_dtype():
    if not torch.cuda.is_available():
        raise RuntimeError("GPU not available. Start the ROCm container with /dev/kfd and /dev/dri exposed.")

    if hasattr(torch.cuda, "is_bf16_supported") and torch.cuda.is_bf16_supported():
        return torch.bfloat16

    return torch.float16


def get_model_load_kwargs():
    kwargs = {
        "device_map": "cuda:0",
        "dtype": get_gpu_dtype(),
    }

    if importlib.util.find_spec("flash_attn") is not None:
        kwargs["attn_implementation"] = "flash_attention_2"

    return kwargs


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

    load_kwargs = get_model_load_kwargs()
    print(
        "Loading model on cuda:0 with "
        f"dtype {load_kwargs['dtype']} and "
        f"attention {load_kwargs.get('attn_implementation', 'manual_pytorch')}"
    )

    tts = Qwen3TTSModel.from_pretrained(args.model, **load_kwargs)

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
