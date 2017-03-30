TEMPLATE = app

QT += qml quick
SOURCES += main.cpp

CONFIG +=link_pkgconfig
PKGCONFIG +=gstreamer-1.0

target.path = /opt/transparent
qml.files = transparent.qml
qml.path = /opt/transparent
INSTALLS += target qml
