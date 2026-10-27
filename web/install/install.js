// Install guide: shows the state of every step from TokenSigningCheck.detect()
(function() {
    var OS_NAMES = {windows: "Windows", macos: "macOS", linux: "Linux"};
    var BROWSER_NAMES = {chrome: "Google Chrome", edge: "Microsoft Edge", firefox: "Firefox", safari: "Safari", opera: "Opera"};

    function step(id, cls, text) {
        var el = document.getElementById(id);
        el.classList.remove("ok", "missing", "update");
        if (cls) {
            el.classList.add(cls);
        }
        el.querySelector(".state").textContent = text;
    }

    function latestVersion() {
        return fetch("version.json", {cache: "no-store"}).then(function(r) {
            return r.ok ? r.json() : {};
        }).then(function(json) {
            return json.version || null;
        }, function() {
            return null;
        });
    }

    function check() {
        ["browser", "extension", "host"].forEach(function(id) {
            step(id, null, "Zjišťuji…");
        });
        document.getElementById("done").style.display = "none";
        document.getElementById("versions").textContent = "";
        latestVersion().then(function(latest) {
            return TokenSigningCheck.detect({latestVersion: latest}).then(function(s) {
                show(s, latest);
            });
        });
    }

    function show(s, latest) {
        var browser = BROWSER_NAMES[s.browser] || "tento prohlížeč";
        if (s.status === "unsupported_os") {
            step("browser", "missing", "Tento systém není podporován, podepisovat lze na Windows, macOS a Linuxu.");
            step("extension", null, "Nejdřív je potřeba krok 1.");
            step("host", null, "Nejdřív je potřeba krok 1.");
            return;
        }
        if (s.status === "unsupported_browser") {
            step("browser", "missing", browser + " není podporován.");
            step("extension", null, "Nejdřív je potřeba krok 1.");
            step("host", null, "Nejdřív je potřeba krok 1.");
            return;
        }
        step("browser", "ok", browser + " na " + OS_NAMES[s.os] + " je v pořádku.");
        document.getElementById("store").href = s.store;
        document.getElementById("how-edge").hidden = s.browser !== "edge";

        var download = document.getElementById("download");
        download.href = s.download;
        download.setAttribute("download", "");
        download.textContent = "Stáhnout program pro " + OS_NAMES[s.os] + (latest ? " (verze " + latest + ")" : "");
        ["windows", "macos", "linux"].forEach(function(os) {
            document.getElementById("how-" + os).hidden = os !== s.os;
        });

        if (s.status === "no_extension") {
            step("extension", "missing", "Rozšíření v prohlížeči chybí.");
            step("host", null, "Ověří se po instalaci rozšíření.");
            return;
        }
        // Without the host the extension cannot report its version
        var installed = "Rozšíření je nainstalované" + (s.extensionVersion ? " (verze " + s.extensionVersion + ")." : ".");
        if (s.updates.indexOf("extension") < 0) {
            step("extension", "ok", installed);
        } else if (s.browser === "edge") {
            // The add-on in the Edge store is not updated, the current version is in the Chrome Web Store
            step("extension", "update", installed + " Nainstalujte novou verzi z Internetového obchodu Chrome a starou odeberte.");
        } else {
            step("extension", "ok", installed + " Novější verzi prohlížeč nainstaluje sám.");
        }
        if (s.status === "no_host") {
            step("host", "missing", "Program v počítači chybí.");
            return;
        }
        if (s.updates.indexOf("host") >= 0) {
            step("host", "update", "Program je nainstalovaný ve verzi " + s.hostVersion
                + ", nainstalujte novější verzi " + latest + ". Podepisovat lze i se stávající.");
        } else {
            step("host", "ok", "Program je nainstalovaný (verze " + s.hostVersion + ").");
            document.getElementById("done").style.display = "block";
        }
        document.getElementById("versions").textContent = "Rozšíření " + s.extensionVersion + " · program " + s.hostVersion;
    }

    document.getElementById("again").onclick = check;
    check();
})();
