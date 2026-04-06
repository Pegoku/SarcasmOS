import torch
import soundfile as sf
from qwen_tts import Qwen3TTSModel

device = "cuda:0"

tts = Qwen3TTSModel.from_pretrained(
    "./Qwen3-TTS-12Hz-0.6B-Base",
    device_map="cpu",
    dtype=torch.float32

#    "./Qwen3-TTS-12Hz-0.6B-Base",
#    device_map=device,
#    dtype=torch.bfloat16,   # use float16 if bfloat16 gives issues on your GPU
    # attn_implementation="flash_attention_2",  # optional, only if installed and supported
)

ref_audio = "Clips/bender-voz7.wav"
ref_text = "Fue horrible. Probé ayudándoles, probé no ayudándoles. Pero al final no logré hacerles ningún bien. ¿Crees que estuvo mal lo que hice?"

wavs, sr = tts.generate_voice_clone(
    text="¡Besa mi brillante trasero metálico!",
    language="Spanish",
    ref_audio=ref_audio,
    ref_text=ref_text,
)

sf.write("bender-es.wav", wavs[0], sr)
