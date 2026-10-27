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

#import "PINDialog.h"
#import "DialogStyle.h"

#import "BinaryUtils.h"
#import "PKCS11CardManager.h"
#import "Labels.h"
#import "PKCS11Path.h"

#include <CommonCrypto/CommonDigest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>

#define _L(KEY) @(Labels::l10n.get(KEY).c_str())

// What is known about the digital signature PIN of the certificate key
enum {
    SignaturePinUnknown = 0,
    SignaturePinSeparate = 1,
    SignaturePinSame = 2,
    SignaturePinNone = 3,
};

@interface MaxNumberFormatter : NSFormatter
@property NSInteger maxLength;
@end

@implementation MaxNumberFormatter
- (id)init
{
    if(self = [super init]) {
        self.maxLength = INT_MAX;
    }
    return self;
}

- (id)initWithCoder:(NSCoder *)aDecoder
{
    // support Nib based initialisation
    self = [super initWithCoder:aDecoder];
    if (self) {
        self.maxLength = INT_MAX;
    }
    return self;
}

- (NSString *)stringForObjectValue:(id)object
{
    if ([object isKindOfClass:[NSString class]]) {
        // A new NSString is perhaps not required here
        // but generically a new object would be generated
        return [NSString stringWithString:object];
    }
    return nil;
}

- (BOOL)getObjectValue:(id *)object forString:(NSString *)string errorDescription:(NSString **)error
{
    // Be sure to generate a new object here or binding woe ensues
    // when continuously updating bindings are enabled.
    *object = [NSString stringWithString:string];
    return YES;
}

- (BOOL)isPartialStringValid:(NSString **)partialStringPtr
       proposedSelectedRange:(NSRangePointer)proposedSelRangePtr
              originalString:(NSString *)origString
       originalSelectedRange:(NSRange)origSelRange
            errorDescription:(NSString **)error
{
    NSString *proposedString = *partialStringPtr;
    if ([proposedString length] > self.maxLength) {
        // The original string has been modified by one or more characters (via pasting).
        // Either way compute how much of the proposed string can be accommodated.
        NSInteger origLength = origString.length;
        NSInteger insertLength = self.maxLength - origLength;
        // If a range is selected then characters in that range will be removed
        // so adjust the insert length accordingly
        insertLength += origSelRange.length;
        // Get the string components
        NSString *prefix = [origString substringToIndex:origSelRange.location];
        NSString *suffix = [origString substringFromIndex:origSelRange.location + origSelRange.length];
        NSString *insert = [proposedString substringWithRange:NSMakeRange(origSelRange.location, insertLength)];
        // Assemble the final string
        *partialStringPtr = [NSString stringWithFormat:@"%@%@%@", prefix, insert, suffix];
        // Fix-up the proposed selection range
        proposedSelRangePtr->location = origSelRange.location + insertLength;
        proposedSelRangePtr->length = 0;
        return NO;
    }
    return YES;
}
@end

// Progress of a batch signed in a worker thread, the user can cancel it between signatures
@interface ProgressPanel : NSObject
@property (atomic) BOOL cancelled;
- (instancetype)initWithCount:(NSUInteger)count;
- (void)setStep:(NSUInteger)step;
- (void)runWhile:(const std::function<bool()> &)busy;
@end

@implementation ProgressPanel {
    NSPanel *panel;
    NSProgressIndicator *indicator;
    NSTextField *label;
    NSUInteger count;
}

- (instancetype)initWithCount:(NSUInteger)total
{
    if (self = [super init]) {
        count = total;
        panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 400, 110)
                                           styleMask:NSWindowStyleMaskTitled
                                             backing:NSBackingStoreBuffered
                                               defer:NO];
        panel.title = @"MIT Token signing";
        label = [NSTextField labelWithString:_L("signing")];
        label.frame = NSMakeRect(20, 74, 360, 20);
        indicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, 50, 360, 20)];
        indicator.indeterminate = NO;
        indicator.minValue = 0;
        indicator.maxValue = total;
        NSButton *cancel = [NSButton buttonWithTitle:_L("cancel") target:self action:@selector(cancelClicked:)];
        cancel.frame = NSMakeRect(280, 12, 100, 32);
        [panel.contentView addSubview:label];
        [panel.contentView addSubview:indicator];
        [panel.contentView addSubview:cancel];
        [panel center];
        [self setStep:0];
    }
    return self;
}

- (void)setStep:(NSUInteger)step
{
    indicator.doubleValue = step;
    label.stringValue = [NSString stringWithFormat:@"%@ %lu / %lu", _L("signing"), (unsigned long)(step + 1), (unsigned long)count];
}

// Shows the panel and processes events until busy returns false
- (void)runWhile:(const std::function<bool()> &)busy
{
    [NSApp activateIgnoringOtherApps:YES];
    NSModalSession session = [NSApp beginModalSessionForWindow:panel];
    while (busy()) {
        [NSApp runModalSession:session];
        [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    [NSApp endModalSession:session];
    [panel close];
}

- (void)cancelClicked:(id)sender
{
    self.cancelled = YES;
}
@end

static NSTouchBarItemIdentifier touchBarItemGroupId = @"cz.mit-consulting.chrome-token-signing.touchbar.group";
static NSTouchBarItemIdentifier touchBarItemOkId = @"cz.mit-consulting.chrome-token-signing.touchbar.ok";
static NSTouchBarItemIdentifier touchBarItemCancelId = @"cz.mit-consulting.chrome-token-signing.touchbar.cancel";

@interface PINPanel () <NSTouchBarDelegate>
- (void)setPinLength:(const PKCS11CardManager::Token &)token;
- (void)offerSignaturePin:(BOOL)same;
- (BOOL)sameChecked;
+ (std::string)signaturePin:(const PKCS11CardManager::Token &)token name:(NSString *)name retry:(BOOL)retry;
+ (std::vector<std::vector<unsigned char>>)signBatch:(const PKCS11CardManager &)pkcs11 token:(const PKCS11CardManager::Token &)token
    hashes:(const std::vector<std::vector<unsigned char>> &)hashes pin:(const std::string &)pin
    signaturePin:(const std::string &)signaturePin same:(BOOL)same name:(NSString *)name;
+ (std::vector<std::vector<unsigned char>>)signWithProgress:(const PKCS11CardManager &)pkcs11 token:(const PKCS11CardManager::Token &)token
    hashes:(const std::vector<std::vector<unsigned char>> &)hashes pin:(const std::string &)pin signPin:(const PKCS11CardManager::SignPin &)signPin;
+ (NSString *)fingerprint:(const std::vector<unsigned char> &)cert;
@end

@implementation PINPanel {
    IBOutlet NSPanel *window;
    IBOutlet NSButton *okButton;
    IBOutlet NSButton *cancelButton;
    IBOutlet NSTextField *nameLabel;
    IBOutlet NSTextField *messageField;
    IBOutlet NSSecureTextField *pinField;
    IBOutlet NSTextField *pinFieldLabel;
    IBOutlet NSProgressIndicator *progressBar;
    NSTextField *headingField, *errorField;
    NSButton *sameBox;
    NSTextField *pin2Label;
    NSSecureTextField *pin2Field;
    NSArray<NSView *> *rows;
    unsigned long minPinLen;
}

- (instancetype)init:(NSString*)label pinpad:(BOOL)pinpad
{
    if (self = [super init]) {
        if (![[NSBundle bundleForClass:PINPanel.class] loadNibNamed:pinpad ? @"PINPadDialog" : @"PINDialog" owner:self topLevelObjects:nil]) {
            self = nil;
            return self;
        }
        if (pinpad) {
            progressBar.doubleValue = 30;
            [progressBar startAnimation:self];
        }
        else {
            cancelButton.title = _L("cancel");
        }
        window.touchBar = [self makeTouchBar];
        pinFieldLabel.stringValue = label;
        errorField = [NSTextField labelWithString:@""];
        MITErrorText(errorField);
        errorField.hidden = YES;
        MITSecondaryText(nameLabel);
        MITSecondaryText(messageField);
        rows = @[nameLabel, messageField, errorField, pinFieldLabel, pinpad ? progressBar : pinField];
        if (!pinpad) {
            // The box that the digital signature PIN is the token PIN and a field for it otherwise,
            // shown by offerSignaturePin
            sameBox = [NSButton checkboxWithTitle:_L("signature PIN same") target:self action:@selector(sameToggled:)];
            pin2Label = [NSTextField labelWithString:_L("signature PIN")];
            pin2Label.font = [NSFont boldSystemFontOfSize:NSFont.systemFontSize];
            pin2Field = [[NSSecureTextField alloc] init];
            pin2Field.formatter = pinField.formatter;
            pin2Field.delegate = self;
            for (NSView *view in @[sameBox, pin2Label, pin2Field]) {
                view.hidden = YES;
            }
            rows = [rows arrayByAddingObjectsFromArray:@[sameBox, pin2Label, pin2Field]];
        }
        headingField = MITStyleDialog(window, rows, pinpad ? @[] : @[cancelButton, okButton], 460);
        // The new content view has no focus, typing goes to the PIN field
        if (!pinpad) {
            window.initialFirstResponder = pinField;
        }
    }
    return self;
}

+ (NSDictionary *)show:(NSDictionary*)params cert:(NSString *)cert batch:(BOOL)batch
{
    // Hashes of SIGN or SIGN_BATCH request, all hex encoded and of the same supported length
    NSArray *list = batch ? params[@"hashes"] : (params[@"hash"] ? @[params[@"hash"]] : nil);
    if (!params[@"cert"] || ![list isKindOfClass:NSArray.class] || list.count == 0 || list.count > MAX_BATCH) {
        return @{@"result": @"invalid_argument"};
    }
    NSCharacterSet *nonHex = [NSCharacterSet characterSetWithCharactersInString:@"0123456789abcdefABCDEF"].invertedSet;
    std::vector<std::vector<unsigned char>> hashes;
    for (id item in list) {
        if (![item isKindOfClass:NSString.class]) {
            return @{@"result": @"invalid_argument"};
        }
        NSString *hash = item;
        NSUInteger length = hash.length;
        if ([hash rangeOfCharacterFromSet:nonHex].location != NSNotFound ||
            (length != 40 && length != 56 && length != 64 && length != 96 && length != 128) ||
            (!hashes.empty() && hashes[0].size() * 2 != length)) {
            return @{@"result": @"invalid_argument"};
        }
        hashes.push_back(BinaryUtils::hex2bin(hash.UTF8String));
    }

    NSString *info = [params[@"info"] isKindOfClass:NSString.class] ? params[@"info"] : @"";
    if (info.length > 500) {
        return @{@"result": @"technical_error"};
    }
    // Shown only when the page sends it, the PIN dialog shows the number of documents
    NSString *count = hashes.size() > 1 ?
        [_L("batch count") stringByReplacingOccurrencesOfString:@"@COUNT@" withString:@(hashes.size()).stringValue] : @"";
    if (info.length > 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert addButtonWithTitle:@"OK"];
        [alert addButtonWithTitle:_L("cancel")];
        alert.messageText = info;
        alert.alertStyle = NSAlertStyleInformational;
        alert.icon = [[NSImage alloc] initByReferencingFile:@"/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/AlertNoteIcon.icns"];
        alert.window.title = @"MIT Token signing";
        [NSApp activateIgnoringOtherApps:YES];
        if ([alert runModal] != NSAlertFirstButtonReturn) {
            return @{@"result": @"user_cancel"};
        }
    }

    PKCS11Path::Params p11 = PKCS11Path::getPkcs11ModulePath();
    std::unique_ptr<PKCS11CardManager> pkcs11;
    PKCS11CardManager::Token selected;
    try {
        pkcs11.reset(new PKCS11CardManager(p11.path));
        for (const PKCS11CardManager::Token &token : pkcs11->tokens()) {
            if (BinaryUtils::hex2bin(cert.UTF8String) == token.cert) {
                selected = token;
                break;
            }
        }
    }
    catch(const BaseException &e) {
        _log("Exception: %s", e.what());
        return @{@"result": @(e.getErrorCode().c_str())};
    }

    if (selected.cert.empty()) {
        return @{@"result": @"invalid_argument"};
    }

    CFDataRef data = CFDataCreateWithBytesNoCopy(nil, selected.cert.data(), selected.cert.size(), kCFAllocatorNull);
    SecCertificateRef x509 = SecCertificateCreateWithData(nil, data);
    CFRelease(data);
    NSDictionary *dict = CFBridgingRelease(SecCertificateCopyValues(x509, nil, nil));
    CFRelease(x509);

    auto result = [batch](const std::vector<std::vector<unsigned char>> &signatures) -> NSDictionary* {
        if (!batch) {
            return @{@"signature": @(BinaryUtils::bin2hex(signatures.at(0)).c_str()), @"result": @"ok"};
        }
        NSMutableArray *list = [NSMutableArray array];
        for (const std::vector<unsigned char> &signature : signatures) {
            [list addObject:@(BinaryUtils::bin2hex(signature).c_str())];
        }
        return @{@"signatures": list, @"result": @"ok"};
    };

    // PKCS#11 cannot tell before the token login whether the key has a digital signature PIN, nor
    // how many tries of it are left, so the PIN dialog has a box that the digital signature PIN is
    // the same and a field for it otherwise. The token PIN is tried only when the user leaves the
    // box checked; when the token rejects it, a dialog asks for the digital signature PIN. The answer
    // is the initial state of the box next time, keys without a digital signature PIN get no box.
    NSString *memoryKey = [@"SignaturePin." stringByAppendingString:[PINPanel fingerprint:selected.cert]];
    const NSInteger memory = [NSUserDefaults.standardUserDefaults integerForKey:memoryKey];
    BOOL same = memory == SignaturePinUnknown || memory == SignaturePinSame;
    bool isInitialCheck = true;
    for (int retriesLeft = selected.retry; retriesLeft > 0; )
    {
        NSString *label;
        NSNumber *ku = dict[(__bridge id)kSecOIDKeyUsage][(__bridge id)kSecPropertyKeyValue];
        if (ku.unsignedIntValue & kSecKeyUsageNonRepudiation) {
            label = [_L(selected.pinpad ? "sign PIN pinpad" : "sign PIN") stringByReplacingOccurrencesOfString:@"@PIN@" withString:@(p11.signPINLabel.c_str())];
        }
        else {
            label = [_L(selected.pinpad ? "auth PIN pinpad" : "auth PIN") stringByReplacingOccurrencesOfString:@"@PIN@" withString:@(p11.authPINLabel.c_str())];
        }
        PINPanel *dialog = [[PINPanel alloc] init:label pinpad:selected.pinpad];
        if (!dialog) {
            return @{@"result": @"technical_error"};
        }
        [dialog setPinLength:selected];
        if (!selected.pinpad && memory != SignaturePinNone) {
            [dialog offerSignaturePin:same];
        }
        for (NSDictionary *item in dict[(__bridge id)kSecOIDX509V1SubjectName][(__bridge id)kSecPropertyKeyValue]) {
            if ([item[(__bridge id)kSecPropertyKeyLabel] isEqualToString:(__bridge id)kSecOIDCommonName]) {
                dialog->nameLabel.stringValue = item[(__bridge id)kSecPropertyKeyValue];
            }
        }

        NSDictionary *pinpadresult;
        std::future<void> future;
        NSTimer *timer;
        if (selected.pinpad) {
            timer = [NSTimer scheduledTimerWithTimeInterval:1.0 target:dialog selector:@selector(handleTimerTick:) userInfo:nil repeats:YES];
            [NSRunLoop.currentRunLoop addTimer:timer forMode:NSModalPanelRunLoopMode];
            future = std::async(std::launch::async, [&] {
                try {
                    pinpadresult = result(pkcs11->signBatch(selected, hashes, nullptr));
                    [NSApp stopModal];
                }
                catch(const UserCancelledException &) {
                    [NSApp abortModal];
                }
                catch(const AuthenticationError &) {
                    --retriesLeft;
                    [NSApp stopModal];
                }
                catch(const PinBlockedException &) {
                    retriesLeft = 0;
                    [NSApp stopModal];
                }
                catch(const AuthenticationBadInput &) {
                    [NSApp stopModal];
                }
                catch(const BaseException &e) {
                    _log("Exception: %s", e.what());
                    pinpadresult = @{@"result": @(e.getErrorCode().c_str())};
                    [NSApp stopModal];
                }
            });
        }

        dialog->headingField.stringValue = _L(hashes.size() > 1 ? "batch title" : "sign title");
        dialog->messageField.stringValue = count;
        dialog->messageField.hidden = count.length == 0;
        if (retriesLeft < 3) {
            dialog->errorField.stringValue = [NSString stringWithFormat:@"%@%@ %u",
                                              (isInitialCheck ? @"" : _L("incorrect PIN2")),
                                              _L("tries left"),
                                              retriesLeft];
            dialog->errorField.hidden = NO;
        }
        isInitialCheck = false;
        MITFitDialog(dialog->window, dialog->rows, dialog->pinFieldLabel);

        [NSApp activateIgnoringOtherApps:YES];
        NSModalResponse response = [NSApp runModalForWindow:dialog->window];
        [dialog->window close];

        if (timer) {
            [timer invalidate];
            timer = nil;
        }

        if (response == NSModalResponseAbort) {
            return @{@"result": @"user_cancel"};
        }

        if (selected.pinpad) {
            future.wait();
            if (pinpadresult) {
                return pinpadresult;
            }
        }
        else {
            std::string pin = dialog->pinField.stringValue.UTF8String;
            if (!dialog->sameBox.hidden) {
                same = [dialog sameChecked];
            }
            std::string signaturePin = dialog->pin2Field.hidden || same ? std::string() : dialog->pin2Field.stringValue.UTF8String;
            NSString *name = dialog->nameLabel.stringValue;
            try {
                std::vector<std::vector<unsigned char>> signatures = [PINPanel signBatch:*pkcs11 token:selected hashes:hashes
                    pin:pin signaturePin:signaturePin same:same name:name];
                std::fill(pin.begin(), pin.end(), '\0');
                std::fill(signaturePin.begin(), signaturePin.end(), '\0');
                return result(signatures);
            }
            catch(const AuthenticationBadInput &) {
            }
            catch(const AuthenticationError &) {
                --retriesLeft;
            }
            catch(const PinBlockedException &) {
                retriesLeft = 0;
            }
            catch(const UserCancelledException &) {
                std::fill(pin.begin(), pin.end(), '\0');
                return @{@"result": @"user_cancel"};
            }
            catch(const BaseException &e) {
                _log("Exception: %s", e.what());
                std::fill(pin.begin(), pin.end(), '\0');
                std::fill(signaturePin.begin(), signaturePin.end(), '\0');
                return @{@"result": @(e.getErrorCode().c_str())};
            }
            std::fill(pin.begin(), pin.end(), '\0');
            std::fill(signaturePin.begin(), signaturePin.end(), '\0');
        }
    }
    return @{@"result": @"pin_blocked"};
}

// Asks for the digital signature PIN of keys with CKA_ALWAYS_AUTHENTICATE when the token PIN dialog
// did not give it or the token rejected it, empty when cancelled
+ (std::string)signaturePin:(const PKCS11CardManager::Token &)token name:(NSString *)name retry:(BOOL)retry
{
    PINPanel *signDialog = [[PINPanel alloc] init:_L("signature PIN") pinpad:NO];
    if (!signDialog) {
        return std::string();
    }
    [signDialog setPinLength:token];
    signDialog->nameLabel.stringValue = name;
    // Must not look like the token PIN dialog, the PINs usually differ
    signDialog->headingField.stringValue = _L("signature PIN title");
    signDialog->messageField.stringValue = _L("signature PIN info");
    signDialog->messageField.textColor = NSColor.systemOrangeColor;
    signDialog->pinFieldLabel.font = [NSFont boldSystemFontOfSize:NSFont.systemFontSize];
    if (retry) {
        signDialog->errorField.stringValue = _L("incorrect PIN2");
        signDialog->errorField.hidden = NO;
    }
    MITFitDialog(signDialog->window, signDialog->rows, signDialog->pinFieldLabel);
    [NSApp activateIgnoringOtherApps:YES];
    NSModalResponse signResult = [NSApp runModalForWindow:signDialog->window];
    [signDialog->window close];
    if (signResult == NSModalResponseAbort) {
        return std::string();
    }
    return std::string(signDialog->pinField.stringValue.UTF8String);
}

// Signs in a worker thread and keeps the UI responsive with a progress panel for a batch,
// the digital signature PIN dialog is shown in the main thread when the key needs it
+ (std::vector<std::vector<unsigned char>>)signBatch:(const PKCS11CardManager &)pkcs11 token:(const PKCS11CardManager::Token &)token
    hashes:(const std::vector<std::vector<unsigned char>> &)hashes pin:(const std::string &)pin
    signaturePin:(const std::string &)signaturePin same:(BOOL)same name:(NSString *)name
{
    // The token PIN is the digital signature PIN only when the user left the box checked,
    // the remembered answer is the initial state of the box next time
    NSString *memoryKey = [@"SignaturePin." stringByAppendingString:[PINPanel fingerprint:token.cert]];
    NSInteger memory = [NSUserDefaults.standardUserDefaults integerForKey:memoryKey];
    bool signaturePinUsed = false, signaturePinIsTokenPin = false;
    auto signPin = [&](bool retry) -> std::string {
        signaturePinUsed = true;
        signaturePinIsTokenPin = !retry && same;
        if (signaturePinIsTokenPin) {
            return pin;
        }
        if (!retry && !signaturePin.empty()) {
            return signaturePin;
        }
        if (NSThread.isMainThread) {
            return [PINPanel signaturePin:token name:name retry:retry];
        }
        __block std::string result;
        dispatch_sync(dispatch_get_main_queue(), ^{
            result = [PINPanel signaturePin:token name:name retry:retry];
        });
        return result;
    };
    std::vector<std::vector<unsigned char>> signatures = hashes.size() == 1 ?
        pkcs11.signBatch(token, hashes, pin.c_str(), signPin) :
        [PINPanel signWithProgress:pkcs11 token:token hashes:hashes pin:pin signPin:signPin];
    NSInteger learned = !signaturePinUsed ? SignaturePinNone :
        signaturePinIsTokenPin ? SignaturePinSame : SignaturePinSeparate;
    if (learned != memory) {
        [NSUserDefaults.standardUserDefaults setInteger:learned forKey:memoryKey];
    }
    return signatures;
}

// SHA-256 of the certificate as hex, key of the digital signature PIN memory
+ (NSString *)fingerprint:(const std::vector<unsigned char> &)cert
{
    unsigned char hash[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(cert.data(), CC_LONG(cert.size()), hash);
    return @(BinaryUtils::bin2hex(std::vector<unsigned char>(hash, hash + sizeof(hash))).c_str());
}

+ (std::vector<std::vector<unsigned char>>)signWithProgress:(const PKCS11CardManager &)pkcs11 token:(const PKCS11CardManager::Token &)token
    hashes:(const std::vector<std::vector<unsigned char>> &)hashes pin:(const std::string &)pin signPin:(const PKCS11CardManager::SignPin &)signPin
{
    ProgressPanel *progress = [[ProgressPanel alloc] initWithCount:hashes.size()];
    auto step = [&, progress](size_t i) {
        if (progress.cancelled) {
            throw UserCancelledException();
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            [progress setStep:i];
        });
    };
    std::future<std::vector<std::vector<unsigned char>>> future = std::async(std::launch::async, [&] {
        return pkcs11.signBatch(token, hashes, pin.c_str(), signPin, step);
    });
    [progress runWhile:[&] {
        return future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
    }];
    return future.get();
}

- (IBAction)okClicked:(id)sender
{
    if (okButton.enabled) {
        [NSApp stopModal];
    }
}

// Shows the box that the digital signature PIN is the token PIN, checked by same, and the field for it
- (void)offerSignaturePin:(BOOL)same
{
    sameBox.state = same ? NSControlStateValueOn : NSControlStateValueOff;
    for (NSView *view in @[sameBox, pin2Label, pin2Field]) {
        view.hidden = NO;
    }
    NSStackView *body = (NSStackView *)sameBox.superview;
    [body setCustomSpacing:14 afterView:sameBox];
    window.autorecalculatesKeyViewLoop = YES;
    [self sameToggled:nil];
}

- (BOOL)sameChecked
{
    return sameBox && !sameBox.hidden && sameBox.state == NSControlStateValueOn;
}

- (void)sameToggled:(id)sender
{
    pin2Field.enabled = ![self sameChecked];
    pin2Label.textColor = pin2Field.enabled ? NSColor.labelColor : NSColor.disabledControlTextColor;
    if (pin2Field.enabled && sender) {
        [window makeFirstResponder:pin2Field];
    }
    [self controlTextDidChange:nil];
}

- (IBAction)cancelClicked:(id)sender
{
    [NSApp abortModal];
}

// Token passwords (e.g. SafeNet) may contain letters, length is limited by the token
- (void)setPinLength:(const PKCS11CardManager::Token &)token
{
    minPinLen = token.minPinLen;
    if ([pinField.formatter isKindOfClass:MaxNumberFormatter.class]) {
        ((MaxNumberFormatter *)pinField.formatter).maxLength =
            token.maxPinLen > 0 && token.maxPinLen < 64 ? NSInteger(token.maxPinLen) : 64;
    }
}

- (void)controlTextDidChange:(NSNotification*)notification;
{
    BOOL needPin2 = pin2Field && !pin2Field.hidden && ![self sameChecked];
    BOOL enabled = pinField.stringValue.length >= minPinLen && (!needPin2 || pin2Field.stringValue.length >= minPinLen);
    if (okButton.enabled != enabled)
    {
        okButton.enabled = enabled;
        window.touchBar = [self makeTouchBar];
    }
}

- (void)handleTimerTick:(NSTimer*)timer
{
    [progressBar incrementBy:-1];
    if (progressBar.doubleValue <= 0) {
        [progressBar stopAnimation:self];
        [NSApp abortModal];
    }
}

#pragma mark - NSTouchBarProvider

- (NSTouchBar *)makeTouchBar
{
    NSTouchBar *touchBar = [[NSTouchBar alloc] init];
    touchBar.delegate = self;
    touchBar.defaultItemIdentifiers = @[touchBarItemGroupId];
    touchBar.principalItemIdentifier = touchBarItemGroupId;
    return touchBar;
}

#pragma mark - NSTouchBarDelegate

- (NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier
{
    if ([identifier isEqualToString:touchBarItemGroupId])
    {
        NSArray *items = @[[self touchBar:touchBar makeItemForIdentifier:touchBarItemCancelId],
                           [self touchBar:touchBar makeItemForIdentifier:touchBarItemOkId]];
        return [NSGroupTouchBarItem groupItemWithIdentifier:identifier items:items];
    }
    if ([identifier isEqualToString:touchBarItemOkId])
    {
        NSButton *button = [NSButton buttonWithTitle:@"OK" target:self action:@selector(okClicked:)];
        button.enabled = okButton.enabled;
        if (okButton.enabled)
            button.bezelColor = NSColor.selectedMenuItemColor;
        NSCustomTouchBarItem *touchBarItem = [[NSCustomTouchBarItem alloc] initWithIdentifier:identifier];
        touchBarItem.view = button;
        return touchBarItem;
    }
    if ([identifier isEqualToString:touchBarItemCancelId])
    {
        NSCustomTouchBarItem *touchBarItem = [[NSCustomTouchBarItem alloc] initWithIdentifier:identifier];
        touchBarItem.view = [NSButton buttonWithTitle:_L("cancel") target:self action:@selector(cancelClicked:)];
        return touchBarItem;
    }
    return nil;
}

@end
