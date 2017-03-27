TEMPLATE = app

QT += qml quick
SOURCES += main.cpp


target.path = /usr/bin
qml.files = transparent.qml
qml.path = /usr/share/transparent-controls
INSTALLS += target qml
