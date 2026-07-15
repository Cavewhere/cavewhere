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

# Assemble the carpet-orbit frames (scraps-carpet-orbit-NN.png) into an animated
# GIF for the Scraps / Carpeting overview, plus a static poster (frame 0) shown by
# default so the animation only plays when the reader expands the <details> in
# carpeting.md. Then drop the frames — they're an intermediate, not a manual
# asset. Needs ffmpeg; skipped with a warning if the frames or ffmpeg are missing.
orbit_frames=("$image_dir"/scraps-carpet-orbit-[0-9][0-9].png)
if [[ -e "${orbit_frames[0]}" ]]; then
    if command -v ffmpeg >/dev/null 2>&1; then
        echo "==> Assembling scraps-carpet-orbit.gif from ${#orbit_frames[@]} frames"
        # Read the frames as a numbered sequence rather than a glob: ffmpeg's
        # -pattern_type glob rejects a bracket expression like [0-9][0-9] ("Error
        # opening input"), and a plain * would sweep -poster.png into the
        # animation. %02d matches frames 00..NN exactly and excludes the poster.
        #
        # Two-pass palette (generate + apply) for clean GIF colors; scale to a
        # doc-friendly width with a light dither to keep the gradient smooth.
        if ! ffmpeg -y -framerate 12 -start_number 0 \
            -i "$image_dir/scraps-carpet-orbit-%02d.png" \
            -vf "scale=1000:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=bayer:bayer_scale=3" \
            -loop 0 "$image_dir/scraps-carpet-orbit.gif" \
            >>/tmp/cavewhere-manual-screenshots.log 2>&1
        then
            # Be loud: a silent failure here leaves the previous run's GIF in
            # place, which looks correct but is stale.
            echo "    error: ffmpeg failed to assemble the GIF;" \
                 "see /tmp/cavewhere-manual-screenshots.log" >&2
            exit 1
        fi
        # The harness writes scraps-carpet-orbit-poster.png (a dedicated low-angle
        # perspective still) at native crop size; scale it down to the doc width.
        if [[ -e "$image_dir/scraps-carpet-orbit-poster.png" ]]; then
            ffmpeg -y -i "$image_dir/scraps-carpet-orbit-poster.png" \
                -vf "scale=1000:-1:flags=lanczos" \
                "$image_dir/scraps-carpet-orbit-poster.tmp.png" \
                >>/tmp/cavewhere-manual-screenshots.log 2>&1 \
                && mv "$image_dir/scraps-carpet-orbit-poster.tmp.png" \
                      "$image_dir/scraps-carpet-orbit-poster.png"
        fi
        rm -f "$image_dir"/scraps-carpet-orbit-[0-9][0-9].png
        echo "    wrote $image_dir/scraps-carpet-orbit.gif + poster"
    else
        echo "    warning: ffmpeg not found — leaving orbit frames, no GIF built" >&2
    fi
fi

echo "==> Done. Screenshots written to $image_dir:"
ls -1 "$image_dir"/*.png "$image_dir"/*.gif 2>/dev/null || echo "  (no images written — check the log above)"
