//Catch includes
#include <catch.hpp>

//Our includes
#include "cwGlobalDirectory.h"
#include "cwQMLRegister.h"
#include "cwRootData.h"
#include "cwQMLReload.h"

//Qt includes
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QDebug>
#include <QSettings>

TEST_CASE("Test that the cavewhere main window remember size and position", "[CavewhereMainWindow]") {

    cwGlobalDirectory::setupBaseDirectory();

    //Register all of the cavewhere types
    cwQMLRegister::registerQML();


    auto createApplicationEnigne = []() {
        cwRootData* rootData = new cwRootData();

        QQmlApplicationEngine* applicationEnigine = new QQmlApplicationEngine();

        rootData->qmlReloader()->setApplicationEngine(applicationEnigine);

        QQmlContext* context =  applicationEnigine->rootContext();

        context->setContextObject(rootData);
        context->setContextProperty("rootData", rootData);

        applicationEnigine->load(cwGlobalDirectory::mainWindowSourcePath());
        return applicationEnigine;
    };

    auto findMainWindow = [](QQmlApplicationEngine* engine) {
        REQUIRE(!engine->rootObjects().isEmpty());
        auto mainWindow = engine->rootObjects().first();
        REQUIRE(mainWindow->objectName().toStdString() == "applicationWindow");
        return mainWindow;
    };

    {
        QSettings settings;
        settings.clear();
    }

    auto firstAppEngine = createApplicationEnigne();

    QEventLoop loop;
    QTimer::singleShot(2000, [firstAppEngine, &loop, createApplicationEnigne, findMainWindow]() {
        REQUIRE(!firstAppEngine->rootObjects().isEmpty());
        auto mainWindow = findMainWindow(firstAppEngine);
        REQUIRE(mainWindow->objectName().toStdString() == "applicationWindow");

        mainWindow->setProperty("x", 0);
        mainWindow->setProperty("y", 20);

        CHECK(mainWindow->property("width").toInt() == 1024);
        CHECK(mainWindow->property("height").toInt() == 576);
        CHECK(mainWindow->property("x").toInt() == 0);
        CHECK(mainWindow->property("y").toInt() == 20);

        mainWindow->setProperty("width", 250);
        mainWindow->setProperty("height", 200);
        mainWindow->setProperty("x", 123);
        mainWindow->setProperty("y", 86);

        QTimer::singleShot(1000, [&loop, findMainWindow, firstAppEngine, createApplicationEnigne]() {

            delete firstAppEngine;

            auto secondEngine = createApplicationEnigne();

            QTimer::singleShot(2000, [&loop, secondEngine, findMainWindow]() {
                auto mainWindow2 = findMainWindow(secondEngine);

                CHECK(mainWindow2->property("width").toInt() == 250);
                CHECK(mainWindow2->property("height").toInt() == 200);
                CHECK(mainWindow2->property("x").toInt() == 123);
                CHECK(mainWindow2->property("y").toInt() == 86);

                delete secondEngine;

                loop.quit();
            });
        });
    });

    loop.exec();
}
