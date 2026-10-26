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

#include "Labels.h"

#include <map>
#include <vector>

#ifdef _WIN32
#define _TOTEXT(X) L##X
#define T(X) _TOTEXT(X)
#else
#define T(X) X
#endif

Labels Labels::l10n {};

Labels::Labels() {
    setLanguage("cz");
}

void Labels::setLanguage(const std::string &language) {
    static const std::map<std::string, int> languages = {
        { "et", 0 }, { "en", 1 }, { "ru", 2 }, { "lt", 3 }, { "lv", 4 }, { "tr", 5 }, { "cs", 6 }, { "cz", 6 },
        { "est", 0 }, { "eng", 1 }, { "rus", 2 }, { "lit", 3 }, { "lat", 4 }, { "tur", 5 }, { "ces", 6 }, { "cze", 6 }
    };
    auto pos = languages.find(language);
    // Unknown language falls back to Czech
    selectedLanguage = pos == languages.cend() ? 6 : pos->second;
}

Labels::lstring Labels::get(const std::string &labelKey) const {
    static const std::map<std::string,std::vector<lstring> > labels = {
    { "language", {
        T("ET"),
        T("EN"),
        T("RU"),
        T("LT"),
        T("LV"),
        T("TR"),
        T("CZ")
    } },
    { "auth PIN", {
        T("Autentimiseks sisesta @PIN@:"),
        T("For authentication enter @PIN@:"),
        T("Для идентификации введите @PIN@-код:"),
        T("Norėdami patvirtinti tapatybę, įveskite @PIN@:"),
        T("Lai autentificētos, ievadi @PIN@:"),
        T("Kimlik doğrulaması için @PIN@ girin:"),
        T("Pro ověření zadejte @PIN@:"),
    } },
    { "auth PIN pinpad", {
        T("Autentimiseks sisesta @PIN@ kaardilugeja sõrmistikult"),
        T("For authentication enter @PIN@ from PIN pad"),
        T("Для идентификации введите @PIN@-код при помощи клавиатуры"),
        T("Norėdami patvirtinti tapatybę, įveskite @PIN@, pasinaudodami klaviatūra"),
        T("Lai autentificētos, ievadi @PIN@ no PIN ievades ierīces"),
        T("Kimlik doğrulaması için PIN pad'den @PIN@ girin"),
        T("Pro ověření zadejte @PIN@ na klávesnici čtečky"),
    } },
    { "sign PIN", {
        T("Allkirjastamiseks sisesta @PIN@:"),
        T("For signing enter @PIN@:"),
        T("Для подписания введите @PIN@:"),
        T("Norėdami pasirašyti, įveskite @PIN@ kodą:"),
        T("Lai parakstītu, ievadi @PIN@:"),
        T("İmzalamak için @PIN@ girin:"),
        T("Pro podpis zadejte @PIN@:"),
    } },
    { "batch count", {
        T("Allkirjastad @COUNT@ dokumenti."),
        T("You are signing @COUNT@ documents."),
        T("Вы подписываете @COUNT@ документов."),
        T("Pasirašote @COUNT@ dokumentus."),
        T("Jūs parakstāt @COUNT@ dokumentus."),
        T("@COUNT@ belge imzalıyorsunuz."),
        T("Počet podepisovaných dokumentů: @COUNT@"),
    } },
    { "batch title", {
        T("Dokumentide allkirjastamine"),
        T("Sign documents"),
        T("Подписание документов"),
        T("Dokumentų pasirašymas"),
        T("Dokumentu parakstīšana"),
        T("Belgeleri imzala"),
        T("Podepsat dokumenty"),
    } },
    { "sign title", {
        T("Dokumendi allkirjastamine"),
        T("Sign document"),
        T("Подписание документа"),
        T("Dokumento pasirašymas"),
        T("Dokumenta parakstīšana"),
        T("Belgeyi imzala"),
        T("Podepsat dokument"),
    } },
    { "signing", {
        T("Allkirjastamine..."),
        T("Signing..."),
        T("Подписание..."),
        T("Pasirašoma..."),
        T("Parakstīšana..."),
        T("İmzalanıyor..."),
        T("Podepisování..."),
    } },
    { "signature PIN", {
        T("Allkirjastamiseks sisesta allkirja PIN:"),
        T("For signing enter the digital signature PIN:"),
        T("Для подписи введите PIN-код подписи:"),
        T("Norėdami pasirašyti, įveskite parašo PIN:"),
        T("Lai parakstītu, ievadi paraksta PIN:"),
        T("İmzalamak için imza PIN'ini girin:"),
        T("Pro podpis zadejte PIN digitálního podpisu:"),
    } },
    { "signature PIN info", {
        T("Samm 2/2: kvalifitseeritud sertifikaat on kaitstud eraldi allkirja PIN-iga. See ei ole sama PIN, mille just sisestasid."),
        T("Step 2 of 2: the qualified certificate is protected by a separate digital signature PIN. It is not the PIN you have just entered."),
        T("Шаг 2 из 2: квалифицированный сертификат защищён отдельным PIN-кодом подписи. Это не тот PIN, который вы только что ввели."),
        T("2 žingsnis iš 2: kvalifikuotas sertifikatas apsaugotas atskiru parašo PIN. Tai ne tas PIN, kurį ką tik įvedėte."),
        T("2. solis no 2: kvalificētais sertifikāts ir aizsargāts ar atsevišķu paraksta PIN. Tas nav PIN, ko tikko ievadījāt."),
        T("Adım 2/2: nitelikli sertifika ayrı bir imza PIN'i ile korunmaktadır. Az önce girdiğiniz PIN değildir."),
        T("Krok 2 ze 2: kvalifikovaný certifikát je chráněn samostatným PINem digitálního podpisu (QPIN). Nejde o PIN, který jste právě zadali."),
    } },
    { "PIN title", {
        T("PIN"),
        T("PIN"),
        T("PIN"),
        T("PIN"),
        T("PIN"),
        T("PIN"),
        T("PIN tokenu"),
    } },
    { "signature PIN title", {
        T("Allkirja PIN"),
        T("Digital signature PIN"),
        T("PIN-код подписи"),
        T("Parašo PIN"),
        T("Paraksta PIN"),
        T("İmza PIN'i"),
        T("PIN digitálního podpisu"),
    } },
    { "sign PIN pinpad", {
        T("Allkirjastamiseks sisesta @PIN@ kaardilugeja sõrmistikult"),
        T("For signing enter @PIN@ from PIN pad"),
        T("Для подписания введите @PIN@ с PIN-клавиатуры"),
        T("Norėdami pasirašyti, įveskite PIN kodą @PIN@ klaviatūros pagalba"),
        T("Lai parakstītu, ievadi @PIN@ paredzētajā PIN ievades ierīcē"),
        T("İmzalamak için PIN pad'den @PIN@ girin"),
        T("Pro podpis zadejte @PIN@ na klávesnici čtečky"),
    } },
    { "tries left", {
        T("Katseid jäänud:"),
        T("Tries left:"),
        T("Возможных попыток:"),
        T("Liko bandymų:"),
        T("Atlikuši mēģinājumi:"),
        T("Kalan deneme sayısı:"),
        T("Zbývá pokusů:"),
    } },
    { "incorrect PIN2", {
        T("Vale PIN! "),
        T("Incorrect PIN! "),
        T("Неправильный PIN! "),
        T("Neteisingas PIN! "),
        T("Nepareizs PIN! "),
        T("Yanlış PIN! "),
        T("Nesprávný PIN! "),
    } },
    { "PIN2 blocked", {
        T("PIN2 blokeeritud, ei saa allkirjastada!"),
        T("PIN2 blocked, cannot sign!"),
        T("PIN2 блокирован, невозможно подписать!"),
        T("PIN2 užblokuotas, pasirašymas negalimas!"),
        T("PIN2 bloķēts, nav iespējams parakstīt!"),
        T("PIN2 engellendi, imzalanamıyor!"),
        T("PIN2 je zablokován, nelze podepisovat!"),
    } },
    { "error", {
        T("Viga"),
        T("Error"),
        T("Ошибка"),
        T("Klaida"),
        T("Kļūda"),
        T("Hata"),
        T("Chyba"),
    } },
    { "cancel", {
        T("Katkesta"),
        T("Cancel"),
        T("Отменить"),
        T("Atšaukti"),
        T("Atcelt"),
        T("İptal"),
        T("Zrušit"),
    } },
    { "select certificate", {
        T("Sertifikaadi valik"),
        T("Select certificate"),
        T("Выбор сертификата"),
        T("Pasirinkite sertifikatą"),
        T("Izvēlēties sertifikātu"),
        T("Sertifika seç"),
        T("Vyberte certifikát"),
    } },
    { "select", {
        T("Vali"),
        T("Select"),
        T("Выбрать"),
        T("Pasirinkti"),
        T("Izvēlēties"),
        T("Seç"),
        T("Vybrat"),
    } },
    { "certificate", {
        T("Sertifikaat"),
        T("Certificate"),
        T("Сертификат"),
        T("Sertifikatas"),
        T("Sertifikāts"),
        T("Sertifika"),
        T("Certifikát"),
    } },
    { "type", {
        T("Tüüp"),
        T("Type"),
        T("Тип"),
        T("Tipas"),
        T("Tips"),
        T("Tip"),
        T("Typ"),
    } },
    { "valid to", {
        T("Kehtiv kuni"),
        T("Valid to"),
        T("Действительный до"),
        T("Galioja iki"),
        T("Derīgs līdz"),
        T("Geçerlilik"),
        T("Platný do"),
    } },
    { "cert info", {
        T("Sertifikaadi valikuga nõustun oma nime ja isikukoodi edastamisega teenusepakkujale."),
        T("By selecting a certificate I accept that my name and personal ID code will be sent to service provider."),
        T("Выбирая сертификат, я соглашаюсь с тем, что мое имя и личный код будут переданы представителю услуг."),
        T("Pasirinkdama(s) sertifikatą, aš sutinku, kad mano vardas, pavardė ir asmens kodas būtų perduoti e. paslaugos teikėjui."),
        T("Izvēloties sertifikātu, es apstiprinu, ka mans vārds un personas kods tiks nosūtīts pakalpojuma sniedzējam."),
        T("Bir sertifika seçerek ismimin ve kimlik numaramın servis sağlayıcıya gönderileceğini kabul ediyorum."),
        T("Výběrem certifikátu souhlasím s předáním svého jména a identifikačního údaje poskytovateli služby."),
    } }
    };
    return labels.at(labelKey).at(selectedLanguage);
}
