TEMPLATE = app

QT += qml quick
SOURCES += main.cpp


target.path = /opt/transparent
qml.files = transparent.qml
qml.path = /opt/transparent
INSTALLS += target qml
