#define CATCH_CONFIG_RUNNER
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

//Qt includes
#include <QCoreApplication>
#include <QThread>
#include <QSettings>

int main( int argc, char* argv[] )
{
    QCoreApplication app(argc, argv);

    QCoreApplication::setOrganizationName("Vadose Solutions");
    QCoreApplication::setOrganizationDomain("cavewhere.com");
    QCoreApplication::setApplicationName("cavewhere-sketch-test");
    QCoreApplication::setApplicationVersion("1.0");

    {
        QSettings settings;
        settings.clear();
    }

    app.thread()->setObjectName("Main QThread");

    int result = 0;
    QMetaObject::invokeMethod(&app, [&result, argc, argv]() {
        result = Catch::Session().run( argc, argv );
        QCoreApplication::quit();
    }, Qt::QueuedConnection);

    app.exec();

    return result;
}

