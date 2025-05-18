#define CATCH_CONFIG_RUNNER
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

//Qt includes
#include <QGuiApplication>
#include <QThread>
#include <QSettings>

//CaveWhere includes
#include "cwTask.h"

int main( int argc, char* argv[] )
{
    QGuiApplication app(argc, argv);

    QGuiApplication::setOrganizationName("Vadose Solutions");
    QGuiApplication::setOrganizationDomain("cavewhere.com");
    QGuiApplication::setApplicationName("cavewhere-sketch-test");
    QGuiApplication::setApplicationVersion("1.0");

    {
        QSettings settings;
        settings.clear();
    }

    app.thread()->setObjectName("Main QThread");

    cwTask::initilizeThreadPool();

    int result = 0;
    QMetaObject::invokeMethod(&app, [&result, argc, argv]() {
        result = Catch::Session().run( argc, argv );
        QCoreApplication::quit();
    }, Qt::QueuedConnection);

    app.exec();

    return result;
}

