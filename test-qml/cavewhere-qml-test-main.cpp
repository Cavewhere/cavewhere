#include <QtQuickTest/quicktest.h>

//Qt includes
#include <QtQuickTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QGuiApplication>
#include <QApplication>
#include <QFont>
#include <QQuickStyle>
#include <QStyleFactory>

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
        QApplication::setApplicationName(QStringLiteral("cavewhere-qml-test-%1").arg(QApplication::applicationPid()));
        // QApplication::setApplicationVersion(CAVEWHERE_VERSION);

        //Clear all the settings
        QSettings settings;
        settings.clear();

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
    //This allows use to create a QGraphicsScene, without QApplication, this crashes
    QApplication app(argc, argv);

    // Match the application's style stack (main.cpp) so tests exercise the same
    // style as production. Fusion is a pure-QML style with no native theme calls,
    // so it also sidesteps the Windows native-style offscreen crash that previously
    // required forcing the Basic style here.
    QApplication::setStyle(QStyleFactory::create("Fusion")); // Qt Widgets
    QQuickStyle::setStyle("Fusion");                          // Qt Quick Controls

    cwMetaTypeSystem::registerTypes();

    // Wire SURVEXLIB and PROJ_DATA search paths so tests that touch coordinate
    // transforms (e.g. cwTripCalibration::autoDeclinationAvailable) work
    // without relying on the dev shell's conanrun environment.
    cwRootData::initCavewherelib();

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
