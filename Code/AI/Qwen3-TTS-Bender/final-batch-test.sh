#!/bin/bash

INPUT_DIR="Clips/Good"
OUTPUT_ROOT="final-testing-bender"
TEXT_FILE="frases.txt"

# Crear carpeta principal
mkdir -p "$OUTPUT_ROOT"

for f in "$INPUT_DIR"/bender-voz*.wav; do
  base=$(basename "$f" .wav)
  outdir="$OUTPUT_ROOT/$base"

  # Crear carpeta para este audio
  mkdir -p "$outdir"

  i=1
  while IFS= read -r line; do
    # Saltar líneas vacías
    [ -z "$line" ] && continue

    python vclone.py "$f" \
      --text "$line" \
      --output "$outdir/frase-$i.wav"

    ((i++))
  done < "$TEXT_FILE"

done
