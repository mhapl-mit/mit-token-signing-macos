# Podporované tokeny a ovladače

Přehled kvalifikovaných prostředků (QSCD) českých poskytovatelů a PKCS#11 knihoven,
které native host na macOS a Linuxu hledá (`host-shared/PKCS11Path.cpp`). Na Windows
host používá úložiště certifikátů Windows, stačí nainstalovat ovladač výrobce.

Stav k 26. 10. 2026. Host použije první modul ze seznamu, který vidí token s certifikátem.
Další moduly lze doplnit do `pkcs11-modules.conf` (viz README).

## Tokeny a ovladače

| Token / karta | Vydavatel | Ovladač | macOS knihovna | Linux knihovna |
|---|---|---|---|---|
| eObčanka (od 1. 7. 2018) | MV ČR, certifikát I.CA | eObčanka (Monet+) | `/usr/local/lib/eOPCZE/libeopproxyp11.dylib` | `/usr/lib/x86_64-linux-gnu/libeopproxyp11.so` |
| ProID+Q (karta i token) | I.CA (iSignum), PostSignum | ProID+ (Monet+) | `/usr/local/lib/ProIDPlus/libproidproxyp11.dylib` | `/usr/lib/x86_64-linux-gnu/libproidproxyp11.so` (deb), `/usr/lib64/libproidqcm11.so` (rpm) |
| Starcos 3.5 / 3.7 | I.CA | I.CA SecureStore 8.3 | `/usr/local/lib/pkcs11/libICASecureStorePkcs11.dylib` | **není** (I.CA ovladač pro Linux nevydává) |
| SafeNet eToken 5110 CC (940/940B/940C), eToken Fusion CC | PostSignum, eIdentity, I.CA | SafeNet Authentication Client 10.9 | `/usr/local/lib/libeTPkcs11.dylib` | `/usr/lib/libeTPkcs11.so` (deb), `/usr/lib64/libeTPkcs11.so` (rpm) |
| Thales IDPrime 940 / 3940 (karta) | PostSignum, eIdentity, Komerční banka | SafeNet Authentication Client 10.9 | `/usr/local/lib/libIDPrimePKCS11.dylib` | `/usr/lib/libIDPrimePKCS11.so` |
| Bit4id TokenME EVO | PostSignum | Bit4id PKI Manager 1.4.11 | `/Library/bit4id/pkcs11/libbit4xpki.dylib` | **není** (PostSignum ovladač pro Linux nenabízí) |

Ověřeno v provozu (Linux, Ubuntu 24.04): ProID+Q s certifikátem I.CA iSignum a
SafeNet eToken 5110 CC s kvalifikovanou pečetí I.CA iSignum.

Už nejsou kvalifikované: eToken 5110 CC / IDPrime MD **840** (certifikace skončila 5. 4. 2026),
starší TokenME EVO (do 17. 4. 2024), TokenME (Oberthur) a Crypto Java Card (do 31. 12. 2022).

## Podpisový PIN (QPIN)

Kvalifikovaný klíč na eTokenu 5110 CC, IDPrime 940 a ProID+Q chrání samostatný PIN
digitálního podpisu. Host se po PINu (heslu) tokenu zeptá i na PIN digitálního podpisu,
pokud ho klíč vyžaduje (PKCS#11 `CKA_ALWAYS_AUTHENTICATE`). Heslo k tokenu SafeNet může
obsahovat písmena.

## Kde stáhnout ovladače

- eObčanka: https://info.identita.gov.cz (macOS, Ubuntu)
- ProID+: https://www.proid.cz/ke-stazeni (macOS, Ubuntu deb, rpm)
- I.CA SecureStore: https://www.ica.cz/secure-store (Windows, macOS)
- SafeNet Authentication Client: od vydavatele certifikátu (PostSignum požaduje sériové číslo
  tokenu), veřejně např. https://support.globalsign.com/digital-certificates/manage-safeNet-eToken/safenet-drivers
- Bit4id: odkaz na stránce tokenu na https://www.postsignum.cz

## Poznámky k macOS

- SafeNet Authentication Client na macOS 14 a 15 může u některých tokenů potřebovat zvlášť
  Thales CCID driver 1.5.2 (Thales KB0027738).
- Podporu Apple Silicon u SAC 10.9 výrobce v release notes neuvádí, ověřte na Macu:
  `lipo -info /usr/local/lib/libeTPkcs11.dylib`. I.CA SecureStore 8.3.1 je universal (Intel i Apple Silicon).
