QT += xml
TEMPLATE        = lib
CONFIG         += plugin
INCLUDEPATH    += ../CorePlugin
HEADERS         = nmeacompass.h \
    ../CorePlugin/coreplugin.h
SOURCES         = nmeacompass.cpp \
    ../CorePlugin/coreplugin.cpp \
    exportplug.cpp
TARGET          = $$qtLibraryTarget(nmeacompass)
DESTDIR     = ../../Release/plugins

INCLUDEPATH += ../../UpWindNMEA/src
HEADERS += ../../UpWindNMEA/src/settingsmanager.h
SOURCES += ../../UpWindNMEA/src/settingsmanager.cpp

MOC_DIR = mocs
OBJECTS_DIR = objects
UI_DIR = uis

target.path = ./
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS NMEACompass.pro
sources.path = ./
INSTALLS += target sources

FORMS += \
    compasswidget.ui
