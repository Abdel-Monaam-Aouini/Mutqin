#!/usr/bin/env bash
# Compares the pinned versions of the C++ dependencies (which Dependabot cannot see: they are CMake /
# CI downloads) with the latest upstream releases, and keeps ONE open issue listing what is outdated.
# Usage: check_cpp_deps.sh            (needs GH_TOKEN; creates / updates / closes the issue)
#        DRY_RUN=1 check_cpp_deps.sh  (only prints)
set -euo pipefail
cd "$(dirname "$0")/../.."

TITLE="C++ dependency updates available"

# name | upstream repo | pinned version (read from the file that pins it) | where
pinned_googletest=$(grep -oE 'googletest/archive/refs/tags/v[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | head -1 | sed 's#.*/##')
pinned_miniaudio=$(grep -oE 'miniaudio/[0-9]+\.[0-9]+\.[0-9]+/miniaudio.h' cli/CMakeLists.txt | head -1 | cut -d/ -f2)
pinned_sherpa=$(grep -oE 'SHERPA_VERSION: v[0-9]+\.[0-9]+\.[0-9]+' .github/workflows/ci.yml | head -1 | awk '{print $2}')

deps=(
  "GoogleTest|google/googletest|$pinned_googletest|CMakeLists.txt (FetchContent URL)"
  "miniaudio|mackron/miniaudio|$pinned_miniaudio|cli/CMakeLists.txt (download URL + SHA-256)"
  "sherpa-onnx|k2-fsa/sherpa-onnx|$pinned_sherpa|.github/workflows/ci.yml (SHERPA_VERSION + SHA-256)"
)

body=""
for entry in "${deps[@]}"; do
  IFS='|' read -r name repo pinned where <<<"$entry"
  if [ -z "$pinned" ]; then
    echo "::error::could not read the pinned version of $name"
    exit 1
  fi
  latest=$(gh api "repos/$repo/releases/latest" --jq .tag_name)
  if [ "$latest" = "$pinned" ]; then
    echo "$name: up to date ($pinned)"
  else
    echo "$name: pinned $pinned, latest $latest"
    body+="- **$name**: pinned \`$pinned\`, latest release \`$latest\` (https://github.com/$repo/releases/tag/$latest). Pinned in $where.\n"
  fi
done

if [ "${DRY_RUN:-0}" = "1" ]; then
  [ -n "$body" ] && printf '%b' "$body"
  exit 0
fi

existing=$(gh issue list --state open --search "\"$TITLE\" in:title" --json number,title --jq ".[] | select(.title==\"$TITLE\") | .number" | head -1)
if [ -z "$body" ]; then
  if [ -n "$existing" ]; then
    gh issue close "$existing" --comment "All C++ dependencies are up to date."
  fi
  exit 0
fi
text=$(printf 'Newer releases exist for pinned C++ dependencies. To update one: change the version in the file named, update its SHA-256 (download the new file and run `sha256sum`), read its license again (docs/LICENSES.md), run the full checks, and commit.\n\n%b' "$body")
if [ -n "$existing" ]; then
  gh issue edit "$existing" --body "$text"
else
  gh issue create --title "$TITLE" --body "$text"
fi
