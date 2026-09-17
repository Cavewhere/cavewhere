/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QGuiApplication>
#include <QApplication>
#include <QFont>
#include <QQmlContext>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QThreadPool>
#include <QQuickWindow>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QtQml/qqml.h>
#include <qpa/qplatformwindow.h>
#include <QQuickStyle>
#include <QStyleFactory>

//Our includes
//#include "cwMainWindow.h"
#include "cwImage.h"
#include "cwRootData.h"
#include "cwProject.h"
#include "cwQmlImageProviderBinder.h"
#include "cwOpenFileEventHandler.h"
#include "cwDeepLinkHandler.h"
#include "cwApplication.h"
#include "cwGlobals.h"
#include "cwMetaTypeSystem.h"
#include "cwTask.h"
#include "cwSettings.h"
#include "cwFontSettings.h"
#include "cwProfileLog.h"
#include "cwRenderingSettings.h"

//std includes
#include <memory>

//QQuickGit includes
#include "GitConcurrent.h"
#include "GitCommitImageProvider.h"
#include "GitRepository.h"

//MarkScope
#include "MarkScope/FrameProfiler.h"

#ifndef CAVEWHERE_VERSION
#define CAVEWHERE_VERSION "Sauce-Release"
#endif

// Installed by --profile-log, which needs the cw.profile.* lines on stderr: a
// bundled app's stderr is a pipe, and QT_FORCE_STDERR_LOGGING alone does not
// reach it. Writing straight to the stream rather than through qDebug() keeps a
// message from re-entering the handler that is printing it.
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    const QByteArray line = qFormatLogMessage(type, context, msg).toLocal8Bit();
    fputs(line.constData(), stderr);
    fputc('\n', stderr);
    fflush(stderr);

    if (type == QtWarningMsg && msg.contains("No module named \"cavewherelib\" found")) {
        // Breakpoint here for the debugger
#ifdef Q_OS_WIN
        __debugbreak(); // Windows
#else
        __builtin_trap(); // macOS/Linux
#endif
    }
}

namespace {

// The point cloud profiling harness (see the plan's §2.1). Every
// cwRenderingSettings setter persists through QSettings, so an override lives
// for this process only: the stored values are read here and written back when
// the app quits.
void applyProfileOverrides(QCoreApplication& app, const QCommandLineParser& parser,
                           const QCommandLineOption& gpuBudgetMbOption,
                           const QCommandLineOption& screenSpaceErrorPxOption,
                           const QCommandLineOption& pointBudgetMillionsOption)
{
    if (!parser.isSet(gpuBudgetMbOption) && !parser.isSet(screenSpaceErrorPxOption)
        && !parser.isSet(pointBudgetMillionsOption)) {
        return;
    }

    auto* settings = cwRenderingSettings::instance();
    const int storedGpuBudgetMb = settings->gpuMemoryBudgetMb();
    const double storedScreenSpaceErrorPx = settings->screenSpaceErrorPx();
    const int storedPointBudgetMillions = settings->pointBudgetMillions();

    QObject::connect(&app, &QCoreApplication::aboutToQuit, settings,
                     [settings, storedGpuBudgetMb, storedScreenSpaceErrorPx,
                      storedPointBudgetMillions]() {
                         settings->setGpuMemoryBudgetMb(storedGpuBudgetMb);
                         settings->setScreenSpaceErrorPx(storedScreenSpaceErrorPx);
                         settings->setPointBudgetMillions(storedPointBudgetMillions);
                     });

    if (parser.isSet(gpuBudgetMbOption)) {
        settings->setGpuMemoryBudgetMb(parser.value(gpuBudgetMbOption).toInt());
    }
    if (parser.isSet(screenSpaceErrorPxOption)) {
        settings->setScreenSpaceErrorPx(parser.value(screenSpaceErrorPxOption).toDouble());
    }
    if (parser.isSet(pointBudgetMillionsOption)) {
        settings->setPointBudgetMillions(parser.value(pointBudgetMillionsOption).toInt());
    }
}

void enableProfileLogging()
{
    qInstallMessageHandler(customMessageHandler);
    QLoggingCategory::setFilterRules(QStringLiteral("cw.profile.render.debug=true\n"
                                                    "cw.profile.pick.debug=true\n"
                                                    "cw.profile.load.debug=true"));
}

} // namespace

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

    // The point cloud profiling harness. Kept in one block so it stays apart
    // from the options around it.
    QCommandLineOption profileGpuBudgetOption("profile-gpu-budget-mb",
                                              "Override the GPU memory budget for this run only.",
                                              "megabytes");
    QCommandLineOption profileSseOption("profile-sse-px",
                                        "Override the screen space error for this run only.",
                                        "pixels");
    QCommandLineOption profilePointBudgetOption("profile-point-budget-millions",
                                                "Override the points drawn per frame for this run only.",
                                                "millions");
    QCommandLineOption profileLogOption("profile-log",
                                        "Write the cw.profile.* lines to stderr.");
    parser.addOption(profileGpuBudgetOption);
    parser.addOption(profileSseOption);
    parser.addOption(profilePointBudgetOption);
    parser.addOption(profileLogOption);

    // Adding optional filename argument
    parser.addPositionalArgument("filename", "The optional file to open.");

    // Parse the command-line arguments
    parser.process(a);

    if (parser.isSet(profileLogOption)) {
        enableProfileLogging();
    }

    // Before the project loads, so the first frame it draws already renders
    // under the overrides.
    applyProfileOverrides(a, parser, profileGpuBudgetOption, profileSseOption,
                          profilePointBudgetOption);

    // Check if --page was provided
    QString pageUrl;
    if (parser.isSet(pageOption)) {
        pageUrl = parser.value(pageOption);
    }

    const QStringList positionalArgs = parser.positionalArguments();
    if (!positionalArgs.isEmpty()) {
        QString filename = positionalArgs.first();

        const bool isDeepLink = filename.startsWith(QLatin1String("cavewhere://"));
        if (isDeepLink) {
            rootData->deepLinkHandler()->handleUrl(QUrl(filename));
        } else {
            rootData->project()->loadOrConvert(filename);
        }

        if(!pageUrl.isEmpty() && !isDeepLink) {
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
#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME")) {
        qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    }
#endif

    // Fusion is the only built-in style that supports dark palettes on Windows and
    // Linux — the native Windows Vista style always ignores the dark palette (Qt 6.5
    // blog). macOS handles dark mode natively too, but its style is a native-drawn
    // one whose controls refuse customization: replacing a part (a GroupBox's label,
    // say) is unsupported and silently mispositions it, so a control that reads
    // correctly everywhere else breaks on macOS alone. One style across the desktops
    // keeps the layout answers the same on each of them, and matches what the QML
    // tests run.
    //
    // The phones keep their platform default. Fusion is drawn on desktop metrics —
    // touch targets sized for a mouse — and this same main.cpp builds the Android and
    // iOS targets, where the style module isn't linked in to resolve anyway.
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    QApplication::setStyle(QStyleFactory::create("Fusion")); // Qt Widgets
    QQuickStyle::setStyle("Fusion");                          // Qt Quick Controls
#endif

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

    // Default QFont() resolves to .AppleSystemUIFont on macOS, which SVG
    // viewers cannot render. Align C++ rendering with the user's configured
    // font family (QML UI already follows Theme.fontFamily).
    cwSettings::initialize();
    auto syncAppFont = []() {
        const QString configured = cwFontSettings::instance()->fontFamily();
        const QString family = configured.isEmpty()
            ? cwFontSettings::fontEntries().first().family
            : configured;
        QFont f = QApplication::font();
        f.setFamily(family);
        QApplication::setFont(f);
    };
    syncAppFont();
    QObject::connect(cwFontSettings::instance(), &cwFontSettings::fontFamilyChanged,
                     qApp, syncAppFont);

    //Clear the settings for testing
    QSettings settings;
    // settings.clear();

    // Configure multisample antialiasing for the window swapchain (2D Quick UI).
    // The 3D view's MSAA is separate: it is the QQuickRhiItem's own sample count,
    // driven by cwRenderingSettings (see cw3dRegionViewer).
    QSurfaceFormat format;
    format.setSamples(4); // Adjust the sample count as needed
    QSurfaceFormat::setDefaultFormat(format);

    QQmlApplicationEngine* applicationEngine = new QQmlApplicationEngine();

    //initilize cavewher lib, gitlib2
    cwRootData::initCavewherelib();

    //Use a single shared thread pool to avoid over-subscribing CPU cores
    QQuickGit::GitConcurrent::setThreadPool(cwTask::threadPool());

    // Add the macOS Resources directory to the QML import search path
    QString resourcePath = QCoreApplication::applicationDirPath() + "/../Resources/qml";
    applicationEngine->addImportPath(resourcePath);

    QQmlContext* context = applicationEngine->rootContext();

    applicationEngine->loadFromModule(QStringLiteral("cavewherelib"),
                                       QStringLiteral("CavewhereMainWindow"));
    auto id = qmlTypeId("cavewherelib", 1, 0, "RootData");
    cwRootData* rootData = applicationEngine->rootContext()->engine()->singletonInstance<cwRootData*>(id);

    qDebug() << "CaveWhere built for" << (rootData->desktopBuild() ? "desktop" : "mobile");

    MarkScope::FrameProfiler frameProfiler(applicationEngine);

    //Handle command line args
    handleCommandline(a, rootData);

    //QPlatformWindow is needed because QQuickWindow has no public windowModified API
    if (!applicationEngine->rootObjects().isEmpty()) {
        auto* mainWindow = qobject_cast<QQuickWindow*>(applicationEngine->rootObjects().first());
        if (mainWindow) {
            auto updateModified = [mainWindow, rootData]() {
                if (auto* platformWindow = mainWindow->handle()) {
                    platformWindow->setWindowModified(rootData->project()->modified());
                }
            };
            QObject::connect(rootData->project(), &cwProject::modifiedChanged,
                             mainWindow, updateModified);
        }
    }

    //Handles when the user clicks on a file in Finder(Mac OS X) or Explorer (Windows)
    cwOpenFileEventHandler* openFileHandler = new cwOpenFileEventHandler(&a);
    openFileHandler->setProject(rootData->project());
    QObject::connect(openFileHandler, &cwOpenFileEventHandler::deepLinkReceived,
                     rootData->deepLinkHandler(), &cwDeepLinkHandler::handleUrl);
    a.installEventFilter(openFileHandler);

    //Creates image providers for the qml engine
    new cwQmlImageProviderBinder(context->engine(), rootData, applicationEngine);

    //Enables image://gitcommit/ URLs in QML for viewing images at any commit
    QQuickGit::GitCommitImageProvider::registerOn(context->engine());

    bool quitCalled = false;
    auto quit = [&a, &quitCalled, applicationEngine]() {
        if (quitCalled) { return; }
        quitCalled = true;
        delete applicationEngine;
        QThreadPool::globalInstance()->waitForDone();
        cwTask::threadPool()->waitForDone();
        a.quit();
    };

    //Allow the engine to quit the application
    QObject::connect(context->engine(), &QQmlEngine::quit, &a, quit, Qt::QueuedConnection);

    int result = a.exec();
    QQuickGit::GitRepository::shutdownGitEngine();
    return result;
}
