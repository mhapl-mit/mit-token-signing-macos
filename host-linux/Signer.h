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

#include "PKCS11CardManager.h"
#include "Labels.h"
#include "PKCS11Path.h"
#include "DialogStyle.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QSslCertificate>
#include <QSslCertificateExtension>
#include <QTimeLine>
#include <QVBoxLayout>

#include <atomic>
#include <chrono>
#include <future>
#include <string>

class Signer: public QDialog {
    enum {
        UserCancel = 0,
        TechnicalError = -1,
        AuthError = -2,
    };
    // What is known about the digital signature PIN of the certificate key
    enum {
        SignaturePinUnknown = 0,
        SignaturePinSeparate = 1,
        SignaturePinSame = 2,
    };
public:
    // Signs one or more hashes with one PIN entry. Returns "signature" for a single hash
    // (SIGN) or "signatures" in the same order as hashes for a batch (SIGN_BATCH).
    static QVariantMap sign(const QStringList &hashes, const QString &cert, const QString &info, bool batch) {
        std::vector<unsigned char> data = fromHex(cert);
        std::vector<std::vector<unsigned char>> digests;
        for (const QString &hash: hashes)
            digests.push_back(fromHex(hash));
        PKCS11CardManager::Token selected;
        PKCS11Path::Params p11 = PKCS11Path::getPkcs11ModulePath();
        PKCS11CardManager pkcs11(p11.path);
        try {
            for (const PKCS11CardManager::Token &token : pkcs11.tokens()) {
                if (token.cert == data) {
                    selected = token;
                    break;
                }
            }
        } catch (const BaseException &e) {
            qDebug() << e.what();
            return {{"result", QString::fromStdString(e.getErrorCode())}};
        }

        if(selected.cert.empty())
            return {{"result", "invalid_argument"}};

        QSslCertificate c(QByteArray::fromRawData((const char*)data.data(), int(data.size())), QSsl::Der);
        bool isNonRepudiation = false;
        for(const QSslCertificateExtension &ex: c.extensions())
        {
            if(ex.name() == QStringLiteral("keyUsage"))
            {
                for(const QVariant &item: ex.value().toList())
                    if(item.toString() == QStringLiteral("Non Repudiation"))
                        isNonRepudiation = true;
            }
        }
        QString label;
        if (isNonRepudiation) {
            label = Labels::l10n.get(selected.pinpad ? "sign PIN pinpad" : "sign PIN").c_str();
            label.replace("@PIN@", p11.signPINLabel.c_str());
        }
        else {
            label = Labels::l10n.get(selected.pinpad ? "auth PIN pinpad" : "auth PIN").c_str();
            label.replace("@PIN@", p11.authPINLabel.c_str());
        }
        QString infoText = info.toHtmlEscaped();
        if (digests.size() > 1) {
            QString count = QString::fromStdString(Labels::l10n.get("batch count"));
            count.replace("@COUNT@", QString::number(digests.size()));
            infoText = infoText.isEmpty() ? count : infoText + "<br/>" + count;
        }

        auto result = [batch](const std::vector<std::vector<unsigned char>> &signatures) -> QVariantMap {
            if (!batch)
                return {{"signature", toHex(signatures.at(0))}};
            QVariantList list;
            for (const std::vector<unsigned char> &signature: signatures)
                list.append(QString::fromLatin1(toHex(signature)));
            return {{"signatures", list}};
        };

        // Keys with a digital signature PIN need it once per batch. Like the dialog of the Windows
        // driver, the token PIN is tried first, it is often the same. Only when the token rejects it,
        // the digital signature PIN is asked and the next batch asks for both PINs in one dialog.
        // The token PIN is tried once per certificate, a rejected attempt counts against the
        // digital signature PIN tries.
        QSettings settings(QStringLiteral("MIT Consulting"), QStringLiteral("mit-token-signing"));
        const QString signaturePinKey = QStringLiteral("SignaturePin/") + QCryptographicHash::hash(
            QByteArray::fromRawData((const char*)data.data(), int(data.size())), QCryptographicHash::Sha1).toHex();
        const int memory = settings.value(signaturePinKey, SignaturePinUnknown).toInt();
        const bool askBoth = memory == SignaturePinSeparate && !selected.pinpad;

        bool isInitialCheck = true;
        for (int retriesLeft = selected.retry; retriesLeft > 0; ) {
            Signer dialog(Labels::l10n.get(digests.size() > 1 ? "batch title" : "sign title").c_str(), label, selected.minPinLen, selected.maxPinLen, selected.pinpad, askBoth);
            if (retriesLeft < 3) {
                dialog.errorLabel->show();
                dialog.errorLabel->setText(QStringLiteral("%1%2 %3")
                     .arg((!isInitialCheck ? Labels::l10n.get("incorrect PIN2") : "").c_str())
                     .arg(Labels::l10n.get("tries left").c_str())
                     .arg(retriesLeft));
            }
            isInitialCheck = false;
            dialog.nameLabel->setText(c.subjectInfo(QSslCertificate::CommonName).join(""));
            if (!infoText.isEmpty()) {
                dialog.infoLabel->setText(infoText);
                dialog.infoLabel->show();
            }
            std::future< std::vector<std::vector<unsigned char>> > signature;

            if (selected.pinpad) {
                signature = std::async(std::launch::async, [&]{
                    std::vector<std::vector<unsigned char>> result;
                    try {
                        result = pkcs11.signBatch(selected, digests, nullptr);
                        dialog.accept();
                    } catch (const AuthenticationError &) {
                        --retriesLeft;
                        dialog.done(AuthError);
                    } catch (const PinBlockedException &) {
                        retriesLeft = 0;
                        dialog.done(AuthError);
                    } catch (const AuthenticationBadInput &) {
                        dialog.done(AuthError);
                    } catch (const UserCancelledException &) {
                        dialog.done(UserCancel);
                    } catch (const BaseException &e) {
                        qDebug() << e.what();
                        dialog.setProperty("exception", QString::fromStdString(e.getErrorCode()));
                        dialog.done(TechnicalError);
                    }
                    return result;
                });
            }

            switch (dialog.exec())
            {
            case UserCancel:
                return {{"result", "user_cancel"}};
            case AuthError:
                continue;
            case TechnicalError:
                return {{"result", dialog.property("exception")}};
            default:
                if (selected.pinpad) {
                    return result(signature.get());
                }
            }

            try {
                bool signaturePinUsed = false, signaturePinIsTokenPin = false;
                std::vector<std::vector<unsigned char>> signatures = signBatch(pkcs11, selected, digests,
                    dialog.pin->text().toUtf8(), askBoth ? dialog.pin2->text().toUtf8() : QByteArray(),
                    dialog.nameLabel->text(), memory != SignaturePinSeparate, signaturePinUsed, signaturePinIsTokenPin);
                if (signaturePinUsed) {
                    int learned = signaturePinIsTokenPin ? SignaturePinSame : SignaturePinSeparate;
                    if (learned != memory)
                        settings.setValue(signaturePinKey, learned);
                }
                return result(signatures);
            } catch (const AuthenticationBadInput &) {
            } catch (const AuthenticationError &) {
                --retriesLeft;
            } catch (const PinBlockedException &) {
                retriesLeft = 0;
            } catch (const UserCancelledException &) {
                return {{"result", "user_cancel"}};
            } catch (const BaseException &e) {
                qDebug() << e.what();
                return {{"result", QString::fromStdString(e.getErrorCode())}};
            }
        }
        return {{"result", "pin_blocked"}};
    }

private:
    // Signs in a worker thread and keeps the UI responsive with a progress dialog,
    // the digital signature PIN dialog is shown in the UI thread when the key needs it
    static std::vector<std::vector<unsigned char>> signBatch(const PKCS11CardManager &pkcs11,
            const PKCS11CardManager::Token &selected, const std::vector<std::vector<unsigned char>> &digests,
            const QByteArray &pin, const QByteArray &signaturePin, const QString &name, bool tryTokenPin,
            bool &signaturePinUsed, bool &signaturePinIsTokenPin) {
        QProgressDialog progress(Labels::l10n.get("signing").c_str(), Labels::l10n.get("cancel").c_str(), 0, int(digests.size()));
        progress.setWindowFlags(Qt::WindowStaysOnTopHint);
        // The default size cuts the window title, the label says what is going on
        progress.setWindowTitle(QApplication::applicationDisplayName());
        progress.setMinimumWidth(480);
        progress.setMinimumDuration(digests.size() > 1 ? 0 : 2000);
        progress.setAutoClose(false);
        progress.setAutoReset(false);
        std::atomic<bool> cancelled(false);
        QObject::connect(&progress, &QProgressDialog::canceled, [&]{ cancelled = true; });

        auto signPin = [&](bool retry) {
            signaturePinUsed = true;
            signaturePinIsTokenPin = false;
            if (!retry && !signaturePin.isEmpty())
                return std::string(signaturePin.constData(), size_t(signaturePin.size()));
            if (!retry && tryTokenPin) {
                signaturePinIsTokenPin = true;
                return std::string(pin.constData(), size_t(pin.size()));
            }
            std::string result;
            QMetaObject::invokeMethod(&progress, [&] {
                Signer signDialog(Labels::l10n.get("signature PIN title").c_str(),
                    Labels::l10n.get("signature PIN").c_str(), selected.minPinLen, selected.maxPinLen, false);
                signDialog.nameLabel->setText(name);
                // Must not look like the token PIN dialog, the PINs usually differ
                signDialog.infoLabel->setText(QString::fromStdString(Labels::l10n.get("signature PIN info")).toHtmlEscaped());
                signDialog.infoLabel->setStyleSheet(QStringLiteral(
                    "QLabel { background: #fff4ce; color: #5c4400; border: 1px solid #e0b000; padding: 6px; }"));
                signDialog.infoLabel->show();
                signDialog.pinLabel->setText(QStringLiteral("<b>%1</b>").arg(signDialog.pinLabel->text().toHtmlEscaped()));
                if (signDialog.exec() == QDialog::Accepted)
                    result = signDialog.pin->text().toUtf8().constData();
            }, Qt::BlockingQueuedConnection);
            return result;
        };
        auto step = [&](size_t i) {
            if (cancelled)
                throw UserCancelledException();
            QMetaObject::invokeMethod(&progress, [&progress, i] { progress.setValue(int(i)); }, Qt::QueuedConnection);
        };
        std::future<std::vector<std::vector<unsigned char>>> future = std::async(std::launch::async, [&] {
            return pkcs11.signBatch(selected, digests, pin.constData(), signPin, step);
        });
        while (future.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        progress.close();
        return future.get();
    }

    static QByteArray toHex(const std::vector<unsigned char> &data)
    {
        return QByteArray::fromRawData((const char*)data.data(), int(data.size())).toHex();
    }

    static std::vector<unsigned char> fromHex(const QString &data)
    {
        QByteArray bin = QByteArray::fromHex(data.toLatin1());
        return std::vector<unsigned char>(bin.cbegin(), bin.cend());
    }

    Signer(const QString &heading, const QString &label, unsigned long minPinLen, unsigned long maxPinLen, bool isPinpad,
            bool withSignaturePin = false)
        : nameLabel(new QLabel(this))
        , infoLabel(new QLabel(this))
        , pinLabel(new QLabel(this))
        , errorLabel(new QLabel(this))
    {
        DialogStyle::Layout layout = DialogStyle::setup(this, heading);
        layout.content->addWidget(nameLabel);
        layout.content->addWidget(infoLabel);
        layout.content->addWidget(errorLabel);
        layout.content->addSpacing(8);
        layout.content->addWidget(pinLabel);

        setMinimumWidth(460);
        setWindowFlags(Qt::WindowStaysOnTopHint);
        DialogStyle::secondary(nameLabel);
        pinLabel->setText(label);
        pinLabel->setWordWrap(true);
        errorLabel->setTextFormat(Qt::PlainText);
        QFont bold = errorLabel->font();
        bold.setBold(true);
        errorLabel->setFont(bold);
        DialogStyle::error(errorLabel);
        errorLabel->hide();
        infoLabel->setTextFormat(Qt::RichText);
        infoLabel->setWordWrap(true);
        DialogStyle::secondary(infoLabel);
        infoLabel->hide();

        if(isPinpad) {
            setWindowFlags((windowFlags()|Qt::CustomizeWindowHint) & ~Qt::WindowCloseButtonHint);
            progress = new QProgressBar(this);
            progress->setRange(0, 30);
            progress->setValue(progress->maximum());
            progress->setTextVisible(false);

            statusTimer = new QTimeLine(progress->maximum() * 1000, this);
            statusTimer->setCurveShape(QTimeLine::LinearCurve);
            statusTimer->setFrameRange(progress->maximum(), progress->minimum());
            connect(statusTimer, &QTimeLine::frameChanged, progress, &QProgressBar::setValue);
            statusTimer->start();

            layout.footer->addWidget(progress);
        } else {
            buttons = new QDialogButtonBox(this);
            connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
            cancel = buttons->addButton(Labels::l10n.get("cancel").c_str(), QDialogButtonBox::RejectRole);
            ok = buttons->addButton("OK", QDialogButtonBox::AcceptRole);
            ok->setEnabled(false);
            ok->setDefault(true);

            pin = new QLineEdit(this);
            pin->setEchoMode(QLineEdit::Password);
            pin->setFocus();
            // Token passwords (e.g. SafeNet) may contain letters, length is limited by the token
            pin->setMaxLength(maxPinLen > 0 && maxPinLen < 64 ? int(maxPinLen) : 64);
            layout.content->addWidget(pin);

            // Digital signature PIN in the same dialog, both PINs are required
            if (withSignaturePin) {
                pin2Label = new QLabel(QStringLiteral("<b>%1</b>").arg(
                    QString::fromStdString(Labels::l10n.get("signature PIN")).toHtmlEscaped()), this);
                pin2 = new QLineEdit(this);
                pin2->setEchoMode(QLineEdit::Password);
                pin2->setMaxLength(pin->maxLength());
                layout.content->addSpacing(8);
                layout.content->addWidget(pin2Label);
                layout.content->addWidget(pin2);
            }
            auto update = [=] {
                ok->setEnabled(pin->text().size() >= int(minPinLen) && (!pin2 || pin2->text().size() >= int(minPinLen)));
            };
            connect(pin, &QLineEdit::textEdited, update);
            if (pin2)
                connect(pin2, &QLineEdit::textEdited, update);

            layout.footer->addWidget(buttons);
        }
        show();
    }

    QLabel *nameLabel, *infoLabel, *pinLabel, *errorLabel, *pin2Label = nullptr;
    QDialogButtonBox *buttons = nullptr;
    QPushButton *ok = nullptr, *cancel = nullptr;
    QLineEdit *pin = nullptr, *pin2 = nullptr;
    QProgressBar *progress = nullptr;
    QTimeLine *statusTimer = nullptr;
};
