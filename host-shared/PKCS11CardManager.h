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

#pragma once

#include "pkcs11.h"
#include "Logger.h"
#include "Exceptions.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#define C(API, ...) Call(__FILE__, __LINE__, "C_"#API, fl->C_##API, __VA_ARGS__)

class PKCS11CardManager {
private:
#ifdef _WIN32
    HINSTANCE library = 0;
#else
    void *library = nullptr;
#endif
    CK_FUNCTION_LIST_PTR fl = nullptr;

    template <typename Func, typename... Args>
    void Call(const char *file, int line, const char *function, Func func, Args... args) const
    {
        CK_RV rv = func(args...);
        Logger::writeLog(function, file, line, "return value %lu", rv);
        switch (rv) {
            case CKR_OK:
                break;
            case CKR_FUNCTION_CANCELED:
                throw UserCancelledException();
            case CKR_PIN_INCORRECT:
                throw AuthenticationError();
            case CKR_PIN_LEN_RANGE:
                throw AuthenticationBadInput();
            case CKR_PIN_LOCKED:
                throw PinBlockedException();
            case CKR_TOKEN_NOT_RECOGNIZED:
                throw PKCS11TokenNotRecognized();
            case CKR_TOKEN_NOT_PRESENT:
                throw PKCS11TokenNotPresent();
            default:
                throw PKCS11Exception("PKCS11 method failed.");
        }
    }

    std::vector<CK_BYTE> attribute(CK_SESSION_HANDLE session, CK_OBJECT_CLASS obj, CK_ATTRIBUTE_TYPE attr) const {
        CK_ATTRIBUTE attribute = { attr, nullptr, 0 };
        C(GetAttributeValue, session, obj, &attribute, 1);
        std::vector<CK_BYTE> data(attribute.ulValueLen);
        attribute.pValue = data.data();
        C(GetAttributeValue, session, obj, &attribute, 1);
        return data;
    }

    std::vector<CK_OBJECT_HANDLE> findObject(CK_SESSION_HANDLE session, CK_OBJECT_CLASS objectClass, const std::vector<CK_BYTE> &id = {}) const {
        if (!fl)
            throw DriverException();
        CK_BBOOL btrue = CK_TRUE;
        std::vector<CK_ATTRIBUTE> searchAttribute{
            {CKA_CLASS, &objectClass, CK_ULONG(sizeof(objectClass))},
            {CKA_TOKEN, &btrue, CK_ULONG(sizeof(btrue))}
        };
        if (!id.empty())
            searchAttribute.push_back({ CKA_ID, CK_VOID_PTR(id.data()), CK_ULONG(id.size()) });
        C(FindObjectsInit, session, searchAttribute.data(), CK_ULONG(searchAttribute.size()));
        CK_ULONG objectCount = 32;
        std::vector<CK_OBJECT_HANDLE> objectHandle(objectCount);
        C(FindObjects, session, objectHandle.data(), CK_ULONG(objectHandle.size()), &objectCount);
        C(FindObjectsFinal, session);
        objectHandle.resize(objectCount);
        return objectHandle;
    }

public:
    PKCS11CardManager(const std::string &module) {
        CK_C_GetFunctionList C_GetFunctionList = nullptr;
        std::string error;
#ifdef _WIN32
        library = LoadLibraryA(module.c_str());
        if (library)
            C_GetFunctionList = CK_C_GetFunctionList(GetProcAddress(library, "C_GetFunctionList"));
		else
		{
			LPSTR msg = nullptr;
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), LPSTR(&msg), 0, nullptr);
			error = msg;
			LocalFree(msg);
		}
#else
        library = dlopen(module.c_str(), RTLD_LOCAL | RTLD_NOW);
        if (library)
            C_GetFunctionList = CK_C_GetFunctionList(dlsym(library, "C_GetFunctionList"));
        else
            error = dlerror();
#endif

        if (!C_GetFunctionList) {
            _log("Function List not loaded %s: %s", module.c_str(), error.c_str());
            throw DriverException();
        }
        Call(__FILE__, __LINE__, "C_GetFunctionList", C_GetFunctionList, &fl);
        _log("initializing module %s", module.c_str());
        C(Initialize, nullptr);
    }

    ~PKCS11CardManager() {
        if (!library)
            return;
        try {
            C(Finalize, nullptr);
        } catch(...) {
            // Do not throw in destructor
        }
#ifdef _WIN32
        FreeLibrary(library);
#else
        dlclose(library);
#endif
    }

    struct Token {
        std::string label;
        CK_SLOT_ID slotID;
        std::vector<CK_BYTE> cert, certID;
        int retry;
        bool pinpad;
        CK_ULONG minPinLen, maxPinLen;
    };

    std::vector<Token> tokens() const {
        if (!fl)
            throw DriverException();
        CK_ULONG slotCount = 0;
        C(GetSlotList, CK_TRUE, nullptr, &slotCount);
        _log("slotCount = %lu", slotCount);
        std::vector<CK_SLOT_ID> slotIDs(slotCount);
        C(GetSlotList, CK_TRUE, slotIDs.data(), &slotCount);

        std::vector<Token> result;
        for (CK_SLOT_ID slotID : slotIDs)
        {
            CK_TOKEN_INFO tokenInfo;
            try {
                C(GetTokenInfo, slotID, &tokenInfo);
            } catch(const BaseException &e) {
                _log("Failed to get slot info at SLOT ID %lu '%s', skiping", slotID, e.what());
                continue;
            }
            // An uninitialized or unknown token in one slot must not hide the tokens in the others
            CK_SESSION_HANDLE session = 0;
            try {
                C(OpenSession, slotID, CKF_SERIAL_SESSION, nullptr, nullptr, &session);
            } catch(const BaseException &e) {
                _log("Failed to open session at SLOT ID %lu '%s', skiping", slotID, e.what());
                continue;
            }

            for (CK_OBJECT_HANDLE obj: findObject(session, CKO_CERTIFICATE)) {
                result.push_back({ std::string((const char*)tokenInfo.label, sizeof(tokenInfo.label)), slotID,
                    attribute(session, obj, CKA_VALUE),
                    attribute(session, obj, CKA_ID),
                    [&tokenInfo] {
                        if (tokenInfo.flags & CKF_USER_PIN_LOCKED) return 0;
                        if (tokenInfo.flags & CKF_USER_PIN_FINAL_TRY) return 1;
                        if (tokenInfo.flags & CKF_USER_PIN_COUNT_LOW) return 2;
                        return 3;
                    }(),
                    (tokenInfo.flags & CKF_PROTECTED_AUTHENTICATION_PATH) > 0,
                    tokenInfo.ulMinPinLen,
                    tokenInfo.ulMaxPinLen,
                });
            }

            C(CloseSession, session);
        }
        return result;
    }

    // Digital signature PIN for keys with context specific login, retry is true when the
    // previous one was rejected. Empty string cancels signing.
    using SignPin = std::function<std::string(bool retry)>;

    std::vector<CK_BYTE> sign(const Token &token, const std::vector<CK_BYTE> &hash, const char *pin,
            const SignPin &signPin = nullptr) const {
        return signBatch(token, {hash}, pin, signPin).at(0);
    }

    // Signs all hashes in one session with one login. signPin is asked once for keys requiring
    // context specific login before each signature, e.g. qualified keys on SafeNet eToken 5110 CC
    // or ProID+Q protected by a digital signature PIN, and once more when the token rejects it.
    // progress is called before each signature with its index and may throw to stop the batch.
    std::vector<std::vector<CK_BYTE>> signBatch(const Token &token, const std::vector<std::vector<CK_BYTE>> &hashes,
            const char *pin, const SignPin &signPin = nullptr,
            const std::function<void(size_t)> &progress = nullptr) const {
        if (!fl)
            throw DriverException();
        if (token.certID.empty())
            throw PKCS11Exception("Could not read private key. Certificate ID is empty");
        CK_SESSION_HANDLE session = 0;
        C(OpenSession, token.slotID, CKF_SERIAL_SESSION, nullptr, nullptr, &session);
        std::string contextPin;
        try {
            C(Login, session, CKU_USER, CK_CHAR_PTR(pin), CK_ULONG(pin ? strlen(pin) : 0));
            std::vector<CK_OBJECT_HANDLE> privateKeyHandle = findObject(session, CKO_PRIVATE_KEY, token.certID);
            if (privateKeyHandle.empty())
                throw PKCS11Exception("Could not read private key. Key not found");
            if (privateKeyHandle.size() > 1)
                throw PKCS11Exception("Could not read private key. Found multiple keys");
            _log("found %lu private keys in slot, using key ID %x", privateKeyHandle.size(), token.certID.data());

            CK_KEY_TYPE keyType = CKK_RSA;
            CK_ATTRIBUTE attribute = { CKA_KEY_TYPE, &keyType, sizeof(keyType) };
            C(GetAttributeValue, session, privateKeyHandle[0], &attribute, 1);
            CK_BBOOL alwaysAuthenticate = CK_FALSE;
            CK_ATTRIBUTE alwaysAuthenticateAttribute = { CKA_ALWAYS_AUTHENTICATE, &alwaysAuthenticate, sizeof(alwaysAuthenticate) };
            if (fl->C_GetAttributeValue(session, privateKeyHandle[0], &alwaysAuthenticateAttribute, 1) != CKR_OK)
                alwaysAuthenticate = CK_FALSE;
            if (alwaysAuthenticate)
                _log("key requires context specific login");

            CK_MECHANISM mechanism = {keyType == CKK_ECDSA ? CKM_ECDSA : CKM_RSA_PKCS, 0, 0};
            std::vector<std::vector<CK_BYTE>> signatures;
            for (size_t i = 0; i < hashes.size(); ++i) {
                if (progress)
                    progress(i);
                C(SignInit, session, &mechanism, privateKeyHandle[0]);
                if (alwaysAuthenticate) {
                    if (token.pinpad) {
                        C(Login, session, CKU_CONTEXT_SPECIFIC, nullptr, 0);
                    } else {
                        for (bool retry = false;; retry = true) {
                            if (contextPin.empty() && signPin)
                                contextPin = signPin(retry);
                            if (contextPin.empty())
                                throw UserCancelledException();
                            try {
                                C(Login, session, CKU_CONTEXT_SPECIFIC, CK_CHAR_PTR(contextPin.c_str()), CK_ULONG(contextPin.size()));
                                break;
                            } catch (const AuthenticationError &) {
                                std::fill(contextPin.begin(), contextPin.end(), '\0');
                                contextPin.clear();
                                if (retry || !signPin)
                                    throw;
                                _log("digital signature PIN rejected, asking again");
                                // Failed login may end the signing operation
                                CK_RV rv = fl->C_SignInit(session, &mechanism, privateKeyHandle[0]);
                                if (rv != CKR_OK && rv != CKR_OPERATION_ACTIVE)
                                    C(SignInit, session, &mechanism, privateKeyHandle[0]);
                            }
                        }
                    }
                }
                std::vector<CK_BYTE> data = keyType == CKK_RSA ? digestInfo(hashes[i].size()) : std::vector<CK_BYTE>();
                data.insert(data.end(), hashes[i].cbegin(), hashes[i].cend());
                CK_ULONG signatureLength = 0;
                C(Sign, session, data.data(), CK_ULONG(data.size()), nullptr, &signatureLength);
                std::vector<CK_BYTE> signature(signatureLength);
                C(Sign, session, data.data(), CK_ULONG(data.size()), signature.data(), &signatureLength);
                signature.resize(signatureLength);
                signatures.push_back(std::move(signature));
            }
            std::fill(contextPin.begin(), contextPin.end(), '\0');
            C(Logout, session);
            C(CloseSession, session);
            return signatures;
        } catch (...) {
            std::fill(contextPin.begin(), contextPin.end(), '\0');
            fl->C_CloseSession(session);
            throw;
        }
    }

private:
    // DER DigestInfo prefix for RSA PKCS#1 v1.5 signature of a hash with given length
    static std::vector<CK_BYTE> digestInfo(size_t hashLength) {
        switch (hashLength) {
            case 20: //BINARY_SHA1_LENGTH:
                return {0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14};
            case 28: //BINARY_SHA224_LENGTH:
                return {0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04, 0x05, 0x00, 0x04, 0x1c};
            case 32: //BINARY_SHA256_LENGTH:
                return {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
            case 48: //BINARY_SHA384_LENGTH:
                return {0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30};
            case 64: //BINARY_SHA512_LENGTH:
                return {0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40};
            default:
                _log("incorrect digest length");
                throw InvalidHashException();
        }
    }
};
