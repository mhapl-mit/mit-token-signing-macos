# TODO

## Rozšíření v obchodech

- [x] **Chrome Web Store:** vydat novou verzi rozšíření 1.1.0 (`signBatch()`), zveřejněno v listopadu 2026.
      Stávající položka: https://chromewebstore.google.com/detail/mit-consulting-token-sign/hkckiflbaedpdigcjoeiacmfnlncpeoj
      Zip `dist/mit-token-signing-extension_1.1.0.zip` (bez pole `key`, obchod drží ID sám).
- [ ] **Microsoft Edge Add-ons:** vydat novou verzi 1.1.0 stávajícího doplňku
      https://microsoftedge.microsoft.com/addons/detail/mit-consulting-token-s/ajndpmmgagjlfgoegeadcgkiehlbcinl
      (Partner Center, https://partner.microsoft.com/dashboard/microsoftedge/), stejný zip
      jako pro Chrome. ID je už v `allowed_origins` manifestů hosta.
      Čeká na převod doplňku na hlavní účet firmy, stav a účty v `docs/ACCOUNTS.md`.
- [ ] ERMS přepnout na `signBatch()` až po rozšíření nové verze k uživatelům,
      se zálohou na dosavadní cyklus `sign()` (viz README).

## Vydání 1.2.0 a průvodce instalací

- [x] Instalačky 1.2.0.13: MSI x64/x86 (WiX 3.14), `.deb` (Docker), `.pkg` (GitHub Actions),
      nepodepsané. Průvodce instalací `web/install` + `build/web/package.sh` → balík pro ERMS.
- [x] Stránka https://repos.mit-consulting.cz/token-sign/ nahrazena průvodcem (14. 11. 2026),
      včetně testu podpisu `test.html`; teď verze 1.2.0.26 (17. 11. 2026, macOS podepsaný
      a notarizovaný), `exe/install_mit_token_sign_*.msi` jsou MSI 1.2.0.26 a jsou přímo
      v `dist/token-signing-install/exe/` (ověřeno proti sestaveným souborům).
- [x] 1.2.0.20: bez karty ve čtečce hlásí macOS a Linux `no_certificates` místo `driver_error`;
      na Macu ověřeno bez karty a s eTokenem 5110 (SafeNet Authentication Client).
- [ ] ERMS: průvodce v podpisové knize — zadání pro erms-core
      `C:\work\JavaProject\erms-archi\docs\koncepce\zadani\erms-core.md`.
- [ ] Modul externího podpisu ISSD (samostatné repo): text návodu v `i18n_*.properties` odkázat na průvodce.
- [x] `.pkg` podepsaný Apple Developer ID a notarizovaný přímo v Codemagic (od 1.2.0.25,
      17. 11. 2026); na Macu nainstalováno bez „Přesto otevřít“, test podpisu prošel.
- [x] 1.2.0.26: host na macOS a Linuxu načte ovladač karty jen jednou na krok (ProID+Q 8 s místo
      16 s), test podpisu bez okna s hláškou `info`.
- [x] 1.2.0.27: MSI bez akce pro Firefox z open-eid, stejně jako zákaznická 1.1.2.0 (5. 3. 2021).
      Zůstala v `.wxs` v gitu a upgrade z 1.2.0.x bez práv správce kvůli ní padal s 1603.
- [ ] Před uvolněním běžným uživatelům: podpis MSI (Authenticode, např. Azure Trusted Signing)
      přímo v sestavení.
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
- [x] macOS: ověřeno se skutečnou kartou ProID+Q (MacBook Pro 2017 Intel, macOS 13) ve verzi 1.2.0.19.
      1.2.0.15 a starší ovladače podepsané výrobcem nenačtou (chyběly entitlements, `driver_error`).
- [x] macOS: skript pro sestavení pro kolegu `build/macos/build.sh` (jen Xcode z App Store).
- [ ] macOS: verze s CryptoTokenKit.
