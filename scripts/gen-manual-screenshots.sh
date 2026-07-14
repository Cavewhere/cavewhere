#!/usr/bin/env bash
#
# Regenerate the user-manual screenshots (docs/manual/images/).
#
# The screenshots are produced by the tst_ManualScreenshots.qml harness, which
# grabs the whole live window (QML UI chrome + the embedded 3D view) via
# QQuickWindow::grabWindow(). That requires a GPU-backed QPA platform — the
# headless `offscreen` platform grabs an empty image and the harness skips — so
# this runs the qml-test target on the native platform (cocoa on macOS).
#
# Usage:
#   scripts/gen-manual-screenshots.sh [build-dir]
#
# build-dir defaults to the newest build/ preset directory.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

build_dir="${1:-}"
if [[ -z "$build_dir" ]]; then
    build_dir="$(ls -dt build/*/ 2>/dev/null | head -n1 || true)"
    build_dir="${build_dir%/}"
fi
if [[ -z "$build_dir" || ! -d "$build_dir" ]]; then
    echo "error: could not find a build directory; pass one explicitly." >&2
    echo "       e.g. scripts/gen-manual-screenshots.sh build/Qt_6_11_0_for_macOS-Debug" >&2
    exit 1
fi

test_bin="$build_dir/cavewhere-qml-test"

echo "==> Building cavewhere-qml-test in $build_dir"
cmake --build "$build_dir" --target cavewhere-qml-test

image_dir="$repo_root/docs/manual/images"
mkdir -p "$image_dir"

echo "==> Generating manual screenshots into $image_dir"
# CW_MANUAL_IMAGE_DIR redirects WindowGrabber's output at the manual's images/.
# QT_QUICK_CONTROLS_STYLE=Fusion matches the real desktop app (the Windows/Linux
# look, with the sidebar File button) rather than the macOS native style.
# No --platform flag: use the native (GPU-backed) QPA so the 3D view renders.
CW_MANUAL_IMAGE_DIR="$image_dir" \
QT_QUICK_CONTROLS_STYLE="Fusion" \
    "$test_bin" -input "$repo_root/test-qml/tst_ManualScreenshots.qml" \
    2>&1 | tee /tmp/cavewhere-manual-screenshots.log

echo "==> Done. Screenshots written to $image_dir:"
ls -1 "$image_dir"/*.png 2>/dev/null || echo "  (no PNGs written — check the log above)"
