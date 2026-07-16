#!/bin/bash
# nightly-launcher.sh
#
# Stable entry point for the CaveWhere nightly test runner. This file is
# meant to be copied to a path outside any git checkout (e.g. ~/.local/bin/)
# and pointed at by ~/Library/LaunchAgents/com.cavewhere.nightly-test.plist.
#
# Putting the launchd entry point inside a dev checkout means a branch
# switch can silently change or break the nightly. The deployed copy of
# this launcher stays put; each run it fetches the latest nightly-test.sh
# from origin/dev on GitHub and executes that. The fetched script does its
# own fresh clone of the repo for the actual build.
#
# To update the launcher itself (rare): edit
# scripts/macos_nightly/nightly-launcher.sh in the repo, then re-copy it
# to the deployed location. See README.md in this directory for setup.

set -euo pipefail

SCRIPT_URL="https://raw.githubusercontent.com/Cavewhere/cavewhere/dev/scripts/macos_nightly/nightly-test.sh"
CACHE_DIR="/var/tmp/cavewhere-nightly"
SCRIPT_PATH="$CACHE_DIR/nightly-test.fetched.sh"

mkdir -p "$CACHE_DIR"

echo "=== Nightly launcher ==="
echo "Date:    $(date)"
echo "Source:  $SCRIPT_URL"
echo "Cached:  $SCRIPT_PATH"

# Fetch to a temp file first so a partial download never overwrites a good copy.
TMP_FILE="$(mktemp "${CACHE_DIR}/nightly-test.fetched.XXXXXX.sh")"
trap 'rm -f "$TMP_FILE"' EXIT

curl -fsSL --retry 3 --retry-delay 5 --max-time 60 "$SCRIPT_URL" -o "$TMP_FILE"

# Sanity-check: file must be non-empty and start with a shebang.
if [[ ! -s "$TMP_FILE" ]] || ! head -1 "$TMP_FILE" | grep -q '^#!'; then
    echo "ERROR: fetched script is empty or missing shebang" >&2
    exit 1
fi

mv "$TMP_FILE" "$SCRIPT_PATH"
trap - EXIT
chmod +x "$SCRIPT_PATH"

exec "$SCRIPT_PATH" "$@"
