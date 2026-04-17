#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <project-root> <output-lib>" >&2
  exit 1
fi

PROJECT_ROOT=$1
OUTPUT_LIB=$2
SDK_DIR="$PROJECT_ROOT/third_party/lzma-sdk-26.00/CPP/7zip/Bundles/Alone7z"
OBJ_DIR="$SDK_DIR/_o"

mkdir -p "$(dirname "$OUTPUT_LIB")"

make -f makefile.gcc -C "$SDK_DIR"

rm -f "$OUTPUT_LIB"

objs=()
for file in "$OBJ_DIR"/*.o; do
  base=$(basename "$file")
  if [[ "$base" == "MainAr.o" ]]; then
    continue
  fi
  objs+=("$file")
done

if [[ ${#objs[@]} -eq 0 ]]; then
  echo "no SDK object files found in $OBJ_DIR" >&2
  exit 1
fi

ar rcs "$OUTPUT_LIB" "${objs[@]}"
