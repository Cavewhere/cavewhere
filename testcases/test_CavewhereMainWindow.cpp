//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGlobalDirectory.h"
#include "cwQMLRegister.h"
#include "cwRootData.h"
#include "cwQMLReload.h"
#include "TestHelper.h"
#include "cwImageProvider.h"
#include "cwSurveyNoteModel.h"
#include "cwTaskManagerModel.h"
#include "cwFutureManagerModel.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwScrap.h"
#include "cwNote.h"
// #include "cwOpenGLSettings.h"

//Qt includes
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QDebug>
#include <QSettings>

//QuickQanave includes
#include <QuickQanava>

class MainHelper {
public:
    static QQmlApplicationEngine* createApplicationEnigne() {
        QQmlApplicationEngine* applicationEnigine = new QQmlApplicationEngine();
        cwRootData* rootData = new cwRootData(applicationEnigine);


        // Add the macOS Resources directory to the QML import search path
        QString resourcePath = QCoreApplication::applicationDirPath() + "/QuickQanava/src";
        applicationEnigine->addImportPath(resourcePath);
        qDebug() << "ResourcePath:" << resourcePath;

        QuickQanava::initialize(applicationEnigine);

        applicationEnigine->loadFromModule(QStringLiteral("cavewherelib"),
                                           QStringLiteral("CavewhereMainWindow"));
        return applicationEnigine;
    }

    static QObject* findMainWidow(QQmlApplicationEngine* engine) {
        REQUIRE(!engine->rootObjects().isEmpty());
        auto mainWindow = engine->rootObjects().first();
        REQUIRE(mainWindow->objectName().toStdString() == "applicationWindow");
        return mainWindow;
    }
};


TEST_CASE("Test that the cavewhere main window remember size and position", "[CavewhereMainWindow]") {

    {
        QSettings settings;
        settings.clear();
    }

    auto firstAppEngine = MainHelper::createApplicationEnigne();

    QEventLoop loop;
    QTimer::singleShot(2000, qApp, [firstAppEngine, &loop]() {
        REQUIRE(!firstAppEngine->rootObjects().isEmpty());
        auto mainWindow = MainHelper::findMainWidow(firstAppEngine);
        REQUIRE(mainWindow->objectName().toStdString() == "applicationWindow");

        mainWindow->setProperty("x", 75);
        mainWindow->setProperty("y", 55);

        CHECK(mainWindow->property("width").toInt() == 1024);
        CHECK(mainWindow->property("height").toInt() == 576);
        CHECK(mainWindow->property("x").toInt() == 75);

        //This seems to move around on macos
        CHECK((mainWindow->property("y").toInt() >= 55
              || mainWindow->property("y").toInt() <= 65));

        mainWindow->setProperty("width", 250);
        mainWindow->setProperty("height", 200);
        mainWindow->setProperty("x", 323);
        mainWindow->setProperty("y", 386);

        QTimer::singleShot(1000, qApp, [&loop, firstAppEngine]() {

            delete firstAppEngine;

            auto secondEngine = MainHelper::createApplicationEnigne();

            QTimer::singleShot(2000, qApp, [&loop, secondEngine]() {
                auto mainWindow2 = MainHelper::findMainWidow(secondEngine);

                CHECK(mainWindow2->property("width").toInt() == 250);
                CHECK(mainWindow2->property("height").toInt() == 200);
                CHECK(mainWindow2->property("x").toInt() == 323);
                CHECK(mainWindow2->property("y").toInt() == 386);

                delete secondEngine;

                loop.quit();
            });
        });
    });

    loop.exec();

    {
        QSettings settings;
        settings.clear();
    }
}

//This testcase is mostly here for checking memory leaks and close crashing
TEST_CASE("Main window should load file and close the window", "[CavewhereMainWindow]") {
    auto firstAppEngine = MainHelper::createApplicationEnigne();
    auto rootData = firstAppEngine->singletonInstance<cwRootData*>("cavewherelib", "RootData");
    REQUIRE(rootData);

    auto filename = copyToTempFolder("://datasets/scrapGuessNeighbor/scrapGuessNeigborPlanContinuous.cw");


    QEventLoop loop;
    QTimer::singleShot(2000, qApp, [rootData, filename, firstAppEngine, &loop]() {
        rootData->project()->loadFile(filename);

        QTimer::singleShot(2000, qApp, [firstAppEngine, &loop](){
            delete firstAppEngine;
            loop.quit();
        });
    });

    loop.exec();
    CHECK(true);
}

TEST_CASE("Load project with no images for scraps", "[CavewhereMainWindow]") {

    auto firstAppEngine = MainHelper::createApplicationEnigne();
    auto rootData = firstAppEngine->singletonInstance<cwRootData*>("cavewherelib", "RootData");
    REQUIRE(rootData);

    auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

    SECTION("Make the file read-only") {
        QFile file(filename);
        CHECK(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::ReadGroup | QFileDevice::ReadUser));

        QFileInfo info(filename);
        CHECK(info.isReadable());
        CHECK(info.isWritable() == false);
    }

    SECTION("Normal / read write mode") {
        //This section is important because it allows catch to run the file when it has write
        //permissions
        QFileInfo info(filename);
        CHECK(info.isReadable());
        CHECK(info.isWritable());
        CHECK(true);
    }

    QEventLoop loop;
    QTimer::singleShot(2000, qApp, [rootData, filename, firstAppEngine, &loop]() {

        auto project = rootData->project();
        project->loadFile(filename);
        project->waitLoadToFinish();

        INFO("Filename:" << project->filename());

        rootData->taskManagerModel()->waitForTasks();
        rootData->futureManagerModel()->waitForFinished();

        REQUIRE(project->cavingRegion()->caveCount() == 1);
        auto cave = project->cavingRegion()->cave(0);

        REQUIRE(cave->tripCount() == 1);
        auto trip = cave->trip(0);

        REQUIRE(trip->notes()->notes().size() == 1);
        auto note = trip->notes()->notes().at(0);

        cwImageProvider imageProvider;
        imageProvider.setProjectPath(project->filename());

        CHECK(note->scraps().size() == 2);

        for(const auto& scrap : note->scraps()) {
            auto triangleData = scrap->triangulationData();

            QList<int> ids = {
                triangleData.croppedImage().original(),
            };

            CHECK(!triangleData.croppedImage().isIconValid());
            CHECK((triangleData.croppedImage().isOriginalValid()));


            for(auto id : ids) {
                auto data = imageProvider.data(id, true);
                INFO("Id:" << id << " isValid:" << data.size().isValid());
                CHECK((data.size().isValid()));
            }
        }

        QTimer::singleShot(1000, qApp, [&loop, firstAppEngine]() {
            delete firstAppEngine;
            loop.quit();
        });
    });

    loop.exec();
    CHECK(true);

    QFile file(filename);
    CHECK(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadUser));
}

