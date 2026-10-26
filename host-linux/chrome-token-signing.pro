TEMPLATE = app
CONFIG += console c++11 link_pkgconfig
CONFIG -= app_bundle
QT += widgets network
isEmpty(VERSION):VERSION=1.2.0.0
PKGCONFIG += libpcsclite
INCLUDEPATH += ../host-shared
LIBS += -ldl
DEFINES += VERSION=\\\"$$VERSION\\\"
SOURCES += \
    ../host-shared/Labels.cpp \
    ../host-shared/Logger.cpp \
    ../host-shared/BinaryUtils.cpp \
    ../host-shared/PKCS11Path.cpp \
    chrome-host.cpp
HEADERS += *.h ../host-shared/*.h
RESOURCES += chrome-token-signing.qrc
target.path = /usr/lib/mit-token-signing
chromeconf.path = /etc/opt/chrome/native-messaging-hosts
chromeconf.files += cz.mit.consulting.json
chromiumconf.path = /etc/chromium/native-messaging-hosts
chromiumconf.files += cz.mit.consulting.json
edgeconf.path = /etc/opt/edge/native-messaging-hosts
edgeconf.files += cz.mit.consulting.json
INSTALLS += target chromeconf chromiumconf edgeconf
