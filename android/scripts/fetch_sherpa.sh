#!/usr/bin/env bash
# Downloads the sherpa-onnx v1.13.8 Android libraries (Apache-2.0; ONNX Runtime inside is MIT) and its C header
# into android/app/sherpa/, checking SHA-256. They are not committed (large). Needed once before building the app.
set -euo pipefail
cd "$(dirname "$0")/../app"
VERSION=v1.13.8
ARCHIVE=sherpa-onnx-$VERSION-android.tar.bz2
ARCHIVE_SHA=2ff63469a71cb6009aa2e3ed5f4a670f8abdcbe4bb9ffd23776afc792a6b4f44
HEADER_SHA=2a1b95084be8fd1deb3228fcad2fd3f7f0258b64582f7402281ec174c7b7f4ce

if [ -f sherpa/jniLibs/arm64-v8a/libsherpa-onnx-c-api.so ] && [ -f sherpa/include/sherpa-onnx/c-api/c-api.h ]; then
  echo "sherpa-onnx already in android/app/sherpa"; exit 0
fi
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
curl -fsSL "https://github.com/k2-fsa/sherpa-onnx/releases/download/$VERSION/$ARCHIVE" -o "$tmp/$ARCHIVE"
echo "$ARCHIVE_SHA  $tmp/$ARCHIVE" | sha256sum -c -
curl -fsSL "https://raw.githubusercontent.com/k2-fsa/sherpa-onnx/$VERSION/sherpa-onnx/c-api/c-api.h" -o "$tmp/c-api.h"
echo "$HEADER_SHA  $tmp/c-api.h" | sha256sum -c -
mkdir -p sherpa/include/sherpa-onnx/c-api sherpa/jniLibs
cp "$tmp/c-api.h" sherpa/include/sherpa-onnx/c-api/c-api.h
tar xjf "$tmp/$ARCHIVE" -C "$tmp"
for abi in arm64-v8a armeabi-v7a x86_64; do   # x86_64 is for the emulator
  mkdir -p "sherpa/jniLibs/$abi"
  cp "$tmp/jniLibs/$abi/libonnxruntime.so" "$tmp/jniLibs/$abi/libsherpa-onnx-c-api.so" "sherpa/jniLibs/$abi/"
done
echo "sherpa-onnx $VERSION ready in android/app/sherpa"
