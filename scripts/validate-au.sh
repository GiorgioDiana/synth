#!/usr/bin/env bash
set -euo pipefail
if [ "$(uname -s)" != Darwin ]; then
  echo 'Audio Unit validation requires macOS and auval.' >&2
  exit 1
fi
cd "$(dirname "$0")/.."
build_dir="${1:-build-macos}"
component="$build_dir/Sixth_artefacts/Release/AU/Sixth.component"
test -d "$component"
installed="$HOME/Library/Audio/Plug-Ins/Components/Sixth.component"
components_file="$(mktemp "${TMPDIR:-/tmp}/sixth-components.XXXXXX")"
trap 'rm -f -- "$components_file"' EXIT

printf 'Installing the built AU for validation: %s\n' "$installed"
mkdir -p "$(dirname "$installed")"
ditto "$component" "$installed"
/usr/libexec/PlistBuddy -c 'Print :AudioComponents:0' "$installed/Contents/Info.plist"
registered() {
  grep -Eq 'aumu[[:space:]]+Sxth[[:space:]]+Gdia' "$components_file"
}
auval -a > "$components_file"
if ! registered; then
  echo 'Refreshing AudioComponentRegistrar to discover the newly installed AU.'
  killall AudioComponentRegistrar || true
  for attempt in {1..10}; do
    auval -a > "$components_file"
    if registered; then
      break
    fi
    sleep 2
  done
fi
if ! registered; then
  echo 'Sixth is not listed after the bounded registration refresh; auval will report the validation result.' >&2
fi
grep -E 'Sxth|Sixth' "$components_file" || true
# Always run the real validator. Its exit status is the result of this script.
auval -v aumu Sxth Gdia
