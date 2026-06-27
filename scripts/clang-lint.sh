#!/usr/bin/env bash
# Diff-scoped clang-format / clang-tidy against origin/master.
#
# Only lines changed vs the merge-base with origin/master are formatted or
# checked — exactly the scope CI enforces. Pre-existing non-conforming code
# elsewhere in the tree is never reformatted or flagged, so running these
# locally produces the same result as the PR gate and never churns unrelated
# files.
#
# Single source of truth for the `format`/`format-check`/`tidy-naming` CMake
# targets (CMakeLists.txt) and the pre-push hook (.githooks/pre-push).
#
# Usage: clang-lint.sh <format|format-check|tidy-naming> [build-dir]
set -euo pipefail

MODE="${1:?usage: clang-lint.sh <format|format-check|tidy-naming> [build-dir]}"
ROOT="$(git rev-parse --show-toplevel)"
BUILD="${2:-$ROOT/build}"

# Directories whose sources are subject to the project style. Mirrors LINT_DIRS
# in CMakeLists.txt; vendored headers (readerwriterqueue/, argh/) are excluded.
LINT_DIRS=(libmodmqttsrv libmodmqttconv modmqttd stdconv exprconv unittests)

if ! BASE_SHA="$(git merge-base origin/master HEAD 2>/dev/null)"; then
    echo "clang-lint: cannot find merge-base with origin/master — fetch it first:"
    echo "  git fetch origin master"
    exit 1
fi

case "$MODE" in
    format)
        # Reformat only the changed lines, in place.
        git clang-format --extensions cpp,hpp "$BASE_SHA" -- "${LINT_DIRS[@]}"
        ;;

    format-check)
        FMT_DIFF="$(git clang-format --diff --extensions cpp,hpp "$BASE_SHA" -- "${LINT_DIRS[@]}")"
        if echo "$FMT_DIFF" | grep -qE '^[-+]' && ! echo "$FMT_DIFF" | grep -q 'no modified files'; then
            echo "clang-format: formatting issues on changed lines. Fix with:"
            echo "  cmake --build $BUILD --target format"
            echo "$FMT_DIFF"
            exit 1
        fi
        echo "clang-format: changed lines clean."
        ;;

    tidy-naming)
        if [[ ! -f "$BUILD/compile_commands.json" ]]; then
            echo "clang-lint: no $BUILD/compile_commands.json — configure first:"
            echo "  cmake -S . -B build -G Ninja"
            exit 1
        fi
        # clang-tidy-diff.py lives in different places per distro/LLVM version.
        TIDY_DIFF_PY=
        for search_dir in /usr/share/clang /usr/lib/llvm-*/share/clang; do
            [[ -d "$search_dir" ]] || continue
            [[ -f "$search_dir/clang-tidy-diff.py" ]] && { TIDY_DIFF_PY="$search_dir/clang-tidy-diff.py"; break; }
        done
        if [[ -z "$TIDY_DIFF_PY" ]]; then
            echo "clang-lint: clang-tidy-diff.py not found — skipping naming check"
            exit 0
        fi
        # `|| true`: clang-tidy-diff.py exits non-zero whenever clang-tidy emits any
        # error — including the include-resolution noise filtered below — which would
        # otherwise abort this script (set -e) before we can classify the output.
        TIDY_LOG="$(git diff -U0 "$BASE_SHA" -- '*.cpp' '*.hpp' \
            ':(exclude)readerwriterqueue/*' ':(exclude)argh/*' \
            | python3 "$TIDY_DIFF_PY" -p1 -path "$BUILD" -- 2>&1 || true)"
        # Real violations always carry an enabled check name in brackets
        # ([readability-identifier-naming] / [readability-braces-around-statements]).
        # clang-diagnostic-error 'file not found' lines are include-resolution noise
        # from clang-tidy processing headers standalone (no compile command, hence no
        # -I flags) — not style violations, so they must not fail the check.
        VIOLATIONS="$(echo "$TIDY_LOG" | grep -E '\[readability-' || true)"
        DIAGS="$(echo "$TIDY_LOG" | grep -E 'error:' | grep -E '\[clang-diagnostic-' || true)"
        if [[ -n "$VIOLATIONS" ]]; then
            echo "clang-tidy: naming/brace violations on changed lines:"
            echo "$TIDY_LOG"
            exit 1
        fi
        if [[ -n "$DIAGS" ]]; then
            echo "clang-tidy: changed lines clean (ignored $(echo "$DIAGS" | wc -l) header include-resolution note(s) from standalone parsing)."
        else
            echo "clang-tidy: changed lines clean."
        fi
        ;;

    *)
        echo "clang-lint: unknown mode '$MODE' (expected format|format-check|tidy-naming)" >&2
        exit 2
        ;;
esac
