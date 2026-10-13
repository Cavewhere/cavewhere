#!/usr/bin/env bash
#
# Grab the Style Gallery page in light and dark for visual review.
#
# Runs tst_StyleGalleryPage's screenshot function on the native platform (a
# window opens briefly). Images land in <build-dir>/style-shots/.
#
# Usage: scripts/gen-style-gallery-screenshots.sh [build-dir]
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
    exit 1
fi

cmake --build "$build_dir" --target cavewhere-qml-test -j 10

shot_dir="$repo_root/$build_dir/style-shots"
mkdir -p "$shot_dir"

CW_MANUAL_IMAGE_DIR="$shot_dir" \
    "$build_dir/cavewhere-qml-test" -input "$repo_root/test-qml/tst_StyleGalleryPage.qml" \
    StyleGalleryPage::test_screenshots \
    2>&1 | tee "$shot_dir/run.log"

echo "==> Screenshots in $shot_dir"
