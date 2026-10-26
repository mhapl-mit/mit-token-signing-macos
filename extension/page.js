/*
 * Chrome token signing extension
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

(function() {
    // Promises
    let _eid_promises = {};
    // Turn the incoming message from extension
    // into pending Promise resolving
    window.addEventListener("message", function(event) {
        if (event.source !== window) return;
        if (event.data.src && (event.data.src === "background.js")) {
            console.log("Page received: ");
            console.log(event.data);
            // Get the promise
            if (event.data.nonce) {
                let p = _eid_promises[event.data.nonce];
                // resolve
                if (event.data.result === "ok") {
                    if (p.raw) {
                        p.resolve(event.data);
                    } else if (event.data.signatures !== undefined) {
                        p.resolve(event.data.signatures.map(function(signature) {
                            return {hex: signature};
                        }));
                    } else if (event.data.signature !== undefined) {
                        p.resolve({hex: event.data.signature});
                    } else if (event.data.version !== undefined) {
                        p.resolve(event.data.extension + "/" + event.data.version);
                    } else if (event.data.cert !== undefined) {
                        p.resolve({hex: event.data.cert});
                    } else {
                        console.log("No idea how to handle message");
                        console.log(event.data);
                    }
                } else {
                    // reject
                    p.reject(new Error(event.data.result));
                }
                delete _eid_promises[event.data.nonce];
            } else {
                console.log("No nonce in event msg");
            }
        }
    }, false);

    window.TokenSigning = function() {
        function nonce() {
            let val = "";
            const hex = "abcdefghijklmnopqrstuvwxyz0123456789";
            for (let i = 0; i < 16; i++) val += hex.charAt(Math.floor(Math.random() * hex.length));
            return val;
        }

        // raw resolves with the whole response message instead of its value
        function messagePromise(msg, raw) {
            return new Promise(function(resolve, reject) {
                // amend with necessary metadata
                msg["nonce"] = nonce();
                msg["src"] = "page.js";
                // send message
                window.postMessage(msg, "*");
                // and store promise callbacks
                _eid_promises[msg.nonce] = {
                    resolve: resolve,
                    reject: reject,
                    raw: raw
                };
            });
        }

        // Runs promise returning function for every item one after another, resolves with all results
        function sequence(items, fn) {
            return items.reduce(function(chain, item) {
                return chain.then(function(results) {
                    return fn(item).then(function(result) {
                        return results.concat(result);
                    });
                });
            }, Promise.resolve([]));
        }
        this.getCertificate = function(options) {
            const msg = {type: "CERT", lang: options.lang, filter: options.filter};
            console.log("getCertificate()");
            return messagePromise(msg);
        };
        this.sign = function(cert, hash, options) {
            const msg = {
                type: "SIGN",
                cert: cert.hex,
                hash: hash.hex,
                hashtype: hash.type,
                lang: options.lang,
                info: options.info
            };
            console.log("sign()");
            return messagePromise(msg);
        };
        // Signs hashes with one PIN entry per batch of at most 100 hashes, resolves with
        // signatures [{hex}] in the same order. Native hosts without batch support ask
        // for the PIN for every hash.
        this.signBatch = function(cert, hashes, options) {
            const self = this;
            options = options || {};
            if (!Array.isArray(hashes) || hashes.length === 0) {
                return Promise.reject(new Error("invalid_argument"));
            }
            console.log("signBatch() " + hashes.length);
            return messagePromise({type: "VERSION"}, true).then(function(version) {
                const max = version.batch || 0;
                if (!max) {
                    return sequence(hashes, function(hash) {
                        return self.sign(cert, hash, options).then(function(signature) {
                            return [signature];
                        });
                    });
                }
                const batches = [];
                for (let i = 0; i < hashes.length; i += max) {
                    batches.push(hashes.slice(i, i + max));
                }
                return sequence(batches, function(batch) {
                    return messagePromise({
                        type: "SIGN_BATCH",
                        cert: cert.hex,
                        hashes: batch.map(function(hash) {
                            return hash.hex;
                        }),
                        hashtype: batch[0].type,
                        lang: options.lang,
                        info: options.info
                    });
                });
            });
        };
        this.getVersion = function() {
            console.log("getVersion()");
            return messagePromise({
                type: "VERSION"
            });
        };
    };
})();