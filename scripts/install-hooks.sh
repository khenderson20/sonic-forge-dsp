#!/usr/bin/env bash
# =============================================================================
# install-hooks.sh — Git pre-commit hook installer for SonicForge DSP
#
# Two installation modes are offered:
#
#   [1] Standalone  — symlinks scripts/git-hooks/pre-commit into .git/hooks/.
#       Self-contained bash script; no extra tooling required.
#       Env vars (SONICFORGE_SKIP_*) allow per-check bypassing.
#
#   [2] pre-commit  — installs hooks via the pre-commit framework
#       (https://pre-commit.com).  Requires `pip install pre-commit`.
#       Hook configuration lives in .pre-commit-config.yaml (tracked in git).
#       Individual hooks can be skipped with SKIP=<hook-id>.
#
# Usage:
#   bash scripts/install-hooks.sh
# =============================================================================

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
HOOK_SRC="$REPO_ROOT/scripts/git-hooks/pre-commit"
HOOK_DST="$REPO_ROOT/.git/hooks/pre-commit"

# ── Terminal colours ──────────────────────────────────────────────────────────
if [[ -t 1 ]]; then
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    RESET='\033[0m'
else
    GREEN='' YELLOW='' CYAN='' BOLD='' RESET=''
fi

info()    { echo -e "${CYAN}  $*${RESET}"; }
success() { echo -e "${GREEN}  ✓ $*${RESET}"; }
warn()    { echo -e "${YELLOW}  ! $*${RESET}"; }

# ── Intro ─────────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}SonicForge DSP — Pre-commit Hook Installer${RESET}"
echo ""
echo "  Hooks mirror all three CI jobs:"
echo "    1. clang-format  (style check on staged files)"
echo "    2. clang-tidy    (static analysis)"
echo "    3. cmake build + ctest (full test suite)"
echo ""
echo "  Installation mode:"
echo ""
echo "    [1] Standalone   no extra tools required"
echo "                     single bash script, SONICFORGE_SKIP_* env vars"
echo ""
echo "    [2] pre-commit   requires: pip install pre-commit"
echo "                     config tracked in .pre-commit-config.yaml"
echo "                     per-hook skipping: SKIP=cmake-build-test git commit"
echo ""
echo "    [q] quit"
echo ""

while true; do
    read -rp "  Selection [1/2/q]: " CHOICE
    case "$CHOICE" in

        # ── Mode 1: Standalone ────────────────────────────────────────────────
        1)
            echo ""
            info "Installing standalone hook..."

            if [[ ! -f "$HOOK_SRC" ]]; then
                echo "  ERROR: source hook not found: $HOOK_SRC"
                exit 1
            fi

            # Back up any existing non-symlink hook.
            if [[ -f "$HOOK_DST" && ! -L "$HOOK_DST" ]]; then
                BACKUP="${HOOK_DST}.bak.$(date +%Y%m%d%H%M%S)"
                cp "$HOOK_DST" "$BACKUP"
                warn "Existing hook backed up to: $BACKUP"
            fi

            # Ensure the source script is executable.
            chmod +x "$HOOK_SRC"

            # Install via symlink so the hook stays in sync with the repo.
            ln -sf "../../scripts/git-hooks/pre-commit" "$HOOK_DST"

            success "Standalone hook installed."
            echo ""
            echo "  The hook now runs on every git commit."
            echo ""
            echo "  Selective skipping:"
            echo "    SONICFORGE_SKIP_FORMAT=1 git commit   # skip clang-format"
            echo "    SONICFORGE_SKIP_TIDY=1   git commit   # skip clang-tidy"
            echo "    SONICFORGE_SKIP_TESTS=1  git commit   # skip build+test"
            echo "    git commit --no-verify                # bypass all hooks"
            break
            ;;

        # ── Mode 2: pre-commit framework ──────────────────────────────────────
        2)
            echo ""
            info "Checking for pre-commit framework..."

            if ! command -v pre-commit &>/dev/null; then
                warn "pre-commit is not installed."
                echo ""
                echo "  Install options:"
                echo "    pip install pre-commit      # Python package manager"
                echo "    pipx install pre-commit     # isolated install (recommended)"
                echo "    brew install pre-commit     # macOS Homebrew"
                echo ""
                read -rp "  Install now with pip? [y/N]: " INSTALL_PC
                if [[ "${INSTALL_PC,,}" == "y" ]]; then
                    pip install --quiet pre-commit
                    success "pre-commit installed."
                else
                    echo ""
                    echo "  Aborted. Install pre-commit and re-run this script."
                    exit 0
                fi
            fi

            PC_VERSION=$(pre-commit --version 2>&1)
            info "Found: $PC_VERSION"

            pre-commit install

            success "pre-commit framework hooks installed."
            echo ""
            echo "  Hooks run on every git commit."
            echo ""
            echo "  Selective skipping:"
            echo "    SKIP=clang-format     git commit   # skip formatter"
            echo "    SKIP=clang-tidy       git commit   # skip static analysis"
            echo "    SKIP=cmake-build-test git commit   # skip build+test"
            echo ""
            echo "  Run all hooks manually (without committing):"
            echo "    pre-commit run --all-files"
            break
            ;;

        q | Q)
            echo ""
            echo "  Aborted. No changes made."
            exit 0
            ;;

        *)
            echo "  Invalid choice — enter 1, 2, or q."
            ;;
    esac
done

echo ""
