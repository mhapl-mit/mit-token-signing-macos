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

# Waits for a window of the host, takes a screenshot and answers it with the given keys.
# osascript needs the Accessibility permission, its errors go to osascript.log.
answer() {
    local name=$1 keys=$2 host count
    for _ in $(seq 30); do
        host=$(pgrep -n -f MacOS/chrome-token-signing)
        count=$(osascript -e 'tell application "System Events" to count windows of (first process whose unix id is '"$host"')' 2>>"$OUT/osascript.log")
        if [ -n "$count" ] && [ "$count" != 0 ]; then
            sleep 1
            screencapture -x "$OUT/$name.png"
            osascript -e 'tell application "System Events" to tell (first process whose unix id is '"$host"') to set frontmost to true'                 -e 'tell application "System Events" to '"$keys" 2>>"$OUT/osascript.log"
            return 0
        fi
        sleep 1
    done
    screencapture -x "$OUT/$name-not-found.png"
    echo "no $name window"
    return 1
}

set +e
answer 1-certificate 'key code 36'
sleep 2
answer 2-pin 'keystroke "'$PIN'"' && osascript -e 'tell application "System Events" to key code 36' 2>>"$OUT/osascript.log"
sleep 1
screencapture -x "$OUT/3-progress.png"

# Stops a host left waiting in a dialog
for _ in $(seq 60); do
    kill -0 $TEST 2>/dev/null || break
    sleep 1
done
pkill -f MacOS/chrome-token-signing

STATUS=0
wait $TEST || STATUS=$?
cat "$OUT/sign-test.log"
cp ~/tmp/chrome-signing.log "$OUT/host.log"
exit $STATUS
