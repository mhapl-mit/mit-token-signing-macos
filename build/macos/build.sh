#!/bin/sh
# Builds the macOS native host installer into dist/mit-token-signing_<version>.pkg.
# Needs only Xcode (from the App Store), no programming. Run in Terminal from the repository:
#
#   sh build/macos/build.sh [build number]
#
# The package is signed and notarized when the Developer ID certificates of M.I.T. Consulting
# are in the keychain (and the notarytool profile "mit-token-signing" exists), otherwise it is
# built unsigned, which is fine for testing.
set -e

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_NUMBER=${1:-0}
cd "$ROOT/host-osx"

if ! xcodebuild -version >/dev/null 2>&1; then
    echo "Xcode is missing: install it from the App Store, open it once and run:"
    echo "  sudo xcode-select -s /Applications/Xcode.app"
    exit 1
fi
xcodebuild -version | head -1

if security find-identity -v | grep -q "Developer ID Installer"; then
    if xcrun notarytool history --keychain-profile mit-token-signing >/dev/null 2>&1; then
        TARGET=notarize
    else
        echo "notarytool profile mit-token-signing not found, the package is signed but not notarized"
        TARGET=signed
    fi
else
    echo "Developer ID certificates not found, building an unsigned package for testing"
    TARGET=pkg
fi

make "$TARGET" BUILD_NUMBER="$BUILD_NUMBER"

mkdir -p "$ROOT/dist"
mv -f mit-token-signing_*.pkg "$ROOT/dist/"
echo "Done: $(ls "$ROOT"/dist/mit-token-signing_*.pkg)"
