#!/usr/bin/env bash
# Downloads ktlint (MIT, Pinterest) into android/.tools/ and checks its SHA-256. Used by make android-lint / android-format
# and by CI to check the Kotlin code style.
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION=1.8.0
SHA256=a3fd620207d5c40da6ca789b95e7f823c54e854b7fade7f613e91096a3706d75
mkdir -p .tools
if [ -x .tools/ktlint ] && echo "$SHA256  .tools/ktlint" | sha256sum -c - >/dev/null 2>&1; then exit 0; fi
curl -fsSL "https://github.com/pinterest/ktlint/releases/download/$VERSION/ktlint" -o .tools/ktlint
echo "$SHA256  .tools/ktlint" | sha256sum -c -
chmod +x .tools/ktlint
