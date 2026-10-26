#
# Chrome Token Signing Native Host
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

# Interactive test with a real token: selects a signing certificate, signs
# a SHA-256 hash (or a batch of hashes with SIGN_BATCH) and verifies the RSA
# signatures with openssl, the same way the extension talks to the native host.
#
# Usage: python3 sign-test.py <path to native host executable> [--batch <count>] [--info <text>]
# Without --info no information dialog is shown, like ERMS does not send it.

import hashlib
import json
import os
import struct
import subprocess
import sys
import tempfile


def send(proc, msg):
    # Chrome sends non-ASCII characters as UTF-8, not as \u escapes
    data = json.dumps(msg, ensure_ascii=False).encode("utf-8")
    proc.stdin.write(struct.pack("=I", len(data)) + data)
    proc.stdin.flush()


def receive(proc):
    header = proc.stdout.read(4)
    if len(header) < 4:
        raise RuntimeError("native host closed the connection")
    size = struct.unpack("=I", header)[0]
    return json.loads(proc.stdout.read(size).decode("utf-8"))


def verify(cert_hex, hash_bytes, signature_hex, show_cert=True):
    with tempfile.TemporaryDirectory() as tmp:
        files = {name: os.path.join(tmp, name) for name in ("cert.der", "pub.pem", "hash", "sig")}
        open(files["cert.der"], "wb").write(bytes.fromhex(cert_hex))
        open(files["hash"], "wb").write(hash_bytes)
        open(files["sig"], "wb").write(bytes.fromhex(signature_hex))
        if show_cert:
            subprocess.run(["openssl", "x509", "-inform", "DER", "-in", files["cert.der"],
                            "-noout", "-subject", "-enddate"], check=True)
        subprocess.run(["openssl", "x509", "-inform", "DER", "-in", files["cert.der"],
                        "-pubkey", "-noout", "-out", files["pub.pem"]], check=True)
        result = subprocess.run(["openssl", "pkeyutl", "-verify", "-pubin", "-inkey", files["pub.pem"],
                                 "-in", files["hash"], "-sigfile", files["sig"],
                                 "-pkeyopt", "digest:sha256"], stdout=subprocess.DEVNULL)
        return result.returncode == 0


def main():
    args = sys.argv[1:]
    count = 1
    info = None
    try:
        exe = args.pop(0)
        while args:
            option, value = args.pop(0), args.pop(0)
            if option == "--batch":
                count = int(value)
            elif option == "--info":
                info = value
            else:
                raise ValueError(option)
    except (IndexError, ValueError):
        print("Usage: %s <native host executable> [--batch <count>] [--info <text>]" % sys.argv[0])
        return 2
    proc = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    origin = "https://test.mit-consulting.cz"
    hashes = [hashlib.sha256(b"MIT Token Signing test %d" % i).digest() for i in range(count)]
    try:
        send(proc, {"type": "VERSION", "nonce": "1", "origin": origin})
        print("VERSION: %s" % receive(proc))

        print("Select a signing certificate in the dialog...")
        send(proc, {"type": "CERT", "nonce": "2", "origin": origin, "lang": "cs", "filter": "SIGN"})
        cert = receive(proc)
        if cert.get("result") != "ok":
            print("FAIL: CERT returned %s" % cert)
            return 1

        print("Enter the PIN in the dialog...")
        if count == 1:
            request = {"type": "SIGN", "nonce": "3", "origin": origin, "lang": "cs",
                       "cert": cert["cert"], "hash": hashes[0].hex(), "hashtype": "SHA-256"}
        else:
            request = {"type": "SIGN_BATCH", "nonce": "3", "origin": origin, "lang": "cs",
                       "cert": cert["cert"], "hashes": [h.hex() for h in hashes], "hashtype": "SHA-256"}
        if info:
            request["info"] = info
        send(proc, request)
        response = receive(proc)
        if response.get("result") != "ok":
            print("FAIL: signing returned %s" % response)
            return 1
    finally:
        proc.stdin.close()
        proc.wait(timeout=30)

    signatures = [response["signature"]] if count == 1 else response["signatures"]
    if len(signatures) != count:
        print("FAIL: expected %d signatures, got %d" % (count, len(signatures)))
        return 1
    for i, (hash_bytes, signature) in enumerate(zip(hashes, signatures)):
        if not verify(cert["cert"], hash_bytes, signature, i == 0):
            print("FAIL: signature %d does not verify (EC keys are not supported by this check)" % (i + 1))
            return 1
    print("OK: %d signature(s) verified" % count)
    return 0


if __name__ == "__main__":
    sys.exit(main())
