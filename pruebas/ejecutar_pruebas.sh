#!/usr/bin/env bash
# Compila el banco de pruebas para el sketch original y el corregido.
# Uso: ./ejecutar_pruebas.sh [directorio_salida]
set -e
cd "$(dirname "$0")"
OUT="${1:-./bin}"
mkdir -p "$OUT"
FLAGS="-O2 -std=c++17 -Wall -Imock"
g++ $FLAGS -Wno-unused-variable -DSKETCH='"../referencia/codigo_original/codigo_original.ino"' simulador.cpp -o "$OUT/sim_original"
g++ $FLAGS -DSKETCH='"../medidor_rms_frecuencia/medidor_rms_frecuencia.ino"' simulador.cpp -o "$OUT/sim_corregido"
g++ $FLAGS -DMODO_RMS_VERDADERO=1 -DSKETCH='"../medidor_rms_frecuencia/medidor_rms_frecuencia.ino"' simulador.cpp -o "$OUT/sim_rms_verdadero"
g++ $FLAGS -DSKETCH='"../medidor_minimo/medidor_minimo.ino"' simulador.cpp -o "$OUT/sim_minimo"
echo "Compilado en $OUT"
