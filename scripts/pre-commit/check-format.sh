#!/usr/bin/env bash
# =============================================================================
# check-format.sh — clang-format check for the pre-commit framework
#
# Invoked by .pre-commit-config.yaml; receives staged C/C++ file paths as $@.
# Runs clang-format in --dry-run mode; fails if any file has violations.
# All offending files are collected before exiting so the developer sees every
# problem in a single run.
#
# Environment overrides:
#   CLANG_FORMAT_BIN  — use this binary instead of auto-detecting
# =============================================================================

set -euo pipefail

# ── Binary resolution ─────────────────────────────────────────────────────────
# Prefer clang-format-18 to match CI exactly; fall back through older releases.
resolve_clang_format() {
    if [[ -n "${CLANG_FORMAT_BIN:-}" ]]; then
        echo "$CLANG_FORMAT_BIN"
        return 0
    fi
    for candidate in clang-format-18 clang-format-17 clang-format-16 clang-format; do
        if command -v "$candidate" &>/dev/null; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

if ! CLANG_FORMAT=$(resolve_clang_format); then
    echo "ERROR: clang-format not found."
    echo "  Debian/Ubuntu: sudo apt-get install clang-format-18"
    echo "  macOS:         brew install llvm && brew link llvm"
    exit 1
fi

CF_VERSION=$("$CLANG_FORMAT" --version 2>&1 | head -1)
echo "  using: $CF_VERSION"
if [[ "$CF_VERSION" != *"version 18"* ]]; then
    echo "  WARNING: CI uses clang-format-18; minor differences are possible"
fi

# ── Check every staged file ───────────────────────────────────────────────────
FAILED_FILES=()

for FILE in "$@"; do
    if ! "$CLANG_FORMAT" --dry-run --Werror "$FILE" 2>/dev/null; then
        FAILED_FILES+=("$FILE")
    fi
done

if [[ ${#FAILED_FILES[@]} -gt 0 ]]; then
    echo ""
    echo "  Formatting violations in the following files:"
    for F in "${FAILED_FILES[@]}"; do
        echo "    $F"
    done
    echo ""
    echo "  Fix with:"
    echo "    $CLANG_FORMAT -i ${FAILED_FILES[*]}"
    echo ""
    echo "  Or auto-fix all staged C/C++ files:"
    echo "    git diff --cached --name-only | grep -E '\\.(cpp|hpp|h)$' | xargs $CLANG_FORMAT -i"
    exit 1
fi
