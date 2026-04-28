#!/usr/bin/env bash
# =============================================================================
# run-tests.sh — cmake incremental build + ctest for the pre-commit framework
#
# Invoked by .pre-commit-config.yaml with pass_filenames: false, always_run.
# Mirrors the CI build-and-test job: incrementally builds the project then
# runs the full test suite.  Automatically configures cmake if no build
# directory is found.
#
# Skipping (for WIP commits):
#   SKIP=cmake-build-test git commit -m "wip: ..."
#
# Environment overrides:
#   SONICFORGE_BUILD  — path to cmake build directory
# =============================================================================

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
START_TIME=$(date +%s)

# ── Locate cmake build directory ──────────────────────────────────────────────
BUILD_DIR="${SONICFORGE_BUILD:-}"

if [[ -z "$BUILD_DIR" ]]; then
    for candidate in \
        "$REPO_ROOT/build-ninja" \
        "$REPO_ROOT/build"; do
        if [[ -f "$candidate/CMakeCache.txt" ]]; then
            BUILD_DIR="$candidate"
            break
        fi
    done
fi

if [[ -z "$BUILD_DIR" || ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "  No cmake build found — configuring..."
    BUILD_DIR="$REPO_ROOT/build"
    cmake -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DSONICFORGE_BUILD_TESTS=ON \
        -DSONICFORGE_OPTIMIZE_FOR_HOST=OFF \
        "$REPO_ROOT"
fi

echo "  build dir: $BUILD_DIR"

# ── Incremental build ─────────────────────────────────────────────────────────
echo "  compiling..."
cmake --build "$BUILD_DIR" --parallel

# ── Run test suite ────────────────────────────────────────────────────────────
echo "  running tests..."
ctest --test-dir "$BUILD_DIR" --output-on-failure

END_TIME=$(date +%s)
echo "  completed in $((END_TIME - START_TIME))s"
