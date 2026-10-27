/*
 * Chrome Token Signing Native Host
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// Finds out what the user needs to sign with a card in this browser: a supported browser,
// the extension and the native host (MIT Token Signing). Used by the install guide
// (install/index.html) and by web applications before signing:
//
//   TokenSigningCheck.detect({latestVersion: "1.2.0.13"}).then(function(state) {
//       if (state.status !== "ok") { show a link to install/index.html }
//   });
//
// state.status is the first missing step: "unsupported_os", "unsupported_browser",
// "no_extension", "no_host" or "ok". Outdated parts that still work are listed in
// state.updates ("extension", "host"), signBatch() then signs one by one.
(function(global) {
    "use strict";

    var STORE = {
        chrome: "https://chromewebstore.google.com/detail/mit-consulting-token-sign/hkckiflbaedpdigcjoeiacmfnlncpeoj",
        // Edge installs the extension from the Chrome Web Store, the add-on in the Edge store
        // (ajndpmmgagjlfgoegeadcgkiehlbcinl) stays at 0.0.6 until its publisher account is recovered
        edge: "https://chromewebstore.google.com/detail/mit-consulting-token-sign/hkckiflbaedpdigcjoeiacmfnlncpeoj"
    };

    // Installer of the native host for every OS, relative to the install guide
    var FILES = {
        "windows-x64": "mit-token-signing-x64.msi",
        "windows-x86": "mit-token-signing-x86.msi",
        "macos": "mit-token-signing.pkg",
        "linux": "mit-token-signing_amd64.deb"
    };

    // Host version with SIGN_BATCH
    var BATCH_VERSION = "1.2.0";

    function platform() {
        var ua = navigator.userAgent;
        var os = "other", arch = "x64";
        if (/Windows/.test(ua)) {
            os = "windows";
            // Chromium reports "Win64; x64" on 64-bit Windows (ARM too, it runs x64 programs)
            arch = /Win64|WOW64/.test(ua) ? "x64" : "x86";
        } else if (/Macintosh|Mac OS X/.test(ua) && !/iPhone|iPad/.test(ua)) {
            os = "macos";
        } else if (/Linux/.test(ua) && !/Android/.test(ua) && !/CrOS/.test(ua)) {
            os = "linux";
        }
        var browser = "other";
        if (/Edg\//.test(ua)) {
            browser = "edge";
        } else if (/OPR\/|Opera/.test(ua)) {
            browser = "opera";
        } else if (/Firefox\//.test(ua)) {
            browser = "firefox";
        } else if (/Chrome\//.test(ua)) {
            browser = "chrome";
        } else if (/Safari\//.test(ua)) {
            browser = "safari";
        }
        return {os: os, arch: arch, browser: browser};
    }

    // Compares dotted versions, missing parts are 0
    function compare(a, b) {
        var x = String(a).split("."), y = String(b).split(".");
        for (var i = 0; i < Math.max(x.length, y.length); i++) {
            var d = (parseInt(x[i], 10) || 0) - (parseInt(y[i], 10) || 0);
            if (d) {
                return d < 0 ? -1 : 1;
            }
        }
        return 0;
    }

    // The extension injects page.js after the page loads, wait for TokenSigning a while
    function waitForExtension(timeout) {
        return new Promise(function(resolve) {
            var start = Date.now();
            (function poll() {
                if (typeof global.TokenSigning === "function") {
                    resolve(true);
                } else if (Date.now() - start >= timeout) {
                    resolve(false);
                } else {
                    setTimeout(poll, 100);
                }
            })();
        });
    }

    function detect(options) {
        options = options || {};
        var p = platform();
        var state = {
            os: p.os,
            arch: p.arch,
            browser: p.browser,
            extension: false,
            extensionVersion: null,
            host: false,
            hostVersion: null,
            batch: false,
            updates: [],
            status: "ok",
            store: STORE[p.browser] || null,
            download: p.os === "windows" ? FILES["windows-" + p.arch] : (FILES[p.os] || null)
        };
        if (p.os === "other") {
            state.status = "unsupported_os";
            return Promise.resolve(state);
        }
        if (!state.store) {
            state.status = "unsupported_browser";
            return Promise.resolve(state);
        }
        return waitForExtension(options.timeout || 3000).then(function(found) {
            if (!found) {
                state.status = "no_extension";
                return state;
            }
            state.extension = true;
            var signing = new global.TokenSigning();
            return signing.getVersion().then(function(versions) {
                // "<extension>/<host>"
                var parts = String(versions).split("/");
                state.extensionVersion = parts[0] || null;
                state.hostVersion = parts[1] || null;
                state.host = true;
                state.batch = typeof signing.signBatch === "function" && compare(state.hostVersion, BATCH_VERSION) >= 0;
                if (typeof signing.signBatch !== "function") {
                    state.updates.push("extension");
                }
                if (options.latestVersion && compare(state.hostVersion, options.latestVersion) < 0) {
                    state.updates.push("host");
                }
                return state;
            }, function(error) {
                // no_implementation: the host is not installed or not allowed for this extension
                state.status = "no_host";
                state.error = error && error.message ? error.message : String(error);
                return state;
            });
        });
    }

    global.TokenSigningCheck = {
        detect: detect,
        compare: compare,
        files: FILES,
        store: STORE
    };
})(window);
