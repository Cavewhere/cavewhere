/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QGuiApplication>
#include <QApplication>
#include <QQmlContext>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>
#include <QThreadPool>

//Our includes
//#include "cwMainWindow.h"
#include "cwImage.h"
#include "cwGlobalDirectory.h"
#include "cwQMLRegister.h"
#include "cwRootData.h"
#include "cwProject.h"
#include "cwImageProvider.h"
#include "cwOpenFileEventHandler.h"
#include "cwQMLReload.h"
#include "cwDebug.h"
#include "cwApplication.h"
#include "cwUsedStationsTask.h"

#ifndef CAVEWHERE_VERSION
#define CAVEWHERE_VERSION "Sauce-Release"
#endif

QUrl mainWindowSourcePath() {
    return QUrl::fromLocalFile(cwGlobalDirectory::baseDirectory() + cwGlobalDirectory::qmlMainFilePath());
}

int main(int argc, char *argv[])
{    
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    cwApplication a(argc, argv);

    cwRootData* rootData = new cwRootData();

    if(argc >= 2) {
        QByteArray filenameByteArray(argv[1]);
        rootData->project()->loadFile(QString::fromLocal8Bit(filenameByteArray));
    }

    //Handles when the user clicks on a file in Finder(Mac OS X) or Explorer (Windows)
    cwOpenFileEventHandler* openFileHandler = new cwOpenFileEventHandler(&a);
    openFileHandler->setProject(rootData->project());
    a.installEventFilter(openFileHandler);

    QApplication::setOrganizationName("Vadose Solutions");
    QApplication::setOrganizationDomain("cavewhere.com");
    QApplication::setApplicationName("Cavewhere");
    QApplication::setApplicationVersion("0.1");

    cwGlobalDirectory::setupBaseDirectory();

    //Register all of the cavewhere types
    cwQMLRegister::registerQML();

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setAlphaBufferSize(8);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
//    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QUrl mainWindowPath = mainWindowSourcePath();
    QQmlApplicationEngine applicationEnigine;

    rootData->qmlReloader()->setApplicationEngine(&applicationEnigine);

    auto checkFormat = []() {
        const QSurfaceFormat format = QSurfaceFormat::defaultFormat(); //dm_gl->format();
        const uint a = format.alphaBufferSize();
        const uint r = format.redBufferSize();
        const uint g = format.greenBufferSize();
        const uint b = format.blueBufferSize();

#define RGBA_BITS(r,g,b,a) (r | (g << 6) | (b << 12) | (a << 18))

        const uint bits = RGBA_BITS(r,g,b,a);
        qDebug() << "Bits:" << QString("%1").arg(bits, 0, 16) << format.alphaBufferSize() << format.redBufferSize() << format.greenBufferSize() << format.blueBufferSize();
        switch (bits) {
        case RGBA_BITS(8,8,8,8):
            qDebug() << "QAbstractTexture::RGBA8_UNorm";
            break;
        case RGBA_BITS(8,8,8,0):
            qDebug() << "QAbstractTexture::RGB8_UNorm";
            break;
        case RGBA_BITS(5,6,5,0):
            qDebug() << "QAbstractTexture::R5G6B5";
            break;
        }
#undef RGBA_BITS
    };

    checkFormat();


//    rootData->project()->load(QDir::homePath() + "/Dropbox/quanko.cw");
//    rootData->project()->load(QDir::homePath() + "/test.cw");
    QQmlContext* context =  applicationEnigine.rootContext(); //view.rootContext();

    context->setContextObject(rootData);
    context->setContextProperty("rootData", rootData);

    //This allow to extra image data from the project's image database
    cwImageProvider* imageProvider = new cwImageProvider();
    imageProvider->setProjectPath(rootData->project()->filename());
    QObject::connect(rootData->project(), SIGNAL(filenameChanged(QString)), imageProvider, SLOT(setProjectPath(QString)));
    context->engine()->addImageProvider(cwImageProvider::Name, imageProvider);

    auto quit = [&a]() {
        QThreadPool::globalInstance()->waitForDone();
        a.quit();
    };

    //Allow the engine to quit the application
    QObject::connect(context->engine(), &QQmlEngine::quit, &a, quit, Qt::QueuedConnection);

    if(!mainWindowPath.isEmpty()) {
        applicationEnigine.load(mainWindowPath);
    } else {
        QMessageBox mainWindowNotFoundMessage(QMessageBox::Critical,
                                              "Cavewhere Failed to Load Main Window",
                                              "ಠ_ರೃ Cavewhere has failed to load its main window... <br><b>This is a bug!</b>",
                                              QMessageBox::Close);
        mainWindowNotFoundMessage.exec();
        return 1;
    }

    return a.exec();
}
