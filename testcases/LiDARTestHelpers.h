#ifndef LIDARTESTHELPERS_H
#define LIDARTESTHELPERS_H

//Qt includes
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

//Our includes
#include "cwFutureManagerModel.h"
#include "cwNoteLiDARManager.h"
#include "cwRootData.h"
#include "cwUpdatable.h"

//Pumps the event loop until the pipeline is done and its row has gone, and
//says whether it got there. The detail line is polled on a timer, so a run
//has to be watched, never waited out in a nested loop.
[[nodiscard]] inline bool pumpUntilLiDARSettles(cwRootData* rootData, cwNoteLiDARManager* manager)
{
    //A cold run's texture encode alone can outlast two minutes under CPU load
    constexpr int kRunTimeoutMs = 600000;
    constexpr int kPollWaitMs = 2;

    const auto settled = [rootData, manager]() {
        return manager->updateState() == cwUpdatable::State::Clean
               && rootData->futureManagerModel()->rowCount() == 0;
    };

    QElapsedTimer timer;
    timer.start();
    while(timer.elapsed() < kRunTimeoutMs && !settled()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents,
                                        kPollWaitMs);
    }
    return settled();
}

#endif // LIDARTESTHELPERS_H
