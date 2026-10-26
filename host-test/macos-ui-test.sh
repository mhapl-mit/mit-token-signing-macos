#!/bin/bash
# Test of the installed macOS native host without a smart card, for a Mac in CI: creates a SoftHSM
# token with a signing certificate, signs a batch with sign-test.py, confirms the dialogs through
# System Events and keeps screenshots of them.
#
# Usage: host-test/macos-ui-test.sh <installer package> [output directory]
set -e

PKG=$1
OUT=${2:-ui-test}
PIN=1234
HERE=$(cd "$(dirname "$0")" && pwd)
APP_DIR="/Library/Application Support/MIT Token Signing"
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

brew install softhsm opensc >/dev/null
SOFTHSM="$(brew --prefix softhsm)/lib/softhsm/libsofthsm2.so"

# Token with an RSA key and a self-signed certificate for signing (non-repudiation)
softhsm2-util --init-token --free --label "MIT Test" --so-pin 87654321 --pin $PIN
WORK=$(mktemp -d)
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$WORK/key.pem" -out "$WORK/cert.pem" -days 30 \
    -utf8 -subj "/CN=Testovací Podepisovatel/O=M.I.T. Consulting" \
    -addext "keyUsage=critical,nonRepudiation,digitalSignature" 2>/dev/null
openssl pkcs8 -topk8 -nocrypt -in "$WORK/key.pem" -outform DER -out "$WORK/key.der"
openssl x509 -in "$WORK/cert.pem" -outform DER -out "$WORK/cert.der"
pkcs11-tool --module "$SOFTHSM" --login --pin $PIN --write-object "$WORK/key.der" --type privkey --id 01 --label signer
pkcs11-tool --module "$SOFTHSM" --login --pin $PIN --write-object "$WORK/cert.der" --type cert --id 01 --label signer

sudo installer -pkg "$PKG" -target /
sudo mkdir -p "$APP_DIR"
echo "$SOFTHSM" | sudo tee "$APP_DIR/pkcs11-modules.conf" >/dev/null
EXE="$APP_DIR/chrome-token-signing.app/Contents/MacOS/chrome-token-signing"
codesign -dvv --entitlements - "$APP_DIR/chrome-token-signing.app" >"$OUT/codesign.txt" 2>&1 || true
# The host logs only into an existing file
mkdir -p ~/tmp
: >~/tmp/chrome-signing.log

python3 "$HERE/sign-test.py" "$EXE" --batch 5 >"$OUT/sign-test.log" 2>&1 &
TEST=$!

UI="$WORK/macos-ui"
swiftc -O "$HERE/macos-ui.swift" -o "$UI"

# Waits for a window of the host, takes a screenshot, clicks its title bar to activate it
# and runs the given macos-ui commands, e.g. "type 1234" "return"
answer() {
    local name=$1 host geometry x y width height
    shift
    for _ in $(seq 30); do
        # Anchored, sign-test.py has the host path in its command line too
        host=$(pgrep -n -f "^$EXE")
        if [ -n "$host" ] && geometry=$("$UI" window "$host"); then
            read -r x y width height <<<"$geometry"
            echo "$name: host $host, window $geometry" >>"$OUT/ui.log"
            sleep 1
            screencapture -x "$OUT/$name.png"
            "$UI" click $((x + width / 2)) $((y + 12))
            for command in "$@"; do
                # shellcheck disable=SC2086
                "$UI" $command
            done
            return 0
        fi
        sleep 1
    done
    screencapture -x "$OUT/$name-not-found.png"
    echo "no $name window"
    return 1
}

set +e
answer 1-certificate return
sleep 2
answer 2-pin "type $PIN" return
sleep 1
screencapture -x "$OUT/3-progress.png"

# Stops a host left waiting in a dialog
for _ in $(seq 60); do
    kill -0 $TEST 2>/dev/null || break
    sleep 1
done
pkill -f "^$EXE"

STATUS=0
wait $TEST || STATUS=$?
cat "$OUT/sign-test.log"
cp ~/tmp/chrome-signing.log "$OUT/host.log"
exit $STATUS
