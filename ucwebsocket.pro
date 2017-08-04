TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += server.c \
    sha1.c \
    base64.c \
    websocket.c \
    wshandshake.c

HEADERS += \
    sha1.h \
    base64.h \
    websocket.h \
    wshandshake.h

win32 {

    LIBS += -lWSock32
}
