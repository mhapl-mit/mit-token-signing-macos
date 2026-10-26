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

#include "BatchSigner.h"
#include "BinaryUtils.h"
#include "Exceptions.h"
#include "Labels.h"
#include "Logger.h"
#include "BatchPinDialog.h"
#include "PKCS11CardManager.h"
#include "PKCS11Path.h"
#include "Signer.h"

#include <Windows.h>
#include <WinCrypt.h>
#include <ShlObj.h>

#include <memory>

using namespace std;

static const wstring APP_TITLE = L"MIT Token signing";

static wstring replace(wstring text, const wstring &key, const wstring &value)
{
	size_t pos = text.find(key);
	if (pos != wstring::npos)
		text.replace(pos, key.size(), value);
	return text;
}

// What is known about the digital signature PIN of the certificate key: it is the same as the token PIN,
// or it differs and the next batch asks for both PINs in one dialog
static const wchar_t *SIGNATURE_PIN_KEY = L"Software\\MIT Token Signing\\SignaturePin";
enum SignaturePinMemory { SIGNATURE_PIN_UNKNOWN = 0, SIGNATURE_PIN_SEPARATE = 1, SIGNATURE_PIN_SAME = 2 };

static wstring fingerprint(const vector<unsigned char> &cert)
{
	BYTE hash[20];
	DWORD size = sizeof(hash);
	if (!CryptHashCertificate(0, CALG_SHA1, 0, cert.data(), DWORD(cert.size()), hash, &size))
		return wstring();
	string hex = BinaryUtils::bin2hex(vector<unsigned char>(hash, hash + size));
	return wstring(hex.cbegin(), hex.cend());
}

static DWORD rememberedSignaturePin(const wstring &id)
{
	DWORD value = SIGNATURE_PIN_UNKNOWN, size = sizeof(value);
	if (id.empty() || RegGetValueW(HKEY_CURRENT_USER, SIGNATURE_PIN_KEY, id.c_str(), RRF_RT_REG_DWORD,
			nullptr, &value, &size) != ERROR_SUCCESS)
		return SIGNATURE_PIN_UNKNOWN;
	return value;
}

static void rememberSignaturePin(const wstring &id, DWORD value)
{
	if (!id.empty())
		RegSetKeyValueW(HKEY_CURRENT_USER, SIGNATURE_PIN_KEY, id.c_str(), REG_DWORD, &value, sizeof(value));
}

static void wipe(string &pin)
{
	if (!pin.empty())
		SecureZeroMemory(&pin[0], pin.size());
	pin.clear();
}

// Information from the web page and the number of documents, false when the user cancelled.
// Shown only when the page sends it, like SIGN, the PIN dialog shows the number of documents.
static bool showInfo(const string &info, size_t count)
{
	if (info.empty())
		return true;
	int size = MultiByteToWideChar(CP_UTF8, 0, info.c_str(), int(info.size()), nullptr, 0);
	if (size > 500)
		throw TechnicalException("Information message to long");
	wstring message(size, 0);
	if (size > 0 && MultiByteToWideChar(CP_UTF8, 0, info.c_str(), int(info.size()), &message[0], size) != size)
		throw TechnicalException("Failed to convert string to wide chars");
	wstring countText = replace(Labels::l10n.get("batch count"), L"@COUNT@", to_wstring(count));
	message = message.empty() ? countText : message + L"\n\n" + countText;
	return MessageBox(nullptr, message.c_str(), APP_TITLE.c_str(), MB_OKCANCEL | MB_ICONINFORMATION | MB_SYSTEMMODAL) == IDOK;
}

// Progress window of the shell runs in its own thread, so it stays responsive while
// the token signs in this thread. It is shown after the first signature, which may
// still ask for the digital signature PIN.
class Progress {
public:
	explicit Progress(size_t total) : total(total) {}
	~Progress()
	{
		if (dialog) {
			dialog->StopProgressDialog();
			dialog->Release();
		}
		if (com)
			CoUninitialize();
	}

	void step(size_t i)
	{
		if (i == 0)
			return;
		if (!dialog)
			start();
		if (!dialog)
			return;
		if (dialog->HasUserCancelled())
			throw UserCancelledException("Signing was cancelled");
		dialog->SetProgress64(i, total);
		dialog->SetLine(2, (to_wstring(i + 1) + L" / " + to_wstring(total)).c_str(), FALSE, nullptr);
	}

private:
	void start()
	{
		com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
		if (FAILED(CoCreateInstance(CLSID_ProgressDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
			dialog = nullptr;
			return;
		}
		dialog->SetTitle(APP_TITLE.c_str());
		dialog->SetLine(1, Labels::l10n.get("signing").c_str(), FALSE, nullptr);
		dialog->StartProgressDialog(nullptr, nullptr, PROGDLG_NORMAL | PROGDLG_AUTOTIME | PROGDLG_NOMINIMIZE, nullptr);
		dialog->SetProgress64(0, total);
	}

	size_t total;
	IProgressDialog *dialog = nullptr;
	bool com = false;
};

vector<vector<unsigned char>> BatchSigner::sign(const vector<unsigned char> &cert,
	const vector<vector<unsigned char>> &digests, const string &info)
{
	if (!showInfo(info, digests.size()))
		throw UserCancelledException();

	// The certificate was selected from the Windows certificate store, find its token
	unique_ptr<PKCS11CardManager> pkcs11;
	PKCS11CardManager::Token selected;
	for (const string &module : PKCS11Path::probedModules()) {
		if (GetFileAttributesA(module.c_str()) == INVALID_FILE_ATTRIBUTES)
			continue;
		try {
			unique_ptr<PKCS11CardManager> manager(new PKCS11CardManager(module));
			for (const PKCS11CardManager::Token &token : manager->tokens()) {
				if (token.cert == cert) {
					selected = token;
					break;
				}
			}
			if (!selected.cert.empty()) {
				_log("batch signing with PKCS#11 module %s", module.c_str());
				pkcs11 = move(manager);
				break;
			}
		}
		catch (const BaseException &e) {
			_log("PKCS#11 module %s: %s", module.c_str(), e.what());
		}
	}

	// PIN pad readers and tokens without a PKCS#11 module are signed as before, one by one
	if (!pkcs11 || selected.pinpad) {
		_log("batch signing one by one through Windows certificate store");
		pkcs11.reset();
		unique_ptr<Signer> signer(Signer::createSigner(cert));
		vector<vector<unsigned char>> signatures;
		for (const vector<unsigned char> &digest : digests)
			signatures.push_back(signer->sign(digest));
		return signatures;
	}

	if (selected.retry <= 0)
		throw PinBlockedException();

	wstring label = Labels::l10n.get("sign PIN");
	if (PCCERT_CONTEXT certificate = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, cert.data(), DWORD(cert.size()))) {
		BYTE keyUsage = 0;
		CertGetIntendedKeyUsage(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, certificate->pCertInfo, &keyUsage, 1);
		if (!(keyUsage & CERT_NON_REPUDIATION_KEY_USAGE))
			label = Labels::l10n.get("auth PIN");
		CertFreeCertificateContext(certificate);
	}
	label = replace(label, L"@PIN@", L"PIN");
	// Certificate holder shown in the PIN dialog
	wstring name;
	if (PCCERT_CONTEXT certificate = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, cert.data(), DWORD(cert.size()))) {
		name.resize(CertGetNameStringW(certificate, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0));
		if (!name.empty())
			name.resize(CertGetNameStringW(certificate, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, &name[0], DWORD(name.size())) - 1);
		CertFreeCertificateContext(certificate);
	}
	const wstring count = replace(Labels::l10n.get("batch count"), L"@COUNT@", to_wstring(digests.size()));

	// Keys with a digital signature PIN (e.g. ProID+Q, eToken 5110 CC) need it once per batch.
	// Like the dialog of the Windows driver, the token PIN is tried first, it is often the same.
	// Only when the token rejects it, the digital signature PIN is asked and the next batch asks
	// for both PINs in one dialog. The token PIN is tried once per certificate, a rejected attempt
	// counts against the digital signature PIN tries.
	const wstring id = fingerprint(cert);
	const DWORD memory = rememberedSignaturePin(id);
	const bool askBoth = memory == SIGNATURE_PIN_SEPARATE;
	string signaturePin;
	const char *tokenPin = nullptr;
	bool signaturePinUsed = false, signaturePinIsTokenPin = false;
	auto signPin = [&](bool retry) -> string {
		signaturePinUsed = true;
		signaturePinIsTokenPin = false;
		if (!retry && !signaturePin.empty())
			return signaturePin;
		if (!retry && memory != SIGNATURE_PIN_SEPARATE && tokenPin) {
			_log("using the token PIN as digital signature PIN, known: %u", unsigned(memory));
			signaturePinIsTokenPin = true;
			return tokenPin;
		}
		BatchPinRequest request;
		request.title = APP_TITLE;
		request.heading = Labels::l10n.get("signature PIN title");
		request.name = name;
		request.details = Labels::l10n.get("signature PIN info");
		if (retry)
			request.error = Labels::l10n.get("incorrect PIN2");
		request.label = Labels::l10n.get("signature PIN");
		request.minPinLen = selected.minPinLen;
		return BatchPinDialog::show(request);
	};

	bool isInitialCheck = true;
	for (int retriesLeft = selected.retry; retriesLeft > 0;) {
		BatchPinRequest request;
		request.title = APP_TITLE;
		request.heading = Labels::l10n.get("batch title");
		request.name = name;
		request.details = count;
		if (retriesLeft < 3) {
			request.error = (isInitialCheck ? wstring() : Labels::l10n.get("incorrect PIN2")) +
				Labels::l10n.get("tries left") + L" " + to_wstring(retriesLeft);
		}
		isInitialCheck = false;
		request.label = label;
		if (askBoth)
			request.label2 = Labels::l10n.get("signature PIN");
		request.minPinLen = selected.minPinLen;

		string pin = BatchPinDialog::show(request, askBoth ? &signaturePin : nullptr);
		if (pin.empty())
			throw UserCancelledException();
		signaturePinUsed = false;
		tokenPin = pin.c_str();
		Progress progress(digests.size());
		try {
			vector<vector<unsigned char>> signatures = pkcs11->signBatch(selected, digests, pin.c_str(), signPin,
				[&progress](size_t i) { progress.step(i); });
			wipe(pin);
			wipe(signaturePin);
			if (signaturePinUsed) {
				DWORD learned = signaturePinIsTokenPin ? SIGNATURE_PIN_SAME : SIGNATURE_PIN_SEPARATE;
				if (learned != memory)
					rememberSignaturePin(id, learned);
			}
			return signatures;
		}
		catch (const AuthenticationError &) {
			_log("Wrong PIN");
			--retriesLeft;
		}
		catch (const PinBlockedException &) {
			retriesLeft = 0;
		}
		catch (const AuthenticationBadInput &) {
			_log("Bad PIN input");
		}
		catch (...) {
			wipe(pin);
			wipe(signaturePin);
			throw;
		}
		wipe(pin);
		wipe(signaturePin);
	}
	MessageBox(nullptr, Labels::l10n.get("PIN2 blocked").c_str(), APP_TITLE.c_str(), MB_OK | MB_ICONERROR);
	throw PinBlockedException();
}
