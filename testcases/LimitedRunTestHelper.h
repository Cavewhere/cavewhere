#ifndef LIMITEDRUNTESTHELPER_H
#define LIMITEDRUNTESTHELPER_H

//Our includes
#include "cwProgressNode.h"
#include "TestHelper.h"

//Qt includes
#include <QFuture>
#include <QList>
#include <QString>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

//Std includes
#include <algorithm>
#include <functional>

//Helpers for tests that watch AsyncFuture::limited() admit a pipeline's items

//How long a limited run gets to settle before the test fails instead of stalling
inline constexpr int kLimitedRunTimeoutMs = 120000;

template <typename T>
bool isSettled(const QFuture<T>& future)
{
    return future.isFinished() || future.isCanceled();
}

template <typename T>
bool allSettled(const QList<QFuture<T>>& futures)
{
    return std::all_of(futures.begin(), futures.end(), [](const QFuture<T>& future) {
        return isSettled(future);
    });
}

//Spins the event loop, calling onPoll each pass, until every future settles
//or the deadline passes
template <typename T, typename OnPoll>
bool spinUntilSettled(const QList<QFuture<T>>& futures, OnPoll onPoll)
{
    return waitUntil([&]() {
        onPoll();
        return allSettled(futures);
    }, kLimitedRunTimeoutMs);
}

template <typename T>
bool spinUntilSettled(const QList<QFuture<T>>& futures)
{
    return spinUntilSettled(futures, []() {});
}

//One pass of spinFinelyUntil: short enough to stop between two hops of a run
inline constexpr int kFineSpinSliceMs = 2;

//Like waitUntil, but checks the condition every kFineSpinSliceMs, so a test can
//act on a run's state before the run's next hop changes it
inline bool spinFinelyUntil(const std::function<bool()>& condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while(!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents,
                                        kFineSpinSliceMs);
    }
    return condition();
}

inline bool hasActiveChild(const cwProgressNodePtr& root, const QString& name)
{
    const QList<cwProgressNodePtr> children = root->activeChildren();
    return std::any_of(children.begin(), children.end(),
                       [&name](const cwProgressNodePtr& child) { return child->name() == name; });
}

#endif // LIMITEDRUNTESTHELPER_H
