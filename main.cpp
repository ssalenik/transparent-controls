#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc,argv);

    QQmlApplicationEngine engine;
    engine.load(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + QLatin1String("/transparent.qml")));

    return app.exec();
}
