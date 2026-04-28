#!/usr/bin/env bash
# =============================================================================
# check-tidy.sh — clang-tidy static analysis for the pre-commit framework
#
# Invoked by .pre-commit-config.yaml with pass_filenames: false.
# Mirrors CI exactly: analyses src/oscillator.cpp + tests/oscillator_test.cpp
# with --warnings-as-errors='*' against the project's compile_commands.json.
#
# A cmake-configured build directory is required for compile_commands.json.
# If none is found the script automatically runs cmake to create one
# (configure only — no full build needed for clang-tidy).
#
# Environment overrides:
#   CLANG_TIDY_BIN    — use this binary instead of auto-detecting
#   SONICFORGE_BUILD  — path to cmake build directory
# =============================================================================

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"

# ── Binary resolution ─────────────────────────────────────────────────────────
resolve_clang_tidy() {
    if [[ -n "${CLANG_TIDY_BIN:-}" ]]; then
        echo "$CLANG_TIDY_BIN"
        return 0
    fi
    for candidate in clang-tidy-18 clang-tidy-17 clang-tidy-16 clang-tidy; do
        if command -v "$candidate" &>/dev/null; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

if ! CLANG_TIDY=$(resolve_clang_tidy); then
    echo "ERROR: clang-tidy not found."
    echo "  Debian/Ubuntu: sudo apt-get install clang-tidy-18"
    echo "  macOS:         brew install llvm && brew link llvm"
    exit 1
fi

CT_VERSION=$("$CLANG_TIDY" --version 2>&1 | head -1)
echo "  using: $CT_VERSION"
if [[ "$CT_VERSION" != *"version 18"* ]]; then
    echo "  WARNING: CI uses clang-tidy-18; some diagnostics may differ"
fi

# ── Locate compile_commands.json ──────────────────────────────────────────────
# Check SONICFORGE_BUILD override first, then probe canonical build dirs.
BUILD_DIR="${SONICFORGE_BUILD:-}"

if [[ -z "$BUILD_DIR" ]]; then
    for candidate in \
        "$REPO_ROOT/build-ninja" \
        "$REPO_ROOT/build"; do
        if [[ -f "$candidate/compile_commands.json" ]]; then
            BUILD_DIR="$candidate"
            break
        fi
    done
fi

if [[ -z "$BUILD_DIR" || ! -f "$BUILD_DIR/compile_commands.json" ]]; then
    echo "  compile_commands.json not found — running cmake configure..."
    BUILD_DIR="$REPO_ROOT/build"
    cmake -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DSONICFORGE_BUILD_TESTS=ON \
        "$REPO_ROOT" 2>&1 | tail -8
    echo "  configured: $BUILD_DIR"
fi

echo "  build dir: $BUILD_DIR"

# ── Run clang-tidy (exact mirror of CI) ───────────────────────────────────────
# Analyse only the two files that CI targets.  HeaderFilterRegex in
# .clang-tidy already limits diagnostic reporting to project headers.
"$CLANG_TIDY" \
    --warnings-as-errors='*' \
    -p "$BUILD_DIR" \
    "$REPO_ROOT/src/oscillator.cpp" \
    "$REPO_ROOT/tests/oscillator_test.cpp"
