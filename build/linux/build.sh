#!/bin/sh
# Builds the Linux .deb package in Docker. Output: dist/mit-token-signing_<version>_amd64.deb
# Usage: build/linux/build.sh [build number]
set -e

# pwd -W gives a Windows path for Docker Desktop when run from Git Bash
ROOT=$(cd "$(dirname "$0")/../.." && (pwd -W 2>/dev/null || pwd))
IMAGE=mit-token-signing-linux-build

mkdir -p "$ROOT/dist"
docker build -t "$IMAGE" "$ROOT/build/linux"
MSYS_NO_PATHCONV=1 docker run --rm \
    -e BUILD_NUMBER="${1:-0}" \
    -v "$ROOT:/src:ro" \
    -v "$ROOT/dist:/dist" \
    "$IMAGE" sh /src/build/linux/package.sh
