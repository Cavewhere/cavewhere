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

//std includes
#include <memory>

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

void handleCommandline(QCoreApplication& a, cwRootData* rootData) {
    // Command-line argument parser
    QCommandLineParser parser;
    parser.setApplicationDescription("CaveWhere Application");
    parser.addHelpOption();

    // Adding --page option
    QCommandLineOption pageOption(QStringList({"p", "page"}),
                                  "Specify the page URL to open.",
                                  "pageurl");
    parser.addOption(pageOption);

    // Adding optional filename argument
    parser.addPositionalArgument("filename", "The optional file to open.");

    // Parse the command-line arguments
    parser.process(a);

    // Check if --page was provided
    QString pageUrl;
    if (parser.isSet(pageOption)) {
        pageUrl = parser.value(pageOption);
    }

    const QStringList positionalArgs = parser.positionalArguments();
    if (!positionalArgs.isEmpty()) {
        QString filename = positionalArgs.first();
        qDebug() << "Loading file:" << filename;
        rootData->project()->loadFile(filename);

        if(!pageUrl.isEmpty()) {
            QObject* obj = new QObject();

            struct ShouldLoad {
                bool isPageViewLoaded = false;
                bool isFileLoaded = false;
            };

            auto shouldLoad = std::make_shared<ShouldLoad>();

            auto loadCommandLinePage = [obj, rootData, pageUrl, shouldLoad]() {
                if(shouldLoad->isPageViewLoaded && shouldLoad->isFileLoaded) {

                    //This is pretty unrealiable, it depends on the loading spead
                    QTimer::singleShot(500, [rootData, obj, pageUrl]() {
                        rootData->pageSelectionModel()->setCurrentPageAddress(pageUrl);

                        //This delete disconnects the connection
                        obj->deleteLater();
                    });
                }
            };

            obj->connect(rootData, &cwRootData::pageViewChanged, obj, [obj, shouldLoad, loadCommandLinePage, rootData]() {
                shouldLoad->isPageViewLoaded = true;
                loadCommandLinePage();
            });


            obj->connect(rootData->project(), &cwProject::loaded, obj, [shouldLoad, loadCommandLinePage]() {
                shouldLoad->isFileLoaded = true;
                loadCommandLinePage();
            });
        }
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

    //Register all of the cavewhere types
    cwQMLRegister::registerQML();

    QQmlApplicationEngine* applicationEnigine = new QQmlApplicationEngine();

    QQmlContext* context = applicationEnigine->rootContext();

    //This allow to extra image data from the project's image database
    cwImageProvider* imageProvider = new cwImageProvider();
    context->engine()->addImageProvider(cwImageProvider::name(), imageProvider);

    applicationEnigine->loadFromModule(QStringLiteral("cavewherelib"),
                                       QStringLiteral("CavewhereMainWindow"));
    auto id = qmlTypeId("cavewherelib", 1, 0, "RootData");
    cwRootData* rootData = applicationEnigine->rootContext()->engine()->singletonInstance<cwRootData*>(id);

    //Handles when the user clicks on a file in Finder(Mac OS X) or Explorer (Windows)
    cwOpenFileEventHandler* openFileHandler = new cwOpenFileEventHandler(&a);
    openFileHandler->setProject(rootData->project());
    a.installEventFilter(openFileHandler);

    //Handle command line args
    handleCommandline(a, rootData);

    //Hookup the image provider now that the rootdata is create
    imageProvider->setProjectPath(rootData->project()->filename());
    QObject::connect(rootData->project(), SIGNAL(filenameChanged(QString)), imageProvider, SLOT(setProjectPath(QString)));

    auto quit = [&a, rootData, applicationEnigine]() {
        delete applicationEnigine;
        delete rootData;
        QThreadPool::globalInstance()->waitForDone();
        cwTask::threadPool()->waitForDone();
        a.quit();
    };

    //Allow the engine to quit the application
    QObject::connect(context->engine(), &QQmlEngine::quit, &a, quit, Qt::QueuedConnection);

    return a.exec();
}
