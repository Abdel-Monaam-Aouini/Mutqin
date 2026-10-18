#!/usr/bin/env bash
# Decides which CI jobs a push or pull request needs, from the directories it changed.
# Writes android=, cpp= and python= (true/false) to $GITHUB_OUTPUT. Everything runs when the base is unknown
# (a first push, a manual run) or when CI itself changed.
#   usage: changes.sh BASE_SHA HEAD_SHA        (BASE_SHA empty or all zeros: run everything)
set -euo pipefail
base="${1:-}"
head="${2:-HEAD}"

if [ -z "$base" ] || [ "$base" = "0000000000000000000000000000000000000000" ] || ! git cat-file -e "${base}^{commit}" 2>/dev/null; then
  files=""
  everything=true
else
  files="$(git diff --name-only "$base" "$head")"
  everything=false
fi

matches() { # does any changed file match this extended regex?
  [ "$everything" = true ] && return 0
  printf '%s\n' "$files" | grep -Eq "$1"
}

# CI itself and the Quran checksums (every job downloads and verifies the text) touch everything
common='^(\.github/workflows/ci\.yml|\.github/scripts/changes\.sh|data/quran/checksums\.txt)$'
# the app compiles the core (JNI), so a change of the core is a change of the app
android='^(android/|core/)'
# the C++ jobs build the core and the CLI; the parity tests run the Python tools
cpp='^(core/|cli/|CMakeLists\.txt|tools/|\.clang-format$)'
python='^(tools/|ruff\.toml$)'

out() { if matches "$common" || matches "$2"; then echo "$1=true"; else echo "$1=false"; fi; }
{
  out android "$android"
  out cpp "$cpp"
  out python "$python"
} | tee -a "${GITHUB_OUTPUT:-/dev/stdout}"
