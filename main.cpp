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
#include "cwRootData.h"
#include "cwProject.h"
#include "cwImageProvider.h"
#include "cwOpenFileEventHandler.h"
#include "cwApplication.h"
#include "cwGlobals.h"
#include "cwConcurrent.h"
#include "cavewhereVersion.h"

//std includes
#include <memory>

//MarkScope
#include "MarkScope/FrameProfiler.h"

//Crash pad
#include "client/crashpad_client.h"
#include "client/crash_report_database.h"
#include "client/settings.h"
#include "base/files/file_path.h"

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

void setupCrashPad() {
    // Define the upload URL (your server endpoint)
    // std::string uploadUrl; // = "https://yourserver.com/crashreport/upload";
    // std::string uploadUrl = QStringLiteral("https://submit.backtrace.io/cavewhere/0b720fe623347ec3c890432a12921cf35341564a5397b522ef16ab04b709a07c/minidump").toStdString();
    std::string uploadUrl = QStringLiteral("https://cavewhere.com/crashdump_upload.php").toStdString();

    // Optional: Provide additional arguments (e.g., metadata as --key=value)
    std::vector<std::string> arguments;
    // arguments.push_back("--log-level=debug");
    // arguments.push_back("--version=" + CavewhereVersion.toStdString());

    QString crashpadHandler = qApp->applicationDirPath() + QStringLiteral("/crashpad_handler");
#if defined(Q_OS_WIN)
    {
        crashpadHandler += QStringLiteral(".exe");
    }
#endif

    auto breakpadStr = [](const QString& str) {
#if defined(Q_OS_WIN)
        return str.toStdWString();
#else
        return str.toStdString();
#endif
    };

    // Define paths for Crashpad's database and the handler executable
    qDebug() << "Crashpad handler:" << crashpadHandler;
    base::FilePath handlerPath(breakpadStr(crashpadHandler));


    auto baseDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));

    QString crashpadDir = QStringLiteral("crashpad");
    baseDir.mkpath(crashpadDir);

    QString databasePathStr = baseDir.absoluteFilePath(crashpadDir + QStringLiteral("/crashpad_db"));
    QString metricsPathStr = baseDir.absoluteFilePath(crashpadDir + QStringLiteral("/metrics"));

    base::FilePath databasePath(breakpadStr(databasePathStr));
    base::FilePath metricsPath(breakpadStr(databasePathStr));

    qDebug() << "Path to crashpad database:" << databasePathStr;
    qDebug() << "Path to crashpad metrics:" << metricsPathStr;


    std::map<std::string, std::string> annotations; // Empty annotations
    annotations["version"] = CavewhereVersion.toStdString();
    annotations["backtrace.version"] = CavewhereVersion.toStdString();



    // Initialize Crashpad
    crashpad::CrashpadClient client;
    bool success = client.StartHandler(handlerPath,
                                       databasePath,
                                       metricsPath,
                                       uploadUrl,
                                       annotations,
                                       arguments,
                                       /* restartable */ true,
                                       /* asynchronous_start */ false);

    if (!success) {
        // If initialization fails, you might log the error or alert the user
        QMessageBox msgBox;
        msgBox.setText("Failed to initialize crash reporting.");
        msgBox.exec();
    }

    // Initialize your Crashpad database.
    std::unique_ptr<crashpad::CrashReportDatabase> database =
        crashpad::CrashReportDatabase::Initialize(databasePath);

    // Check that the database was successfully created.
    if (database) {
        auto settings = database->GetSettings();
        settings->SetUploadsEnabled(true);
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

    setupCrashPad();

    cwRootData* data = nullptr;
    qDebug() << "Crash:" << data->defaultTrip();

    //Load all the fonts
    cwGlobals::loadFonts();

    // Configure multisample antialiasing
    QSurfaceFormat format;
    format.setSamples(4); // Adjust the sample count as needed
    QSurfaceFormat::setDefaultFormat(format);

    QQmlApplicationEngine* applicationEngine = new QQmlApplicationEngine();

    // Add the macOS Resources directory to the QML import search path
    QString resourcePath = QCoreApplication::applicationDirPath() + "/../Resources/qml";
    applicationEngine->addImportPath(resourcePath);

    QQmlContext* context = applicationEngine->rootContext();

    //This allow to extra image data from the project's image database
    cwImageProvider* imageProvider = new cwImageProvider();
    context->engine()->addImageProvider(cwImageProvider::name(), imageProvider);

    applicationEngine->loadFromModule(QStringLiteral("cavewherelib"),
                                       QStringLiteral("CavewhereMainWindow"));
    auto id = qmlTypeId("cavewherelib", 1, 0, "RootData");
    cwRootData* rootData = applicationEngine->rootContext()->engine()->singletonInstance<cwRootData*>(id);

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
