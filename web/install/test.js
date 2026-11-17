// Signing test: selects a certificate, signs SHA-256 hashes of test data with sign() and
// signBatch() and verifies the signatures with WebCrypto against the public key of the certificate
(function() {
    "use strict";

    var BATCH_COUNT = 3;
    var ERRORS = {
        user_cancel: "Zrušeno uživatelem.",
        no_certificates: "Nenalezen žádný certifikát pro podpis. Je karta zasunutá ve čtečce a je nainstalovaný ovladač ke kartě?",
        driver_error: "Chyba ovladače karty. Zkontrolujte, že je nainstalovaný ovladač od vydavatele karty, karta je zasunutá a čtečka připojená (případně ji zkuste zapojit do jiného USB portu).",
        pin_blocked: "PIN je zablokovaný. Odblokujte ho programem od vydavatele karty.",
        pin_invalid: "Chybný PIN.",
        pin_format_error: "PIN nemá správný formát.",
        not_allowed: "Podpis z této stránky není povolený.",
        no_implementation: "Program MIT Token signing není nainstalovaný.",
        invalid_argument: "Neplatný požadavek.",
        technical_error: "Technická chyba programu MIT Token signing."
    };

    function step(id, cls, text) {
        var el = document.getElementById(id);
        el.classList.remove("ok", "missing", "update");
        if (cls) {
            el.classList.add(cls);
        }
        el.querySelector(".state").textContent = text;
    }

    function errorText(error) {
        var code = error && error.message ? error.message : String(error);
        return (ERRORS[code] || "Chyba: " + code) + (ERRORS[code] ? " (" + code + ")" : "");
    }

    // --- DER / X.509 -------------------------------------------------------------------

    function fromHex(hex) {
        var bytes = new Uint8Array(hex.length / 2);
        for (var i = 0; i < bytes.length; i++) {
            bytes[i] = parseInt(hex.substr(i * 2, 2), 16);
        }
        return bytes;
    }

    function toHex(bytes) {
        var hex = "";
        bytes = new Uint8Array(bytes);
        for (var i = 0; i < bytes.length; i++) {
            hex += (bytes[i] < 16 ? "0" : "") + bytes[i].toString(16);
        }
        return hex;
    }

    // One DER element at pos: tag, content start and end, element end
    function element(der, pos) {
        var tag = der[pos], len = der[pos + 1], start = pos + 2;
        if (len & 0x80) {
            var n = len & 0x7f;
            len = 0;
            for (var i = 0; i < n; i++) {
                len = len * 256 + der[start + i];
            }
            start += n;
        }
        if (start + len > der.length) {
            throw new Error("DER");
        }
        return {der: der, tag: tag, header: pos, start: start, end: start + len};
    }

    function children(el) {
        var list = [], pos = el.start;
        while (pos < el.end) {
            var child = element(el.der, pos);
            list.push(child);
            pos = child.end;
        }
        return list;
    }

    function content(el) {
        return el.der.subarray(el.start, el.end);
    }

    function text(el) {
        var bytes = content(el);
        if (el.tag === 0x1e) {
            var s = "";
            for (var i = 0; i + 1 < bytes.length; i += 2) {
                s += String.fromCharCode(bytes[i] * 256 + bytes[i + 1]);
            }
            return s;
        }
        return new TextDecoder().decode(bytes);
    }

    // Name: the value of the attribute with the given OID (hex), e.g. 550403 = commonName
    function attribute(name, oid) {
        var found = null;
        children(name).forEach(function(set) {
            children(set).forEach(function(pair) {
                var parts = children(pair);
                if (parts.length === 2 && toHex(content(parts[0])) === oid) {
                    found = text(parts[1]);
                }
            });
        });
        return found;
    }

    function time(el) {
        var s = text(el);
        var year = el.tag === 0x17 ? 2000 + parseInt(s.substr(0, 2), 10) : parseInt(s.substr(0, 4), 10);
        if (el.tag === 0x17 && year >= 2050) {
            year -= 100;
        }
        s = el.tag === 0x17 ? s.substr(2) : s.substr(4);
        return new Date(Date.UTC(year, parseInt(s.substr(0, 2), 10) - 1, parseInt(s.substr(2, 2), 10),
            parseInt(s.substr(4, 2), 10), parseInt(s.substr(6, 2), 10), parseInt(s.substr(8, 2), 10) || 0));
    }

    var CURVES = {"2a8648ce3d030107": "P-256", "2b81040022": "P-384", "2b81040023": "P-521"};

    function parseCertificate(hex) {
        var der = fromHex(hex);
        var tbs = children(children(element(der, 0))[0]);
        if (tbs[0].tag === 0xa0) {
            tbs.shift();
        }
        // serial, signature algorithm, issuer, validity, subject, public key
        var validity = children(tbs[3]);
        var spki = tbs[5];
        var algorithm = children(children(spki)[0]);
        var oid = toHex(content(algorithm[0]));
        var key = null;
        if (oid === "2a864886f70d010101") {
            key = {name: "RSASSA-PKCS1-v1_5", hash: "SHA-256"};
        } else if (oid === "2a8648ce3d0201" && algorithm[1] && CURVES[toHex(content(algorithm[1]))]) {
            key = {name: "ECDSA", namedCurve: CURVES[toHex(content(algorithm[1]))]};
        }
        return {
            subject: attribute(tbs[4], "550403") || "?",
            issuer: attribute(tbs[2], "550403") || attribute(tbs[2], "55040a") || "?",
            notBefore: time(validity[0]),
            notAfter: time(validity[1]),
            spki: der.slice(spki.header, spki.end),
            key: key
        };
    }

    // ECDSA signatures from the card are r||s; a DER SEQUENCE {r, s} is converted to it
    function rawEcSignature(signature, curve) {
        var size = {"P-256": 32, "P-384": 48, "P-521": 66}[curve];
        if (signature.length === 2 * size || signature[0] !== 0x30) {
            return signature;
        }
        var raw = new Uint8Array(2 * size);
        children(element(signature, 0)).forEach(function(n, i) {
            var v = content(n);
            while (v.length > size && v[0] === 0) {
                v = v.subarray(1);
            }
            raw.set(v, (i + 1) * size - v.length);
        });
        return raw;
    }

    function verify(cert, data, signatureHex) {
        if (!cert.key || !window.crypto || !crypto.subtle) {
            return Promise.resolve(null);
        }
        var signature = fromHex(signatureHex);
        var params = cert.key.name === "ECDSA" ? {name: "ECDSA", hash: "SHA-256"} : cert.key.name;
        if (cert.key.name === "ECDSA") {
            signature = rawEcSignature(signature, cert.key.namedCurve);
        }
        return crypto.subtle.importKey("spki", cert.spki, cert.key, false, ["verify"]).then(function(key) {
            return crypto.subtle.verify(params, key, signature, data);
        }).catch(function() {
            // A key WebCrypto does not support, the signature is not verified
            return null;
        });
    }

    // --- Test --------------------------------------------------------------------------

    function testData(label) {
        return new TextEncoder().encode("Test podpisu MIT Token signing, " + label + ", "
            + new Date().toISOString() + ", " + Math.random());
    }

    function hashOf(data) {
        return crypto.subtle.digest("SHA-256", data).then(function(digest) {
            return {type: "SHA-256", hex: toHex(digest)};
        });
    }

    function formatDate(d) {
        return d.getDate() + ". " + (d.getMonth() + 1) + ". " + d.getFullYear();
    }

    function verifiedText(results, count) {
        if (results.indexOf(false) >= 0) {
            return null;
        }
        if (results.indexOf(null) >= 0) {
            return (count > 1 ? count + " podpisy vytvořeny" : "Podpis vytvořen") + ", typ klíče ale stránka ověřit neumí.";
        }
        return count > 1 ? count + " podpisy vytvořeny a ověřeny." : "Podpis vytvořen a ověřen.";
    }

    var state = null;

    function run() {
        var start = document.getElementById("start");
        var done = document.getElementById("done");
        // No info: the host would show it in an extra dialog before the PIN, the page says it instead
        var options = {lang: "cs"};
        var signing = new window.TokenSigning();
        var cert = null, certHex = null;
        start.disabled = true;
        done.style.display = "none";
        done.classList.remove("failed");
        document.getElementById("cert-detail").hidden = true;
        step("cert", null, "Vyberte certifikát v okně, které se otevřelo.");
        step("sign", null, "Po výběru certifikátu se zobrazí okno pro zadání PINu.");
        step("batch", null, state.batch ? "Tři podpisy s jedním zadáním PINu." : "Vyžaduje rozšíření 1.1.0 a program 1.2.0 nebo novější.");

        function fail(id, error) {
            step(id, "missing", errorText(error));
            done.textContent = "Test podpisu se nezdařil.";
            done.classList.add("failed");
            done.style.display = "block";
            start.disabled = false;
            start.textContent = "Spustit test znovu";
        }

        signing.getCertificate(options).then(function(c) {
            certHex = c.hex;
            try {
                cert = parseCertificate(certHex);
            } catch (e) {
                cert = {subject: "?", issuer: "?", key: null};
            }
            var now = new Date();
            var detail = document.getElementById("cert-detail");
            detail.textContent = cert.notAfter ? "Vydavatel " + cert.issuer + ", platný od " + formatDate(cert.notBefore)
                + " do " + formatDate(cert.notAfter) + "." : "";
            detail.hidden = !cert.notAfter;
            if (cert.notAfter && (cert.notAfter < now || cert.notBefore > now)) {
                step("cert", "update", "Vybrán certifikát " + cert.subject + ", ale není platný. Podpis se vyzkouší, "
                    + "pro skutečné podepisování vyberte platný certifikát.");
            } else {
                step("cert", "ok", "Vybrán certifikát " + cert.subject + ".");
            }
            step("sign", null, "Zadejte PIN v okně, které se otevřelo.");
            var data = testData("1");
            return hashOf(data).then(function(hash) {
                return signing.sign({hex: certHex}, hash, options);
            }).then(function(signature) {
                return verify(cert, data, signature.hex);
            }, function(error) {
                fail("sign", error);
                throw null;
            }).then(function(ok) {
                var t = verifiedText([ok], 1);
                if (!t) {
                    fail("sign", new Error("Podpis neodpovídá certifikátu."));
                    throw null;
                }
                step("sign", "ok", t);
            });
        }, function(error) {
            fail("cert", error);
            throw null;
        }).then(function() {
            if (!state.batch) {
                step("batch", null, "Přeskočeno, vyžaduje rozšíření 1.1.0 a program 1.2.0 nebo novější.");
                return;
            }
            step("batch", null, "Zadejte PIN v okně, které se otevřelo.");
            var data = [];
            for (var i = 0; i < BATCH_COUNT; i++) {
                data.push(testData("dávka " + (i + 1)));
            }
            return Promise.all(data.map(hashOf)).then(function(hashes) {
                return signing.signBatch({hex: certHex}, hashes, options);
            }).then(function(signatures) {
                return Promise.all(signatures.map(function(signature, i) {
                    return verify(cert, data[i], signature.hex);
                }));
            }, function(error) {
                fail("batch", error);
                throw null;
            }).then(function(results) {
                var t = results.length === BATCH_COUNT ? verifiedText(results, BATCH_COUNT) : null;
                if (!t) {
                    fail("batch", new Error("Podpisy neodpovídají certifikátu."));
                    throw null;
                }
                step("batch", "ok", t);
            });
        }).then(function() {
            done.textContent = "Podpis kartou funguje, můžete podepisovat.";
            done.style.display = "block";
            start.disabled = false;
            start.textContent = "Spustit test znovu";
        }, function(error) {
            if (error) {
                fail("sign", error);
            }
        });
    }

    function prerequisites() {
        var prereq = document.getElementById("prereq");
        var versions = document.getElementById("versions");
        if (!window.crypto || !crypto.subtle) {
            prereq.hidden = false;
            document.getElementById("prereq-text").textContent = "Test je potřeba otevřít přes zabezpečené připojení (https).";
            versions.textContent = "";
            return;
        }
        TokenSigningCheck.detect().then(function(s) {
            state = s;
            if (s.status !== "ok") {
                var missing = {
                    unsupported_os: "Tento systém není podporován.",
                    unsupported_browser: "Podpis kartou funguje jen v prohlížečích Google Chrome a Microsoft Edge.",
                    no_extension: "V prohlížeči chybí rozšíření M.I.T. Consulting Token signing.",
                    no_host: "V počítači chybí program MIT Token signing."
                }[s.status];
                prereq.hidden = false;
                document.getElementById("prereq-text").textContent = missing + " Doinstalujte ho podle průvodce a test spusťte znovu.";
                versions.textContent = "";
                return;
            }
            versions.textContent = "Rozšíření " + s.extensionVersion + " · program " + s.hostVersion;
            var start = document.getElementById("start");
            start.disabled = false;
            start.onclick = run;
            if (!s.batch) {
                step("batch", null, "Vyžaduje rozšíření 1.1.0 a program 1.2.0 nebo novější, test ho přeskočí.");
            }
        });
    }

    prerequisites();
})();
