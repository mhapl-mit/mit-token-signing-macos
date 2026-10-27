# TODO

## Rozšíření v obchodech

- [ ] **Chrome Web Store:** vydat novou verzi rozšíření 1.1.0 (`signBatch()`).
      Stávající položka: https://chromewebstore.google.com/detail/mit-consulting-token-sign/hkckiflbaedpdigcjoeiacmfnlncpeoj
      Zip `dist/mit-token-signing-extension_1.1.0.zip` (bez pole `key`, obchod drží ID sám).
- [ ] **Microsoft Edge Add-ons:** vydat novou verzi 1.1.0 stávajícího doplňku
      https://microsoftedge.microsoft.com/addons/detail/mit-consulting-token-s/ajndpmmgagjlfgoegeadcgkiehlbcinl
      (Partner Center, https://partner.microsoft.com/dashboard/microsoftedge/), stejný zip
      jako pro Chrome. ID je už v `allowed_origins` manifestů hosta.
- [ ] ERMS přepnout na `signBatch()` až po rozšíření nové verze k uživatelům,
      se zálohou na dosavadní cyklus `sign()` (viz README).

## Vydání 1.2.0 a průvodce instalací

- [x] Instalačky 1.2.0.13: MSI x64/x86 (WiX 3.14), `.deb` (Docker), `.pkg` (GitHub Actions),
      nepodepsané. Průvodce instalací `web/install` + `build/web/package.sh` → balík pro ERMS.
- [ ] Nahradit stránku https://repos.mit-consulting.cz/token-sign/index.html průvodcem
      (obsah `dist/token-signing-install/`); rozšíření tam posílá uživatele bez programu.
      Dnešní `exe/install_mit_token_sign_x64.msi` (1.1.2.0, podepsané vlastním certifikátem
      M.I.T. Consulting s.r.o. platným do 1/2022, Windows mu nevěří) nahradit nebo přesměrovat.
- [ ] ERMS: průvodce v podpisové knize — zadání pro erms-core
      `C:\work\JavaProject\erms-archi\docs\koncepce\zadani\erms-core.md`.
- [ ] Modul externího podpisu ISSD (samostatné repo): text návodu v `i18n_*.properties` odkázat na průvodce.
- [ ] Před uvolněním běžným uživatelům: podpis MSI (Authenticode, např. Azure Trusted Signing)
      a `.pkg` (Apple Developer ID + notarizace) přímo v sestavení.
- [ ] Instalace bez práv správce (registrace hosta pro uživatele: HKCU, `~/Library`, `~/.config`).

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
- [x] macOS: SIGN_BATCH přeložen a otestován na Macu v GitHub Actions s tokenem SoftHSM
      (`host-test/macos-ui-test.sh`, 5 podpisů ověřeno), dialogy ve stejném vzhledu jako
      Windows a Linux. Repozitář `mhapl-mit/mit-token-signing-macos` je jen kopie kódu bez
      `.pem`/`.crx` pro sestavení na Macu (lokální větev `github-snapshot`).
- [ ] macOS: ověřit se skutečnou kartou na Macu kolegy (PIN, QPIN, pinpad, okno s průběhem).
- [x] macOS: skript pro sestavení pro kolegu `build/macos/build.sh` (jen Xcode z App Store).
- [ ] macOS: verze s CryptoTokenKit.
