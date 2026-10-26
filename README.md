# Chrome Native Client plugin

![European Regional Development Fund](https://github.com/e-gov/RIHA-Frontend/raw/master/logo/EU/EU.png "European Regional Development Fund - DO NOT REMOVE THIS IMAGE BEFORE 05.03.2020")

**Now available from [Chrome Web Store](https://chrome.google.com/webstore/detail/ckjefchnfjhjfedoccjbhjpbncimppeg)**
**Now available from [Windows Store](https://microsoftedge.microsoft.com/addons/detail/fofaekogmodbjplbmlbmjiglndceaajh)**

**Info: Firefox version 50 supports Chrome extensions and is implemented in version 1.0.4.**

 * License: LGPL 2.1
 * &copy; Estonian Information System Authority

## Building
[![Build Status](https://travis-ci.com/open-eid/chrome-token-signing.svg?branch=master)](https://travis-ci.com/open-eid/chrome-token-signing)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/open-eid/chrome-token-signing?branch=master&svg=true)](https://ci.appveyor.com/project/open-eid/chrome-token-signing)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/2449/badge.svg)](https://scan.coverity.com/projects/2449)

1. Install dependencies

   1.0 blackPanther OS

        installing qtbase5-common-devel libopenssl-devel libpcsclite-devel libqt5core-devel libqt5network-devel libqt5widgets-devel
         (or binary package from official repository : installing chrome-token-signing)

   1.1 Ubuntu

        sudo apt-get install qtbase5-dev libssl-dev libpcsclite-dev qt5-default

   1.2 Windows

     * [Visual Studio Community 2015/2017/2019](https://www.visualstudio.com/downloads/)

   1.3 OSX

     * [XCode](https://itunes.apple.com/en/app/xcode/id497799835?mt=12)

        
2. Fetch the source

        git clone --recursive https://github.com/open-eid/chrome-token-signing
        cd chrome-token-signing

3. Build locally

    3.1 Linux (Docker, from Linux or Git Bash on Windows)

        build/linux/build.sh [build number]

    Builds dist/mit-token-signing_<version>_amd64.deb in Ubuntu 22.04 container
    (runs on Ubuntu 22.04+ and Debian 12+) and smoke tests the installed package.

    3.2 Windows

        nmake
        nmake pkg-unsigned

    3.3 OSX

        cd host-osx
        make build      # universal arm64 + x86_64 app in host-osx/tmp
        make test       # smoke test of native messaging protocol, no card needed
        make pkg        # unsigned mit-token-signing_<version>.pkg

    Signed and notarized package needs Apple Developer ID Application and Installer
    certificates in the keychain and notarytool credentials:

        xcrun notarytool store-credentials mit-token-signing --apple-id <apple id> --team-id <team id>
        make notarize SIGNER="M.I.T. Consulting, s.r.o. (<team id>)"

    The package installs the host to `/Library/Application Support/MIT Token Signing`
    and registers `cz.mit.consulting` for Chrome, Edge and Chromium.

## PKCS#11 modules on macOS and Linux

Tokens are matched by ATR first. Czech tokens (eObčanka, I.CA SecureStore,
SafeNet eToken / PostSignum, Monet+ ProID+) are found by probing installed vendor modules,
the first module reporting a token with a certificate is used. Additional
modules can be listed one per line (`#` starts a comment) in

 * macOS: `/Library/Application Support/MIT Token Signing/pkcs11-modules.conf`
 * Linux: `/etc/mit-token-signing/pkcs11-modules.conf`

## Batch signing

Extension 1.1.0 adds `signBatch()`, which signs many hashes with one PIN entry on hosts
reporting `batch` in the VERSION response (1.2.0, up to 100 hashes per request). On older
hosts it falls back to one `sign()` per hash with the PIN dialog every time.

The hosts sign a batch through the PKCS#11 module of the token vendor, on Windows too
(`SIGN` there keeps using the Windows certificate store). Qualified keys with a digital
signature PIN get the token PIN first; only when the token rejects it, the digital signature
PIN is asked and remembered per certificate as different, so the next batch asks both at once.

```js
const ts = new TokenSigning();
const cert = await ts.getCertificate({lang: "cs"});
// hashes: [{hex, type: "SHA-256"}, ...], resolves with [{hex}] in the same order
const signatures = typeof ts.signBatch === "function"
    ? await ts.signBatch(cert, hashes, {lang: "cs", info: "..."})
    : await signOneByOne(ts, cert, hashes); // extension older than 1.1.0
```

The native message is `{"type": "SIGN_BATCH", "cert": ..., "hashes": [hex, ...], "hashtype", "lang", "info"}`,
the response `{"signatures": [hex, ...]}`. The batch is all or nothing.

## Support
Official builds are provided through official distribution point [id.ee](https://www.id.ee/en/article/install-id-software/). If you want support, you need to be using official builds.

Source code is provided on "as is" terms with no warranty (see license for more information). Do not file Github issues with generic support requests.
Contact our support via [www.id.ee](http://www.id.ee) for assistance.
