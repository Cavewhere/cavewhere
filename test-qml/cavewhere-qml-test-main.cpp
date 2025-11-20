#include <QtQuickTest/quicktest.h>

//Qt includes
#include <QtQuickTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QGuiApplication>
#include <QApplication>

//Our inculdes
#include "cwImageProvider.h"
#include "cwCacheImageProvider.h"
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

        //This allow to extra image data from the project's image database
        cwImageProvider* imageProvider = new cwImageProvider();
        engine->addImageProvider(cwImageProvider::name(), imageProvider);

        cwCacheImageProvider* cacheImageProvider = new cwCacheImageProvider();
        engine->addImageProvider(cwCacheImageProvider::name(), cacheImageProvider);

        //Init libgit2
        QQuickGit::GitRepository::initGitEngine();

        auto id = qmlTypeId("cavewherelib", 1, 0, "RootData");
        cwRootData* rootData = engine->rootContext()->engine()->singletonInstance<cwRootData*>(id);

        if(rootData) {
            //Hookup the image provider now that the rootdata is create
            const QString filename = rootData->project()->filename();
            imageProvider->setProjectPath(filename);
            QObject::connect(rootData->project(), SIGNAL(filenameChanged(QString)), imageProvider, SLOT(setProjectPath(QString)));

            cacheImageProvider->setProjectDirectory(QFileInfo(filename).absoluteDir());
            QObject::connect(rootData->project(), &cwProject::filenameChanged, cacheImageProvider, [cacheImageProvider](const QString& newFilename) {
                cacheImageProvider->setProjectDirectory(QFileInfo(newFilename).absoluteDir());
            });
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

    cwMetaTypeSystem::registerTypes();

    cwGlobals::loadFonts();

    QSurfaceFormat format;
    format.setSamples(4); // Adjust the sample count as needed
    QSurfaceFormat::setDefaultFormat(format);

    QTEST_SET_MAIN_SOURCE_PATH
    Setup setup;
    const char* qmlSourceDirectory = QUICK_TEST_SOURCE_DIR; //The source directory where the qml tests are located
    qDebug() << "qmlSourceDirectory:" << qmlSourceDirectory;
    return quick_test_main_with_setup(argc, argv, "CaveWhere-qml-tests", qmlSourceDirectory, &setup); \
}



#include "cavewhere-qml-test-main.moc"
#include <QFileInfo>
