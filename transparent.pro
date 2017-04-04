TEMPLATE = app

QT += qml quick
HEADERS += Pipeline.h
SOURCES += main.cpp pipeline.cpp

CONFIG +=link_pkgconfig
PKGCONFIG +=gstreamer-1.0

target.path = /opt/transparent
qml.files = transparent.qml
qml.path = /opt/transparent
INSTALLS += target qml
