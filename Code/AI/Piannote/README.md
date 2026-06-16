# Piannote Setup

This folder contains a `pyannote.audio`-based script that splits one audio file into one output WAV per detected speaker.

## Files

- `split_speakers.py`: speaker diarization and export script
- `environment.cpu.yml`: reproducible CPU conda environment
- `environment.nvidia.yml`: reproducible NVIDIA GPU conda environment
- `environment.amd.yml`: reproducible AMD ROCm GPU conda environment

## CPU Install

```bash
conda env create -f environment.cpu.yml
conda activate piannote311cpu
```

## NVIDIA GPU Install

This environment is for NVIDIA systems and pins the CUDA 12.4 PyTorch builds from `defaults`.

Requirements:

- NVIDIA GPU
- NVIDIA driver compatible with CUDA 12.4 runtime

```bash
conda env create -f environment.nvidia.yml
conda activate piannote311nvidia
```

## AMD GPU Install

This environment is for AMD ROCm systems and installs PyTorch ROCm 6.2.4 wheels through `pip` inside conda.

Requirements:

- AMD GPU supported by ROCm
- ROCm 6.2.x compatible host system and drivers

```bash
conda env create -f environment.amd.yml
conda activate piannote311amd
```

## Hugging Face Access

Log in once in the target environment:

```bash
hf auth login
```

Accept the gated model terms in a browser:

- `https://hf.co/pyannote/speaker-diarization-3.1`
- `https://hf.co/pyannote/segmentation-3.0`

## Run

```bash
python split_speakers.py /path/to/input.wav
```

Optional example:

```bash
python split_speakers.py /path/to/input.wav --min-speakers 2 --max-speakers 4
```

## Output

The script writes:

- one WAV per detected speaker in `speaker_splits/`
- one JSON manifest with segment timings

## Linux Note

On one Linux machine, `torch` failed to import because `libtorch_cpu.so` requested an executable stack. If that happens again, run this once inside the created env path:

```bash
patchelf --clear-execstack "$CONDA_PREFIX/lib/python3.11/site-packages/torch/lib/libtorch_cpu.so"
```

Then retry the import or script.
