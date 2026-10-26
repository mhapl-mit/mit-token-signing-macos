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

let inuse = false;

// Event listener for messages from the page to the extension
window.addEventListener("message", function (event) {
    // Accept only messages from the current window
    if (event.source !== window) return;

    if (event.data.src && event.data.src === "page.js") {
        event.data["origin"] = location.origin;

        // Forward the message to the background service worker
        chrome.runtime.sendMessage(event.data, function (response) {
            if (chrome.runtime.lastError) {
                console.error("Error sending message to background:", chrome.runtime.lastError.message);
            }
        });

        // Add "beforeunload" handler if not already added
        if (!inuse) {
            window.addEventListener("beforeunload", function () {
                chrome.runtime.sendMessage({ src: "page.js", type: "DONE" });
            });
            inuse = true;
        }
    }
    return true;
}, false);

// Forward messages from the extension to the page
chrome.runtime.onMessage.addListener(function (request, sender, sendResponse) {
    window.postMessage(request, "*");
});

// Dynamically inject external script
if (!document.querySelector("script[data-name='TokenSigning']")) {
    const script = document.createElement("script");
    script.type = "text/javascript";
    script.dataset.name = "TokenSigning";
    script.src = chrome.runtime.getURL("page.js"); // Host `page.js` as an external file
    (document.head || document.documentElement).appendChild(script);
}