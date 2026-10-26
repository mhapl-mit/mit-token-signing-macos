#
# Chrome Token Signing Native Host
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

BUILD_NUMBER ?= 0
include ../VERSION.mk
# Common name of the Developer ID certificates without the type prefix, e.g. "M.I.T. Consulting, s.r.o. (TEAMID)"
SIGNER ?= M.I.T. Consulting, s.r.o.
# Credentials for notarytool, by default a keychain profile created by
# xcrun notarytool store-credentials mit-token-signing --apple-id <id> --team-id <team>
NOTARY_ARGS ?= --keychain-profile mit-token-signing
HOST_NAME = cz.mit.consulting
PKG_ID = cz.mit-consulting.token-signing
PKG = mit-token-signing_$(VERSIONEX).pkg
APP_DIR = /Library/Application Support/MIT Token Signing
TMP_ROOT = $(CURDIR)/tmp
APP = $(TMP_ROOT)$(APP_DIR)/chrome-token-signing.app
COMPONENT_PLIST = $(CURDIR)/tmp_component.plist
PROJ = xcodebuild -project chrome-token-signing.xcodeproj -configuration Release \
	ARCHS="arm64 x86_64" ONLY_ACTIVE_ARCH=NO \
	MARKETING_VERSION=$(VERSION) CURRENT_PROJECT_VERSION=$(BUILD_NUMBER)
# The app must not be relocatable, otherwise installer updates any other copy with the same bundle ID
PKG_COMPONENT = pkgbuild --analyze --root $(TMP_ROOT) $(COMPONENT_PLIST) && \
	plutil -replace 0.BundleIsRelocatable -bool NO $(COMPONENT_PLIST)
PKG_BUILD = pkgbuild --root $(TMP_ROOT) --component-plist $(COMPONENT_PLIST) \
	--identifier $(PKG_ID) --version $(VERSIONEX) --install-location /

.PHONY: build codesign pkg signed notarize clean install uninstall test

# Builds universal (arm64 + x86_64) app and native messaging manifests into $(TMP_ROOT)
build:
	rm -rf $(TMP_ROOT)
	$(PROJ) install DSTROOT=$(TMP_ROOT)

codesign: build
	codesign -f --timestamp --options=runtime --entitlements chrome-token-signing.entitlements \
		-s "Developer ID Application: $(SIGNER)" "$(APP)"
	codesign --verify --strict --verbose=2 "$(APP)"

pkg: build
	$(PKG_COMPONENT)
	$(PKG_BUILD) $(PKG)

signed: codesign
	$(PKG_COMPONENT)
	$(PKG_BUILD) --sign "Developer ID Installer: $(SIGNER)" $(PKG)

notarize: signed
	xcrun notarytool submit $(PKG) $(NOTARY_ARGS) --wait
	xcrun stapler staple $(PKG)

clean:
	$(PROJ) clean
	rm -rf $(TMP_ROOT) $(COMPONENT_PLIST) build *.pkg

# helpers

install: build
	sudo ditto $(TMP_ROOT) /

uninstall:
	sudo rm -rf "$(APP_DIR)"
	sudo rm -f "/Library/Google/Chrome/NativeMessagingHosts/$(HOST_NAME).json" \
		"/Library/Microsoft/Edge/NativeMessagingHosts/$(HOST_NAME).json" \
		"/Library/Application Support/Chromium/NativeMessagingHosts/$(HOST_NAME).json"
	-sudo pkgutil --forget $(PKG_ID)

test: build
	python3 ../host-test/smoke-test.py "$(APP)/Contents/MacOS/chrome-token-signing"
