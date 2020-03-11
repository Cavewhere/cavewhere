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
#include <QThreadPool>
#include <QQuickWindow>

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
#include "cwOpenGLSettings.h"

#ifndef CAVEWHERE_VERSION
#define CAVEWHERE_VERSION "Sauce-Release"
#endif

int main(int argc, char *argv[])
{    
    //This needs to be first for QSettings
    QApplication::setOrganizationName("Vadose Solutions");
    QApplication::setOrganizationDomain("cavewhere.com");
    QApplication::setApplicationName("CaveWhere");
    QApplication::setApplicationVersion(CAVEWHERE_VERSION);

    cwOpenGLSettings::setApplicationRenderer();
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

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

    //TODO: Add rendering dialog, for checking bad text
    //Fixes text rendering issues on windows for bad graphics cards
    //https://stackoverflow.com/questions/29733711/blurred-qt-quick-text
    QQuickWindow::setTextRenderType(cwOpenGLSettings::instance()->useNativeTextRendering() ? QQuickWindow::NativeTextRendering : QQuickWindow::QtTextRendering);

    cwGlobalDirectory::setupBaseDirectory();

    //Register all of the cavewhere types
    cwQMLRegister::registerQML();

    QUrl mainWindowPath = cwGlobalDirectory::mainWindowSourcePath();
    QQmlApplicationEngine* applicationEnigine = new QQmlApplicationEngine();

    rootData->qmlReloader()->setApplicationEngine(applicationEnigine);

    QQmlContext* context = applicationEnigine->rootContext();

    context->setContextObject(rootData);
    context->setContextProperty("rootData", rootData);

    //This allow to extra image data from the project's image database
    cwImageProvider* imageProvider = new cwImageProvider();
    imageProvider->setProjectPath(rootData->project()->filename());
    QObject::connect(rootData->project(), SIGNAL(filenameChanged(QString)), imageProvider, SLOT(setProjectPath(QString)));
    context->engine()->addImageProvider(cwImageProvider::Name, imageProvider);

    auto quit = [&a, rootData, applicationEnigine]() {
        delete applicationEnigine;
        delete rootData;
        QThreadPool::globalInstance()->waitForDone();
        a.quit();
    };

    //Allow the engine to quit the application
    QObject::connect(context->engine(), &QQmlEngine::quit, &a, quit, Qt::QueuedConnection);

    if(!mainWindowPath.isEmpty()) {
        applicationEnigine->load(mainWindowPath);
    } else {
        QMessageBox mainWindowNotFoundMessage(QMessageBox::Critical,
                                              "CaveWhere Failed to Load Main Window",
                                              "ಠ_ರೃ CaveWhere has failed to load its main window... <br><b>This is a bug!</b>",
                                              QMessageBox::Close);
        mainWindowNotFoundMessage.exec();
        return 1;
    }

    return a.exec();
}
