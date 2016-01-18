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
    cwApplication a(argc, argv);
    a.setAttribute(Qt::AA_UseDesktopOpenGL);

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

    QUrl mainWindowPath = mainWindowSourcePath();
    QQmlApplicationEngine applicationEnigine;

    rootData->qmlReloader()->setApplicationEngine(&applicationEnigine);

//    applicationEnigine.rootContext();

//    QQuickView view;
//    view.setTitle(QString("Cavewhere - %1").arg(CAVEWHERE_VERSION));

//    QSurfaceFormat format = view.format();
//    format.setSamples(4);

     //&view);

//    rootData->setQuickView(&view);
//    rootData->project()->load(QDir::homePath() + "/Dropbox/quanko.cw");
//    rootData->project()->load(QDir::homePath() + "/test.cw");
    QQmlContext* context =  applicationEnigine.rootContext(); //view.rootContext();

    context->setContextObject(rootData);
    context->setContextProperty("rootData", rootData);
    //    context->setContextProperty("mainWindow", &view);

    //This allow to extra image data from the project's image database
    cwImageProvider* imageProvider = new cwImageProvider();
    imageProvider->setProjectPath(rootData->project()->filename());
    QObject::connect(rootData->project(), SIGNAL(filenameChanged(QString)), imageProvider, SLOT(setProjectPath(QString)));
    context->engine()->addImageProvider(cwImageProvider::Name, imageProvider);

    //Allow the engine to quit the application
    QObject::connect(context->engine(), SIGNAL(quit()), &a, SLOT(quit()));

//    QUrl mainWindowPath = mainWindowSourcePath();

    if(!mainWindowPath.isEmpty()) {
        applicationEnigine.load(mainWindowPath);
        //        view.setFormat(format);
//        view.setResizeMode(QQuickView::SizeRootObjectToView);
//        view.setSource(mainWindowPath);
//        view.show();
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
