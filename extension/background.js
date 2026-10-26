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

const NO_NATIVE_URL = "https://repos.mit-consulting.cz/token-sign/index.html";
const DEVELOPER_URL = "https://repos.mit-consulting.cz/token-sign/forbidden.html";

const NATIVE_HOST = "cz.mit.consulting";

const K_SRC = "src";
const K_NONCE = "nonce";
const K_RESULT = "result";
const K_TAB = "tab";
const K_EXTENSION = "extension";

const PERMISSIONS = "Access to the specified native messaging host is forbidden.";
const MISSING_MESSAGE = "Specified native messaging host not found.";

let ports = {};
let missing = true;

console.log("Background service worker activated");

// Force kill of native process
// Becasue Port.disconnect() does not work
function _killPort(tab) {
	if (tab in ports) {
		console.log("KILL " + tab);
		// Force killing with an empty message
		ports[tab].postMessage({});
	}
}

// Check if native implementation is OK resolves with "ok", "missing" or "forbidden"
function _testNativeComponent() {
	return new Promise(function(resolve, reject) {
		chrome.runtime.sendNativeMessage(NATIVE_HOST, {}, function(response) {
			if (!response) {
				console.log("TEST: ERROR " + JSON.stringify(chrome.runtime.lastError));
				if (chrome.runtime.lastError.message === PERMISSIONS) {
					resolve("forbidden");
				} else if (chrome.runtime.lastError.message === MISSING_MESSAGE) {
					resolve("missing");
				} else {
					resolve("missing");
				}
			} else {
				console.log("TEST: " + JSON.stringify(response));
				if (response["result"] === "invalid_argument") {
					resolve("ok");
				} else {
					resolve("missing"); // Může být upraveno podle potřeby
				}
			}
		});
	});
}

// Při instalaci nebo aktualizaci rozšíření
chrome.runtime.onInstalled.addListener(function(details) {
	if (details.reason === "install" || details.reason === "update") {
		_testNativeComponent().then(function(result) {
			let url = null;
			if (result === "ok" && details.reason === "install") {
				missing = false;
				// Zde můžete přidat uvítací stránku
			} else if (result === "forbidden") {
				url = DEVELOPER_URL;
			} else if (result === "missing") {
				url = NO_NATIVE_URL;
			}
			if (url) {
				chrome.tabs.create({ 'url': url });
			}
		});
	}
});

// Příjem zpráv z obsahových skriptů
chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
	if (sender.id !== chrome.runtime.id && sender.extensionId !== chrome.runtime.id) {
		console.log('WARNING: Ignoring message not from our extension');
		return false;
	}
	if (sender.tab) {
		if (request["type"] === "DONE") {
			console.log("DONE " + sender.tab.id);
			if (sender.tab.id in ports) {
				_killPort(sender.tab.id);
			}
		} else {
			request[K_TAB] = sender.tab.id;
			if (missing) {
				_testNativeComponent().then(function(result) {
					if (result === "ok") {
						missing = false;
						_forward(request);
					} else {
						_fail_with(request, "no_implementation");
					}
				});
			} else {
				_forward(request);
			}
		}
	}
	return true;
});

// Odeslání odpovědi zpět do obsahového skriptu
function _reply(tab, msg) {
	msg[K_SRC] = "background.js";
	msg[K_EXTENSION] = chrome.runtime.getManifest().version;
	chrome.tabs.sendMessage(tab, msg);
}

// Odeslání chyby zpět do obsahového skriptu
function _fail_with(msg, result) {
	let resp = {};
	resp[K_NONCE] = msg[K_NONCE];
	resp[K_RESULT] = result;
	_reply(msg[K_TAB], resp);
}

// Přeposlání zprávy nativní komponentě
function _forward(message) {
	const tabid = message[K_TAB];
	console.log("SEND " + tabid + ": " + JSON.stringify(message));

	if(!ports[tabid]) {
		// create a new port
		const port = chrome.runtime.connectNative(NATIVE_HOST);
		if (!port) {
			console.log("OPEN ERROR: " + JSON.stringify(chrome.runtime.lastError));
		}
		port.onMessage.addListener(function (response) {
			if (response) {
				console.log("RECV " + tabid + ": " + JSON.stringify(response));
				_reply(tabid, response);
			} else {
				console.log("ERROR " + tabid + ": " + JSON.stringify(chrome.runtime.lastError));
				_fail_with(message, "technical_error");
			}
		});

		port.onDisconnect.addListener(function () {
			if (chrome.runtime.lastError) {
				console.log("Port disconnected due to an error: " + chrome.runtime.lastError.message);
			} else {
				console.log("Port disconnected");
			}
			delete ports[tabid];
		});
		ports[tabid] = port;
		ports[tabid].postMessage(message);
	} else {
		// Port already open
		ports[tabid].postMessage(message);
	}
}