#!/bin/sh
# Runs inside the build container: /src is the read-only repository, /dist the output directory.
set -e

BUILD_NUMBER=${BUILD_NUMBER:-0}
rm -rf /build && mkdir /build
cd /src && tar --exclude=./.git --exclude=./dist --exclude=./.idea -cf - . | tar -C /build -xf -
cd /build

MAJOR=$(sed -n 's/^MAJOR_VERSION=//p' VERSION.mk | tr -d '\r')
MINOR=$(sed -n 's/^MINOR_VERSION=//p' VERSION.mk | tr -d '\r')
RELEASE=$(sed -n 's/^RELEASE_VERSION=//p' VERSION.mk | tr -d '\r')
VERSION="$MAJOR.$MINOR.$RELEASE.$BUILD_NUMBER"
echo "Building mit-token-signing $VERSION"

# Files checked out on Windows may have CRLF line endings
find debian host-linux -type f -exec sed -i 's/\r$//' {} +
# and are all executable when mounted from Windows, qmake would then strip the json manifests
find . -type f -exec chmod 644 {} +
chmod +x debian/rules

export DEBFULLNAME="M.I.T. Consulting, s.r.o." DEBEMAIL="mhapl@mit-consulting.cz"
dch --newversion "$VERSION" --distribution unstable --force-distribution "Release $VERSION."
dpkg-buildpackage -us -uc -b

DEB=$(ls /mit-token-signing_"$VERSION"_*.deb)
echo "Smoke test of $DEB"
apt-get update -qq && apt-get install -y -qq "$DEB" > /dev/null
QT_QPA_PLATFORM=offscreen python3 host-test/smoke-test.py /usr/lib/mit-token-signing/chrome-token-signing

cp "$DEB" /dist/
echo "Done: dist/$(basename "$DEB")"
