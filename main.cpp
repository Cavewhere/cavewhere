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
#include <QCommandLineParser>

//Our includes
//#include "cwMainWindow.h"
#include "cwImage.h"
#include "cwRootData.h"
#include "cwProject.h"
#include "cwImageProvider.h"
#include "cwOpenFileEventHandler.h"
#include "cwApplication.h"
#include "cwGlobals.h"
#include "cwMetaTypeSystem.h"

//QQuickGit
#include "GitRepository.h"

//QuickQanave includes
#include <QuickQanava>

//std includes
#include <memory>

//MarkScope
#include "MarkScope/FrameProfiler.h"

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
                if(shouldLoad->isFileLoaded) {

                    //This is pretty unrealiable, it depends on the loading spead
                    QTimer::singleShot(250, [rootData, obj, pageUrl]() {
                        rootData->pageSelectionModel()->setCurrentPageAddress(pageUrl);

                        //This delete disconnects the connection
                        obj->deleteLater();
                    });
                }
            };

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

    cwApplication a(argc, argv);

    //Register meta system
    cwMetaTypeSystem::registerTypes();

    //Load all the fonts
    cwGlobals::loadFonts();

    QSettings settings;
    settings.clear();

    // Configure multisample antialiasing
    QSurfaceFormat format;
    format.setSamples(4); // Adjust the sample count as needed
    QSurfaceFormat::setDefaultFormat(format);

    //initilize git
    QQuickGit::GitRepository::initGitEngine();

    QQmlApplicationEngine* applicationEngine = new QQmlApplicationEngine();

    // Add the macOS Resources directory to the QML import search path
    QString resourcePath = QCoreApplication::applicationDirPath() + "/../Resources/qml";
    applicationEngine->addImportPath(resourcePath);
    applicationEngine->addImportPath(":/"); //This enable QuickQanava to load in qml correctly

    //Initilize QuickQanava
    // Q_IMPORT_QML_PLUGIN(QuickQanava)
    QuickQanava::initialize(applicationEngine);

    QQmlContext* context = applicationEngine->rootContext();

    //This allow to extra image data from the project's image database
    cwImageProvider* imageProvider = new cwImageProvider();
    context->engine()->addImageProvider(cwImageProvider::name(), imageProvider);

    applicationEngine->loadFromModule(QStringLiteral("cavewherelib"),
                                       QStringLiteral("CavewhereMainWindow"));
    auto id = qmlTypeId("cavewherelib", 1, 0, "RootData");
    cwRootData* rootData = applicationEngine->rootContext()->engine()->singletonInstance<cwRootData*>(id);

    qDebug() << "CaveWhere built for" << (rootData->desktopBuild() ? "desktop" : "mobile");

    MarkScope::FrameProfiler frameProfiler(applicationEngine);

    //Handle command line args
    handleCommandline(a, rootData);

    //Handles when the user clicks on a file in Finder(Mac OS X) or Explorer (Windows)
    cwOpenFileEventHandler* openFileHandler = new cwOpenFileEventHandler(&a);
    openFileHandler->setProject(rootData->project());
    a.installEventFilter(openFileHandler);

    //Hookup the image provider now that the rootdata is create
    imageProvider->setProjectPath(rootData->project()->filename());
    QObject::connect(rootData->project(), SIGNAL(filenameChanged(QString)), imageProvider, SLOT(setProjectPath(QString)));

    auto quit = [&a, rootData, applicationEngine]() {
        delete applicationEngine;
        QThreadPool::globalInstance()->waitForDone();
        cwTask::threadPool()->waitForDone();
        a.quit();
    };

    //Allow the engine to quit the application
    QObject::connect(context->engine(), &QQmlEngine::quit, &a, quit, Qt::QueuedConnection);

    return a.exec();
}
