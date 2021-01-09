/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

//Qt includes
#include <QApplication>
#include <QThread>
#include <QThreadPool>
#include <QMetaObject>
#include <QThreadPool>
#include <QSettings>
#include <QSurfaceFormat>

//Our includes
#include "cwSettings.h"
#include "cwTask.h"
#include "cwOpenGLSettings.h"

int main( int argc, char* argv[] )
{    
    QApplication::setOrganizationName("Vadose Solutions");
    QApplication::setOrganizationDomain("cavewhere.com");
    QApplication::setApplicationName("cavewhere-test");
    QApplication::setApplicationVersion("1.0");

    cwOpenGLSettings::setApplicationRenderer();
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_DisableShaderDiskCache);

    //This is a workaround to https://bugreports.qt.io/browse/QTBUG-83871
    qputenv("QT3D_SCENE3D_STOP_RENDER_HIDDEN", "1");

    QApplication app(argc, argv);

    {
        QSettings settings;
        settings.clear();
    }

    cwSettings::initialize();

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setAlphaBufferSize(8);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    //    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    app.thread()->setObjectName("Main QThread");

    int result = 0;
    QMetaObject::invokeMethod(&app, [&result, argc, argv]() {
        result = Catch::Session().run( argc, argv );
        QThreadPool::globalInstance()->waitForDone();
        cwTask::threadPool()->waitForDone();
        QApplication::quit();
    }, Qt::QueuedConnection);

    app.exec();

    return result;
}

