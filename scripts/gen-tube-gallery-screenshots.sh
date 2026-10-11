#!/usr/bin/env bash
#
# Write screenshot sheets of the passage tube gallery: the plan, profile,
# along-arm and 3/4 presets of every catalog case, as
# <build-dir>/tube-shots/<case>-<preset>.png.
#
# The gallery grabs its live window with QQuickWindow::grabWindow(), which
# needs a GPU-backed QPA platform, so this runs on the native platform and the
# window shows while it works.
#
# Usage:
#   scripts/gen-tube-gallery-screenshots.sh [--failing] [build-dir]
#
# --failing keeps only the cases whose mesh check reports issues.
# build-dir defaults to the newest build/ preset directory.

set -euo pipefail

# Print this file's leading comment block (everything after the shebang, up to
# the first non-comment line) as the help text.
usage() {
    awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' \
        "${BASH_SOURCE[0]}"
}

failing=()
build_dir=""
for arg in "$@"; do
    case "$arg" in
        --failing) failing=(--failing) ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "error: unknown option $arg" >&2; usage >&2; exit 1 ;;
        *) build_dir="$arg" ;;
    esac
done

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if [[ -z "$build_dir" ]]; then
    build_dir="$(ls -dt build/*/ 2>/dev/null | head -n1 || true)"
    build_dir="${build_dir%/}"
fi
if [[ -z "$build_dir" || ! -d "$build_dir" ]]; then
    echo "error: could not find a build directory; pass one explicitly." >&2
    echo "       e.g. scripts/gen-tube-gallery-screenshots.sh build/Qt_6_11_2_for_macOS_Debug" >&2
    exit 1
fi

echo "==> Building cavewhere-tube-gallery in $build_dir"
cmake --build "$build_dir" --target cavewhere-tube-gallery

shot_dir="$build_dir/tube-shots"
mkdir -p "$shot_dir"

echo "==> Writing tube gallery screenshots into $shot_dir"
"$build_dir/cavewhere-tube-gallery" --grab-all "$shot_dir" "${failing[@]+"${failing[@]}"}"
