//Qt includes
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>

//CaveWhere
#include "cwTask.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    cwTask::initilizeThreadPool();

    // Configure multisample antialiasing
    QSurfaceFormat format;
    format.setSamples(4); // Adjust the sample count as needed
    QSurfaceFormat::setDefaultFormat(format);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // QString resourcePath = QCoreApplication::applicationDirPath() + "/../../../";
    // qDebug() << "resourcePath:" << resourcePath;
    // engine.addImportPath(resourcePath);

    engine.loadFromModule("CaveWhereSketch", "Main");

    return app.exec();
}
