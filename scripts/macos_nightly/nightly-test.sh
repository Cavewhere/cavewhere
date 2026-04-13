#!/bin/bash
set -euo pipefail

# --- Configuration ---
REPO_URL="https://github.com/Cavewhere/cavewhere.git"
BRANCH="origin/dev"
CPP_RUNS="${CPP_RUNS:-16}"
QML_RUNS="${QML_RUNS:-8}"
QT_PATH="$HOME/Qt/6.11.0/macos"
WORK_DIR="/var/tmp/cavewhere-nightly/$(date +%Y-%m-%d_%H%M%S)"
KEEP_DAYS=90
TEST_TIMEOUT=22m

# --- Cleanup old runs ---
NIGHTLY_DIR="/var/tmp/cavewhere-nightly"
if [[ -d "$NIGHTLY_DIR" ]]; then
    find "$NIGHTLY_DIR" -maxdepth 1 -mindepth 1 -type d -mtime +${KEEP_DAYS} -exec rm -rf {} +
fi

# --- Setup ---
echo "=== CaveWhere Nightly Flaky Test Runner ==="
echo "Date:       $(date)"
echo "Branch:     $BRANCH"
echo "Iterations: cpp=$CPP_RUNS qml=$QML_RUNS"
echo "Work dir:   $WORK_DIR"
echo ""

mkdir -p "$WORK_DIR"
LOG_DIR="$WORK_DIR/logs"
mkdir -p "$LOG_DIR"

# --- Clone ---
echo ">>> Cloning repository..."
git clone --recursive "$REPO_URL" "$WORK_DIR/cavewhere" 2>&1 | tee "$LOG_DIR/clone.log"
cd "$WORK_DIR/cavewhere"

echo ">>> Checking out $BRANCH..."
git checkout --detach "$BRANCH"
COMMIT_SHA=$(git rev-parse --short HEAD)
echo "Commit: $COMMIT_SHA"

# --- Conan install ---
echo ">>> Running conan install..."

# Pin CMake below 4.0 (wxWidgets compatibility)
CONAN_PROFILE="$(conan config home)/profiles/nightly-test"
# Write a dedicated conan profile (not default) with deployment target 14.0, C++20, and CMake < 4.0
cat > "$CONAN_PROFILE" <<'PROFILE'
[settings]
arch=armv8
build_type=Release
compiler=apple-clang
compiler.cppstd=gnu20
compiler.libcxx=libc++
compiler.version=17
os=Macos
os.version=14.0

[tool_requires]
!cmake/*: cmake/[>=3 <4]
PROFILE

conan install . -pr nightly-test -o "&:system_qt=True" --build=missing -of conan_deps 2>&1 | tee "$LOG_DIR/conan-install.log"

# --- CMake configure ---
echo ">>> Configuring CMake..."
cmake --preset conan-release \
    -DCMAKE_TOOLCHAIN_FILE=conan_deps/conan_toolchain.cmake \
    -DCMAKE_PREFIX_PATH="$QT_PATH" \
    -DBUILD_TESTING=ON \
    2>&1 | tee "$LOG_DIR/cmake-configure.log"

# Discover the build directory from the preset
BUILD_DIR="conan_deps"
echo "Build directory: $BUILD_DIR"

# --- Build test targets (single build, no parallel test launching during build) ---
echo ">>> Building test targets..."
cmake --build "$BUILD_DIR" -j8 --target cavewhere-test cavewhere-qml-test 2>&1 | tee "$LOG_DIR/cmake-build.log"

CPP_TEST="$(pwd)/$BUILD_DIR/cavewhere-test"
QML_TEST="$(pwd)/$BUILD_DIR/cavewhere-qml-test"

if [[ ! -x "$CPP_TEST" ]]; then
    echo "ERROR: cavewhere-test binary not found at $CPP_TEST"
    exit 1
fi
if [[ ! -x "$QML_TEST" ]]; then
    echo "ERROR: cavewhere-qml-test binary not found at $QML_TEST"
    exit 1
fi

echo ""
echo ">>> Build complete. Starting test runs..."

# --- Run tests: cpp first (parallel), then qml (sequential) ---
CPP_PARALLEL="${CPP_PARALLEL:-4}"
QML_PARALLEL="${QML_PARALLEL:-1}"
echo "=== Running tests (cpp: $CPP_RUNS runs x$CPP_PARALLEL parallel, then qml: $QML_RUNS runs x$QML_PARALLEL parallel) ==="

# Directory for exit code files
mkdir -p "$LOG_DIR/exit_codes"

# Helper script for running a single test
RUN_SCRIPT="$LOG_DIR/run-test.sh"
cat > "$RUN_SCRIPT" <<RUNEOF
#!/bin/bash
BINARY="\$1"
LOG_FILE="\$2"
EXIT_FILE="\$3"
shift 3
gtimeout --kill-after=30s ${TEST_TIMEOUT} "\$BINARY" "\$@" > "\$LOG_FILE" 2>&1
EC=\$?
if [ \$EC -eq 124 ]; then
    echo "TIMEOUT: test killed after ${TEST_TIMEOUT}" >> "\$LOG_FILE"
fi
echo \$EC > "\$EXIT_FILE"
RUNEOF
chmod +x "$RUN_SCRIPT"

# Build job lists: one line per test run
CPP_JOBS="$LOG_DIR/cpp-jobs.txt"
QML_JOBS="$LOG_DIR/qml-jobs.txt"
> "$CPP_JOBS"
> "$QML_JOBS"
for i in $(seq 1 "$CPP_RUNS"); do
    ii=$(printf "%02d" "$i")
    echo "$CPP_TEST $LOG_DIR/cpp-test-run-${ii}.log $LOG_DIR/exit_codes/cpp-${ii} --platform offscreen" >> "$CPP_JOBS"
done
for i in $(seq 1 "$QML_RUNS"); do
    ii=$(printf "%02d" "$i")
    echo "$QML_TEST $LOG_DIR/qml-test-run-${ii}.log $LOG_DIR/exit_codes/qml-${ii} --platform offscreen" >> "$QML_JOBS"
done

# Run cpp tests first, then qml tests (no overlap to avoid load-induced flakiness)
echo ">>> Running cavewhere-test ($CPP_RUNS runs, $CPP_PARALLEL parallel)..."
xargs -P "$CPP_PARALLEL" -L1 "$RUN_SCRIPT" < "$CPP_JOBS"

echo ">>> Running cavewhere-qml-test ($QML_RUNS runs, $QML_PARALLEL parallel)..."
xargs -P "$QML_PARALLEL" -L1 "$RUN_SCRIPT" < "$QML_JOBS"

# Collect results
cpp_failures=0
qml_failures=0
cpp_results=()
qml_results=()

for i in $(seq 1 "$CPP_RUNS"); do
    ii=$(printf "%02d" "$i")
    cpp_exit=$(cat "$LOG_DIR/exit_codes/cpp-${ii}" 2>/dev/null || echo 1)
    if [[ "$cpp_exit" -eq 0 ]]; then
        cpp_results+=("PASS")
    else
        cpp_results+=("FAIL")
        cpp_failures=$((cpp_failures + 1))
    fi
done

for i in $(seq 1 "$QML_RUNS"); do
    ii=$(printf "%02d" "$i")
    qml_exit=$(cat "$LOG_DIR/exit_codes/qml-${ii}" 2>/dev/null || echo 1)
    if [[ "$qml_exit" -eq 0 ]]; then
        qml_results+=("PASS")
    else
        qml_results+=("FAIL")
        qml_failures=$((qml_failures + 1))
    fi
done

# --- Generate summary ---
echo ""
echo "=== Generating summary ==="

SUMMARY="$LOG_DIR/summary.txt"
{
    echo "CaveWhere Nightly Test Summary"
    echo "=============================="
    echo "Date:       $(date)"
    echo "Branch:     $BRANCH"
    echo "Commit:     $COMMIT_SHA"
    echo "Iterations: cpp=$CPP_RUNS qml=$QML_RUNS"
    echo ""
    echo "Results:"
    echo "  cavewhere-test:     $cpp_failures/$CPP_RUNS failures"
    echo "  cavewhere-qml-test: $qml_failures/$QML_RUNS failures"
    echo ""
    echo "cavewhere-test runs:"
    for i in $(seq 1 "$CPP_RUNS"); do
        idx=$((i - 1))
        printf "  Run %02d: %s\n" "$i" "${cpp_results[$idx]}"
    done

    # Extract failing test names from C++ logs
    if [[ $cpp_failures -gt 0 ]]; then
        echo ""
        echo "  Failing tests (with frequency):"
        for i in $(seq 1 "$CPP_RUNS"); do
            idx=$((i - 1))
            if [[ "${cpp_results[$idx]}" == "FAIL" ]]; then
                cpp_log=$(printf "%s/cpp-test-run-%02d.log" "$LOG_DIR" "$i")
                grep -E "\.cpp:[0-9]+: FAILED:" "$cpp_log" 2>/dev/null || true
            fi
        done | sort | uniq -c | sort -rn
    fi

    echo ""
    echo "cavewhere-qml-test runs:"
    for i in $(seq 1 "$QML_RUNS"); do
        idx=$((i - 1))
        printf "  Run %02d: %s\n" "$i" "${qml_results[$idx]}"
    done

    # Extract failing test names from QML logs
    if [[ $qml_failures -gt 0 ]]; then
        echo ""
        echo "  Failing tests (with frequency):"
        for i in $(seq 1 "$QML_RUNS"); do
            idx=$((i - 1))
            if [[ "${qml_results[$idx]}" == "FAIL" ]]; then
                qml_log=$(printf "%s/qml-test-run-%02d.log" "$LOG_DIR" "$i")
                grep -E "^FAIL!" "$qml_log" 2>/dev/null || true
            fi
        done | sort | uniq -c | sort -rn
    fi
} > "$SUMMARY"

cat "$SUMMARY"

# --- macOS notification ---
total_failures=$((cpp_failures + qml_failures))
if [[ $total_failures -eq 0 ]]; then
    notify_msg="All runs passed (cpp: $CPP_RUNS, qml: $QML_RUNS)."
else
    notify_msg="cpp: $cpp_failures/$CPP_RUNS failures, qml: $qml_failures/$QML_RUNS failures. See $LOG_DIR"
fi

terminal-notifier \
    -title "CaveWhere Nightly Tests" \
    -subtitle "Commit $COMMIT_SHA" \
    -message "$notify_msg" \
    -open "file://$LOG_DIR"

# --- Cleanup clone and build artifacts, keep only logs ---
echo ">>> Cleaning up clone and build artifacts..."
CLONE_DIR="$WORK_DIR/cavewhere"
if [[ "$CLONE_DIR" == /var/tmp/cavewhere-nightly/*/cavewhere && -d "$CLONE_DIR" ]]; then
    rm -rf "$CLONE_DIR"
else
    echo "WARNING: Skipping cleanup — unexpected clone path: $CLONE_DIR"
fi

echo ""
echo "=== Done ==="
echo "Logs: $LOG_DIR"
echo "Summary: $SUMMARY"
