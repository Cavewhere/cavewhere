/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwPhaseJob.h"
#include "cwFutureManagerModel.h"
#include "cwFutureManagerToken.h"

//Qt includes
#include <QCoreApplication>
#include <QElapsedTimer>

namespace {

// The model resolves rows through queued QFutureWatcher events, so pump the
// event loop until the condition holds (or the deadline passes).
bool processEventsUntil(const std::function<bool()>& condition, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while(!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return condition();
}

} // namespace

TEST_CASE("cwPhaseJob surfaces a job in the manager and finish removes it",
          "[cwPhaseJob]")
{
    cwFutureManagerModel model;
    cwFutureManagerToken token(&model);

    cwPhaseJob job;
    CHECK(!job.isStarted());

    job.start(QStringLiteral("Rendering map tiles"), 42, token);
    CHECK(job.isStarted());

    REQUIRE(model.rowCount() == 1);
    const QModelIndex index = model.index(0);
    CHECK(index.data(cwFutureManagerModel::NameRole).toString().toStdString()
          == "Rendering map tiles");
    CHECK(index.data(cwFutureManagerModel::NumberOfStepRole).toInt() == 42);

    CHECK(!job.future().isCanceled());
    CHECK(!job.future().isFinished());

    job.setProgressValue(7);
    job.finish();

    CHECK(job.future().isFinished());
    CHECK(processEventsUntil([&model]() { return model.rowCount() == 0; }));
}

TEST_CASE("cwPhaseJob with an invalid token runs silently", "[cwPhaseJob]")
{
    // The preview path: the job still runs (so the caller's exit paths stay
    // unconditional) but nothing is registered anywhere.
    cwPhaseJob job;
    job.start(QStringLiteral("Silent phase"), 10, cwFutureManagerToken());

    CHECK(job.isStarted());
    CHECK(!job.future().isCanceled());

    job.setProgressValue(10);
    job.finish();
    CHECK(job.future().isFinished());
}

TEST_CASE("cwPhaseJob destructor resolves a destroyed job", "[cwPhaseJob]")
{
    cwFutureManagerModel model;
    cwFutureManagerToken token(&model);

    QFuture<void> future;
    {
        cwPhaseJob job;
        job.start(QStringLiteral("Abandoned phase"), 5, token);
        future = job.future();
        REQUIRE(model.rowCount() == 1);
    }

    CHECK(future.isFinished());
    CHECK(processEventsUntil([&model]() { return model.rowCount() == 0; }));
}

TEST_CASE("cwPhaseJob finish is idempotent and legal after a cancel",
          "[cwPhaseJob]")
{
    cwFutureManagerModel model;
    cwFutureManagerToken token(&model);

    cwPhaseJob job;
    job.start(QStringLiteral("Canceled phase"), 5, token);
    REQUIRE(model.rowCount() == 1);

    // The owner-side early resolve (cancelCapture/deleteSceneItems in
    // cwCaptureViewport): cancel the future, then the exit branch still calls
    // finish(), possibly more than once.
    job.future().cancel();
    job.setProgressValue(2); // tick racing a cancel must be harmless
    job.finish();
    job.finish();

    CHECK(job.future().isFinished());
    CHECK(processEventsUntil([&model]() { return model.rowCount() == 0; }));
}
