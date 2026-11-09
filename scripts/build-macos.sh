#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "$(uname -s)" != Darwin ]; then
  echo 'Run this script on a Mac with Xcode and CMake installed.' >&2
  exit 1
fi
cmake -S . -B build-macos -G Xcode '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build-macos --config Release --parallel 4
for bundle in build-macos/Sixth_artefacts/Release/{AU/Sixth.component,VST3/Sixth.vst3,Standalone/Sixth.app}; do
  lipo -verify_arch arm64 x86_64 "$bundle/Contents/MacOS/Sixth"
  codesign --force --deep --sign - "$bundle"
  codesign --verify --deep --strict "$bundle"
done
ctest --test-dir build-macos -C Release --output-on-failure
bash scripts/package-macos.sh build-macos
printf '\nPlugins: %s/build-macos/Sixth_artefacts/Release/{AU,VST3}\n' "$PWD"
