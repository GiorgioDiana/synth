#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "$(uname -s)" != Darwin ]; then
  echo 'Run this script on a Mac with Xcode and CMake installed.' >&2
  exit 1
fi
bash scripts/ci-check.sh 'Configure universal AU and VST3' cmake -S . -B build-macos -G Xcode '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
bash scripts/ci-check.sh 'Build macOS binaries' cmake --build build-macos --config Release --parallel 4
for bundle in build-macos/Sixth_artefacts/Release/{AU/Sixth.component,VST3/Sixth.vst3,Standalone/Sixth.app}; do
  bash scripts/ci-check.sh "$bundle universal verification" lipo "$bundle/Contents/MacOS/Sixth" -verify_arch arm64 x86_64
  bash scripts/ci-check.sh "$bundle signing" codesign --force --deep --sign - "$bundle"
  bash scripts/ci-check.sh "$bundle signature verification" codesign --verify --deep --strict "$bundle"
done
bash scripts/ci-check.sh 'Audio, MIDI, state and VST3 tests' ctest --test-dir build-macos -C Release --output-on-failure
bash scripts/ci-check.sh 'Package macOS installer' bash scripts/package-macos.sh build-macos
printf '\nPlugins: %s/build-macos/Sixth_artefacts/Release/{AU,VST3}\n' "$PWD"
