# TODO

## Rozšíření v obchodech

- [ ] **Chrome Web Store:** vydat novou verzi rozšíření 1.1.0 (`signBatch()`).
      Stávající položka: https://chromewebstore.google.com/detail/mit-consulting-token-sign/hkckiflbaedpdigcjoeiacmfnlncpeoj
      Zip ze složky `extension/` (bez starých `rozsireni_*.zip`), ID zůstává díky `key` v manifestu.
- [ ] **Microsoft Edge Add-ons:** vytvořit a zaregistrovat nový doplněk
      (Partner Center, https://partner.microsoft.com/dashboard/microsoftedge/).
      Edge přidělí doplňku vlastní ID – to je nutné doplnit do `allowed_origins`
      v manifestech nativního hosta (`cz.mit.consulting.json` pro Windows, Linux a macOS)
      a vydat nové balíčky hosta.
- [ ] ERMS přepnout na `signBatch()` až po rozšíření nové verze k uživatelům,
      se zálohou na dosavadní cyklus `sign()` (viz README).

## Nativní host

- Windows: stávající `SIGN` zůstává beze změny (přes úložiště certifikátů a ovladač karty,
  používají ho tisíce uživatelů). Nový `SIGN_BATCH` podepisuje přes PKCS#11 výrobce
  (`BatchSigner`, ProID `proid*cm11.dll`, SafeNet `eTPKCS11.dll`, další v
  `%ProgramData%\MIT Token Signing\pkcs11-modules.conf`). Bez PKCS#11 nebo s PIN pad čtečkou
  podepisuje po jednom jako `SIGN`. Ověřeno s ProID+Q 26. 10. 2026.
  Pozn.: minidriver ProID zobrazuje dialog pro PIN podpisu sám před každým podpisem,
  předání PINu přes `NCRYPT_PIN_PROPERTY` to neobejde – proto PKCS#11.
- PIN digitálního podpisu (QPIN) při dávce na všech platformách: nejdřív se zkusí PIN tokenu
  (jako dialog ovladače na Windows, PIN a QPIN bývají stejné). Když ho karta odmítne, host se
  zeptá na QPIN a zapamatuje si k certifikátu, že je jiný (Windows registr
  `HKCU\Software\MIT Token Signing\SignaturePin`, Linux QSettings, macOS NSUserDefaults);
  na Windows a Linuxu se pak oba PINy zadávají v jednom dialogu, na macOS ve dvou po sobě.
  Odmítnutý pokus stojí jeden pokus QPINu, zkouší se jen jednou na certifikát.
- [ ] Windows: MSI 1.2.0 (WiX Toolset 3), otestovat dávku s eTokenem.
- [ ] macOS: SIGN_BATCH je napsaný (jeden PIN, okno s průběhem), ale zatím nebyl přeložen
      ani otestován – ověřit na Macu.
- [ ] macOS: dialogy (výběr certifikátu, PIN) ve stejném vzhledu jako Linux (`host-linux/DialogStyle.h`)
      a Windows (`host-windows/BatchPinDialog.*`), až bude Mac na ověření: titulek okna jen
      „MIT Token signing“, velký tučný nadpis vlevo a logo MIT (výška 36 bodů) vpravo nahoře,
      jméno a počet dokumentů šedě, chyba červeně tučně, pole přes celou šířku, tlačítka
      vpravo v šedé patičce s dělicí čarou, hlavní tlačítko modré.
- [ ] macOS: skript pro sestavení pro kolegu (bez programování).
- [ ] macOS: verze s CryptoTokenKit.
