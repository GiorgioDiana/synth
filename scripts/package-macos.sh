#!/usr/bin/env bash
set -euo pipefail
if [ "$(uname -s)" != Darwin ]; then
  echo 'Packaging requires macOS and its pkgbuild tool.' >&2
  exit 1
fi
cd "$(dirname "$0")/.."
build_dir="${1:-build-macos}"
build_dir="$(cd "$build_dir" && pwd)"
artefacts="$build_dir/Sixth_artefacts/Release"
for bundle in AU/Sixth.component VST3/Sixth.vst3 Standalone/Sixth.app; do
  test -d "$artefacts/$bundle"
done
staging="$(mktemp -d "$build_dir/package.XXXXXX")"
trap 'rm -rf -- "$staging"' EXIT
mkdir -p "$staging/Library/Audio/Plug-Ins/Components" "$staging/Library/Audio/Plug-Ins/VST3" "$staging/Applications"
ditto "$artefacts/AU/Sixth.component" "$staging/Library/Audio/Plug-Ins/Components/Sixth.component"
ditto "$artefacts/VST3/Sixth.vst3" "$staging/Library/Audio/Plug-Ins/VST3/Sixth.vst3"
ditto "$artefacts/Standalone/Sixth.app" "$staging/Applications/Sixth.app"
pkgbuild --root "$staging" --identifier com.giorgiodiana.sixth.pkg --version 0.4.0 --install-location / "$build_dir/Sixth-macOS.pkg"
tar -czf "$build_dir/Sixth-macOS.tar.gz" -C "$artefacts" AU VST3 Standalone
printf 'Installer: %s/Sixth-macOS.pkg\n' "$build_dir"
