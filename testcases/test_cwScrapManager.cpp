//Catch includes
#include <catch.hpp>

//Our includes
#include "cwScrapManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "TestHelper.h"
#include "cwAsyncFuture.h"

//Qt includes
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <QThreadPool>

//Std includes
#include <random>

//Async includes
#include "asyncfuture.h"

TEST_CASE("cwScrapManager should make the file size grow when re-calculaing scraps", "[cwScrapManager]") {
    QFile file;
    qint64 firstSize;

    {
        auto rootData = std::make_unique<cwRootData>();
        auto project = rootData->project();
        fileToProject(project, "://datasets/test_cwScrapManager/scrapGuessNeigborPlan.cw");

        file.setFileName(project->filename());
        file.open(QFile::ReadOnly);
        firstSize = file.size();

        auto scrapManager = rootData->scrapManager();

        int numberOfRuns = 5;
        QList<qint64> runTime;
        QElapsedTimer timer;
        for(int i = 0; i < numberOfRuns; i++) {
            timer.restart();
            scrapManager->updateAllScraps();
            scrapManager->waitForFinish();
            runTime.append(timer.nsecsElapsed());
        }

        double runFraction = 1 / static_cast<double>(runTime.size());
        double averageTime = std::accumulate(runTime.begin(), runTime.end(), 0.0,
                                             [runFraction](double average, int runTime)
        {
            return average + runFraction * runTime;
        });

        std::default_random_engine generator;
        std::uniform_real_distribution<double> distribution(0.0,averageTime * 1.5);

        for(int i = 0; i < 20; i++) {
            double waitTime = distribution(generator);

            QTimer timer;
            timer.setInterval(waitTime * 1e-6);
            timer.start();

            scrapManager->updateAllScraps();

            QEventLoop eventLoop;
            QObject::connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
            eventLoop.exec();
        }

        scrapManager->waitForFinish();
        QThreadPool::globalInstance()->waitForDone();
        INFO("Filename:" << project->filename());
    }

    auto nextSize = file.size();
    double ratio = nextSize / static_cast<double>(firstSize);
    CHECK(ratio <= Approx(1.01));
}
