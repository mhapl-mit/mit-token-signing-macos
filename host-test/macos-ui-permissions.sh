#!/bin/bash
# Allows the UI test (macos-ui-test.sh) to click into the dialogs and take screenshots on a CI Mac:
# grants Accessibility and Screen Recording to this shell and every process above it, the
# responsible process macOS asks for. Works only with System Integrity Protection disabled
# (GitHub runners have the permissions already), otherwise it only says so.
#
# Usage: host-test/macos-ui-permissions.sh

if ! csrutil status | grep -q disabled; then
    csrutil status
    echo "System Integrity Protection is on, the permissions cannot be granted"
    exit 0
fi

CLIENTS="/bin/bash /bin/sh /bin/zsh /usr/sbin/screencapture"
pid=$$
while [ "$pid" -gt 1 ]; do
    CLIENTS="$CLIENTS $(ps -o comm= -p "$pid")"
    pid=$(ps -o ppid= -p "$pid" | tr -d ' ')
done

for db in "/Library/Application Support/com.apple.TCC/TCC.db" "$HOME/Library/Application Support/com.apple.TCC/TCC.db"; do
    [ -f "$db" ] || continue
    for client in $CLIENTS; do
        case $client in /*) ;; *) client=$(command -v "$client") || continue ;; esac
        for service in kTCCServiceAccessibility kTCCServiceScreenCapture kTCCServicePostEvent; do
            sudo sqlite3 "$db" "INSERT OR REPLACE INTO access
                (service, client, client_type, auth_value, auth_reason, auth_version, flags)
                VALUES ('$service', '$client', 1, 2, 4, 1, 0)" && echo "$service: $client"
        done
    done
done
