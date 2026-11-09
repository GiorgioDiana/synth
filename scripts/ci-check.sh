#!/usr/bin/env bash
# Run one check, preserving its exit status and publishing bounded failure details.
set -euo pipefail
if [ "$#" -lt 2 ]; then
  echo 'Usage: ci-check.sh <label> <command> [arguments ...]' >&2
  exit 64
fi
label="$1"
shift
log_file="$(mktemp "${TMPDIR:-/tmp}/sixth-check.XXXXXX")"
trap 'rm -f -- "$log_file"' EXIT

# Keep the command status rather than tee's status, including when it fails.
set +e
"$@" 2>&1 | tee "$log_file"
result="${PIPESTATUS[0]}"
set -e

if [ "$result" -ne 0 ]; then
  printf '%s failed with exit status %s.\n' "$label" "$result" >&2
  if [ "${GITHUB_ACTIONS:-}" = true ]; then
    # Limit annotation size to the last 60 lines and 8,000 characters.
    details="$(tail -n 60 "$log_file")"
    if [ "${#details}" -gt 8000 ]; then
      details="${details: -8000}"
    fi
    message="$label failed with exit status $result. Last output:"
    if [ -n "$details" ]; then
      message="$message"$'\n'"$details"
    else
      message="$message (the command produced no output)"
    fi
    escape_data() {
      local value="$1"
      value="${value//'%'/'%25'}"
      value="${value//$'\r'/'%0D'}"
      value="${value//$'\n'/'%0A'}"
      printf '%s' "$value"
    }
    title="$(escape_data "$label")"
    title="${title//':'/'%3A'}"
    title="${title//','/'%2C'}"
    printf '\n::error title=%s::%s\n' "$title" "$(escape_data "$message")"
  fi
fi
exit "$result"
