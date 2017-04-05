#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <gst/gst.h>

#include "pipeline.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc,argv);

    gst_init (&argc, &argv);

    QQmlApplicationEngine engine;

    Pipeline *pipeline = new Pipeline(&engine);
    engine.rootContext()->setContextProperty(QLatin1String("pipeline"), pipeline);

    engine.load(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + QLatin1String("/transparent.qml")));

    return app.exec();
}
