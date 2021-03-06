TEMPLATE = app
CONFIG += console
CONFIG -= qt

LIBS += -lrt
RCC_DIR = ./build/rcc
UI_DIR = ./build/ui
MOC_DIR = ./build/moc
DESTDIR = ./app

win32:CONFIG += qaxcontainer

CONFIG(debug, debug|release):{
DEFINES+=DEBUG
OBJECTS_DIR = ./build/debug/obj
} else: {
OBJECTS_DIR = ./build/release/obj
}

CONFIG(debug, debug|release):DEFINES += DEBUG
DEPENDPATH += $$PWD/netLink/src
INCLUDEPATH =+ $$PWD/netLink/include
DEPENDPATH += $$PWD/tinyxml

SOURCES += main.cpp \
    netLink/src/util.cc \
    netLink/src/socket_group.cc \
    netLink/src/socket.cc \
    netLink/src/smart_buffer.cc \
    netLink/src/core.cc \
    regexp/WCPattern.cpp \
    regexp/WCMatcher.cpp \
    regexp/Pattern.cpp \
    regexp/Matcher.cpp \
    tinyxml2/tinyxml2.cpp \
    md5.c \
    monitoractivity.cpp

OTHER_FILES += \
    netLink/include/netlink/exception.code.inc

HEADERS += \
    netLink/include/netlink/util.inline.h \
    netLink/include/netlink/util.h \
    netLink/include/netlink/socket_group.inline.h \
    netLink/include/netlink/socket_group.h \
    netLink/include/netlink/socket.inline.h \
    netLink/include/netlink/socket.h \
    netLink/include/netlink/smart_buffer.inline.h \
    netLink/include/netlink/smart_buffer.h \
    netLink/include/netlink/release_manager.inline.h \
    netLink/include/netlink/release_manager.h \
    netLink/include/netlink/netlink.h \
    netLink/include/netlink/exception.h \
    netLink/include/netlink/core.h \
    netLink/include/netlink/config.h \
    regexp/WCPattern.h \
    regexp/WCMatcher.h \
    regexp/Pattern.h \
    regexp/Matcher.h \
    md5.h \
    tinyxml2/tinyxml2.h \
    main.h \
    monitoractivity.h

