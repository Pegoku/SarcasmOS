import argparse
import torch
import soundfile as sf
from qwen_tts import Qwen3TTSModel


def main():
    parser = argparse.ArgumentParser(description="Qwen3-TTS voice clone")
    parser.add_argument("text", help="Text to synthesize")
    parser.add_argument("--ref-audio", default="Clips/bender-voz7.wav", help="Reference audio path")
    parser.add_argument(
        "--ref-text",
        default="Fue horrible. Probé ayudándoles, probé no ayudándoles. Pero al final no logré hacerles ningún bien. ¿Crees que estuvo mal lo que hice?",
        help="Exact transcript of the reference audio",
    )
    parser.add_argument("--output", default="bender-es.wav", help="Output WAV filename")
    parser.add_argument("--model", default="./Qwen3-TTS-12Hz-0.6B-Base", help="Local model path")
    parser.add_argument("--language", default="Spanish", help="Target language")

    args = parser.parse_args()

    tts = Qwen3TTSModel.from_pretrained(
        args.model,
        device_map="cuda:0",
        dtype=torch.float16,
    )

    wavs, sr = tts.generate_voice_clone(
        text=args.text,
        language=args.language,
        ref_audio=args.ref_audio,
        ref_text=args.ref_text,
    )

    sf.write(args.output, wavs[0], sr)
    print(f"Saved to {args.output}")


if __name__ == "__main__":
    main()
