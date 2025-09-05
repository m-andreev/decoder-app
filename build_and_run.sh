#!/usr/bin/env bash
set -euo pipefail

SRC="parser.cpp"
EXE="parser"

if [[ ! -f "$SRC" ]]; then
  echo "Source file $SRC not found."
  exit 1
fi

# Опитай g++, после clang++
if command -v g++ >/dev/null 2>&1; then
  echo "Building with g++..."
  g++ -std=gnu++17 -O2 -o "$EXE" "$SRC"
elif command -v clang++ >/dev/null 2>&1; then
  echo "Building with clang++..."
  clang++ -std=c++17 -O2 -o "$EXE" "$SRC"
else
  echo "No C++ compiler found. Please install g++ or clang++."
  exit 1
fi

if [[ ! -f transcript.txt ]]; then
  echo "Input file transcript.txt not found next to the script."
  exit 1
fi

echo "Running..."
./"$EXE" transcript.txt > output.txt

echo "Done. Results saved to output.txt"
