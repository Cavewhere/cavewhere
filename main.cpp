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

void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    switch (type) {
    case QtWarningMsg:
        qDebug() << "QWarning triggered: " << msg;
        if (msg.contains("No module named \"cavewherelib\" found")) {
            // Breakpoint here for the debugger
            qDebug() << "Breaking on QWarning...";
#ifdef Q_OS_WIN
            __debugbreak(); // Windows
#else
            __builtin_trap(); // macOS/Linux
#endif
        }
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        // Handle other types of messages if necessary
        break;
    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    //This needs to be first for QSettings
    QApplication::setOrganizationName("Vadose Solutions");
    QApplication::setOrganizationDomain("cavewhere.com");
    QApplication::setApplicationName("CaveWhere");
    QApplication::setApplicationVersion(CAVEWHERE_VERSION);

    // cwOpenGLSettings::setApplicationRenderer();
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    cwApplication a(argc, argv);

    // Install the custom message handler
    // qInstallMessageHandler(customMessageHandler);

    // Set the default graphics API to OpenGL
    // QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    cwRootData* rootData = new cwRootData();

    if(argc >= 2) {
        QByteArray filenameByteArray(argv[1]);
        qDebug() << "Loading file:" << filenameByteArray;
        rootData->project()->loadFile(QString::fromLocal8Bit(filenameByteArray));
    }

    //Handles when the user clicks on a file in Finder(Mac OS X) or Explorer (Windows)
    cwOpenFileEventHandler* openFileHandler = new cwOpenFileEventHandler(&a);
    openFileHandler->setProject(rootData->project());
    a.installEventFilter(openFileHandler);

    //TODO: Add rendering dialog, for checking bad text
    //Fixes text rendering issues on windows for bad graphics cards
    //https://stackoverflow.com/questions/29733711/blurred-qt-quick-text
    // QQuickWindow::setTextRenderType(cwOpenGLSettings::instance()->useNativeTextRendering() ? QQuickWindow::NativeTextRendering : QQuickWindow::QtTextRendering);

    // cwGlobalDirectory::setupBaseDirectory();

    //Register all of the cavewhere types
    cwQMLRegister::registerQML();

    QQmlApplicationEngine* applicationEnigine = new QQmlApplicationEngine();

    QQmlContext* context = applicationEnigine->rootContext();

    //This allow to extra image data from the project's image database
    cwImageProvider* imageProvider = new cwImageProvider();
    imageProvider->setProjectPath(rootData->project()->filename());
    QObject::connect(rootData->project(), SIGNAL(filenameChanged(QString)), imageProvider, SLOT(setProjectPath(QString)));
    context->engine()->addImageProvider(cwImageProvider::name(), imageProvider);

    auto quit = [&a, rootData, applicationEnigine]() {
        delete applicationEnigine;
        delete rootData;
        QThreadPool::globalInstance()->waitForDone();
        cwTask::threadPool()->waitForDone();
        a.quit();
    };

    //Allow the engine to quit the application
    QObject::connect(context->engine(), &QQmlEngine::quit, &a, quit, Qt::QueuedConnection);

    applicationEnigine->loadFromModule(QStringLiteral("cavewherelib"),
                                       QStringLiteral("CavewhereMainWindow"));

    return a.exec();
}
