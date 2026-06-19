import torch
import soundfile as sf
from qwen_tts import Qwen3TTSModel

device = "cuda:0"

tts = Qwen3TTSModel.from_pretrained(
    "./Qwen3-TTS-12Hz-1.7B-Base",
    device_map="cpu",
    dtype=torch.float32

#    "./Qwen3-TTS-12Hz-0.6B-Base",
#    device_map=device,
#    dtype=torch.bfloat16,   # use float16 if bfloat16 gives issues on your GPU
    # attn_implementation="flash_attention_2",  # optional, only if installed and supported
)

ref_audio = "Clips/Good/bender-voz3.wav"
ref_text = "¿Pero por qué iba a pensar Dios en binario? ¡A no ser, que no seas Dios sino los restos de una sonda espacial informatizada que colisionó con Dios!"

wavs, sr = tts.generate_voice_clone(
    text="Escucha, saco de carne: soy Bender, doblador, bebedor profesional y robot superior. ¿Qué quieres? Habla rápido, que mi batería no se va a cargar sola… y no pienso hacerlo gratis",
    language="Spanish",
    ref_audio=ref_audio,
    ref_text=ref_text,
)

sf.write("bender-es.wav", wavs[0], sr)
