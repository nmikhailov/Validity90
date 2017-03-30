TEMPLATE = app
LIBS += -lusb-1.0 -lssl3 -lsmime3 -lnss3 -lnssutil3 -lplds4 -lplc4 -lnspr4
CONFIG += console
CONFIG += c99

CONFIG -= app_bundle
CONFIG -= qt
INCLUDEPATH += /usr/include/libusb-1.0 /usr/include/nss /usr/include/nspr

SOURCES += main.c

HEADERS += \
    constants.h
