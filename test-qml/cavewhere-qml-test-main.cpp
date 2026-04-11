#include <QtQuickTest/quicktest.h>

//Qt includes
#include <QtQuickTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QGuiApplication>
#include <QApplication>
#include <QFont>

//Our inculdes
#include "cwQmlImageProviderBinder.h"
#include "cwFontSettings.h"
#include "cwRootData.h"
#include "cwGlobals.h"
#include "cwMetaTypeSystem.h"

//QuickGit includes
#include "GitRepository.h"

class Setup : public QObject
{
    Q_OBJECT

public:
    Setup() {}

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        //Init custom handlers here,
        engine->addImportPath(":/"); //For Qan

        //This needs to be first for QSettings
        QApplication::setOrganizationName("Vadose Solutions");
        QApplication::setOrganizationDomain("cavewhere.com");
        QApplication::setApplicationName("cavewhere-qml-test");
        // QApplication::setApplicationVersion(CAVEWHERE_VERSION);

        //Clear all the settings
        QSettings settings;
        settings.clear();

        //Init libgit2
        QQuickGit::GitRepository::initGitEngine();

        auto id = qmlTypeId("cavewherelib", 1, 0, "RootData");
        cwRootData* rootData = engine->rootContext()->engine()->singletonInstance<cwRootData*>(id);

        if(rootData) {
            new cwQmlImageProviderBinder(engine, rootData, engine);
        } else {
            qFatal("RootData didn't load correctly, check qml import path / build setup");
        }
    }
};

//This is the replacement for, so we can set EnableHighDpiScaling
//QUICK_TEST_MAIN_WITH_SETUP(mapWhere-qml-tests, Setup)
int main(int argc, char **argv) \
{
#ifdef Q_OS_WIN
    // Force Basic style when running offscreen to avoid Windows native style
    // crashes (OpenThemeData fails → unit_width assertion in qquickwindowsstyle.cpp)
    for (int i = 1; i < argc - 1; ++i) {
        if ((qstrcmp(argv[i], "-platform") == 0 || qstrcmp(argv[i], "--platform") == 0)
            && qstrcmp(argv[i + 1], "offscreen") == 0) {
            qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
            break;
        }
    }
#endif

    //This allows use to create a QGraphicsScene, without QApplication, this crashes
    QApplication app(argc, argv);

    cwMetaTypeSystem::registerTypes();

    cwGlobals::loadFonts();

    // Set the default application font to match CavewhereMainWindow.qml
    // so test layout is consistent across platforms regardless of system fonts.
    const auto& entry = cwFontSettings::fontEntries().first();
    QFont defaultFont(entry.family);
    defaultFont.setPixelSize(entry.defaultSize);
    QApplication::setFont(defaultFont);

    QSurfaceFormat format;
    format.setSamples(4); // Adjust the sample count as needed
    QSurfaceFormat::setDefaultFormat(format);

    QTEST_SET_MAIN_SOURCE_PATH
    Setup setup;
    const char* qmlSourceDirectory = QUICK_TEST_SOURCE_DIR; //The source directory where the qml tests are located
    qDebug() << "qmlSourceDirectory:" << qmlSourceDirectory;
    int result = quick_test_main_with_setup(argc, argv, "CaveWhere-qml-tests", qmlSourceDirectory, &setup);
    QQuickGit::GitRepository::shutdownGitEngine();
    return result;
}



#include "cavewhere-qml-test-main.moc"
#include <QFileInfo>
