#!/bin/sh
# Puts the install guide (web/install) and the installers of one release under stable names
# into dist/token-signing-install/ and dist/mit-token-signing-install_<version>.zip, which web
# applications serve as it is (ERMS). The folder (with exe/) is also the content of
# repos.mit-consulting.cz/token-sign/, dist/repos-server/ has the layout of the site root.
#
# Usage: build/web/package.sh <version>, e.g. 1.2.0.13; the installers of that version
# must be in dist/ (build/windows/build.ps1, build/linux/build.sh, build/macos/build.sh).
set -e

VERSION=${1:?usage: $0 <version>}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
DIST="$ROOT/dist"
OUT="$DIST/token-signing-install"

for f in "mit-token-signing_$VERSION.x64.msi" "mit-token-signing_$VERSION.x86.msi" \
        "mit-token-signing_$VERSION.pkg" "mit-token-signing_${VERSION}_amd64.deb"; do
    [ -f "$DIST/$f" ] || { echo "Missing $DIST/$f" >&2; exit 1; }
done

rm -rf "$OUT"
mkdir -p "$OUT"
cp "$ROOT"/web/install/* "$OUT/"
cp "$DIST/mit-token-signing_$VERSION.x64.msi" "$OUT/mit-token-signing-x64.msi"
cp "$DIST/mit-token-signing_$VERSION.x86.msi" "$OUT/mit-token-signing-x86.msi"
cp "$DIST/mit-token-signing_$VERSION.pkg" "$OUT/mit-token-signing.pkg"
cp "$DIST/mit-token-signing_${VERSION}_amd64.deb" "$OUT/mit-token-signing_amd64.deb"
printf '{"version": "%s"}\n' "$VERSION" > "$OUT/version.json"

rm -f "$DIST/mit-token-signing-install_$VERSION.zip"
(cd "$DIST" && python3 -c "import shutil, sys; shutil.make_archive(sys.argv[1], 'zip', '.', 'token-signing-install')" \
    "mit-token-signing-install_$VERSION" 2>/dev/null \
    || python -c "import shutil, sys; shutil.make_archive(sys.argv[1], 'zip', '.', 'token-signing-install')" \
    "mit-token-signing-install_$VERSION")
# repos.mit-consulting.cz/token-sign/ gets the whole folder, with the links of the old page in
# exe/ (added after the zip, ERMS does not need the second copy of the MSI packages)
mkdir -p "$OUT/exe"
cp "$OUT/mit-token-signing-x64.msi" "$OUT/exe/install_mit_token_sign_x64.msi"
cp "$OUT/mit-token-signing-x86.msi" "$OUT/exe/install_mit_token_sign_x86.msi"
SERVER="$DIST/repos-server/token-sign"
rm -rf "$DIST/repos-server"
mkdir -p "$DIST/repos-server"
cp -r "$OUT" "$SERVER"

echo "Done: dist/token-signing-install, dist/mit-token-signing-install_$VERSION.zip (ERMS), dist/repos-server (repos.mit-consulting.cz)"
