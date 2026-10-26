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

# Sends the same empty message the extension uses to detect the native host
# and expects "invalid_argument". Does not need a card or reader.
#
# Usage: python3 smoke-test.py <path to native host executable>

import json
import struct
import subprocess
import sys


def main():
    if len(sys.argv) != 2:
        print("Usage: %s <native host executable>" % sys.argv[0])
        return 2
    msg = json.dumps({}).encode("utf-8")
    proc = subprocess.run([sys.argv[1]], input=struct.pack("=I", len(msg)) + msg,
                          stdout=subprocess.PIPE, timeout=30)
    if len(proc.stdout) < 4:
        print("FAIL: no response, exit code %d" % proc.returncode)
        return 1
    size = struct.unpack("=I", proc.stdout[:4])[0]
    response = json.loads(proc.stdout[4:4 + size].decode("utf-8"))
    print("Response: %s" % response)
    if response.get("result") != "invalid_argument":
        print("FAIL: expected result invalid_argument")
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
