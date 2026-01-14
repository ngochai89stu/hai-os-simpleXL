#!/usr/bin/env bash
# Build script for SimpleXL web demo using emscripten
echo "Building SimpleXL Web Demo (Emscripten)…"
set -e
EMSCRIPTEN_PATH=${EMSDK:-$HOME/emsdk}/upstream/emscripten
if [ ! -d "$EMSCRIPTEN_PATH" ]; then
  echo "Emscripten not found. Install EMSDK and activate first."
  exit 1
fi

echo "Running emcmake to configure…"
mkdir -p build && cd build
emcmake cmake ..
cmake --build . -j$(nproc)

# Copy output to parent directory
cp ui_demo.js ui_demo.wasm ../../web_demo/
cd ../../web_demo

echo "Build finished → web_demo/ui_demo.js / ui_demo.wasm"
