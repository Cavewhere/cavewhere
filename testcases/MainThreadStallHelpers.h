#pragma once

//Qt includes
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Std includes
#include <algorithm>
#include <functional>

//Our includes
#include "TestHelper.h"

namespace mainThreadStall {
inline constexpr int kFrameIntervalMs = 16;
inline constexpr int kLeadInTicks = 3;
inline constexpr int kTrailingTicks = 3;
inline constexpr int kStallWaitMs = 600000;
} // namespace mainThreadStall

//! The longest gap between two ticks of a 16 ms timer on the main thread while
//! one queued call runs \a work: how long \a work holds the event loop.
inline qint64 longestMainThreadStall(const std::function<void()>& work)
{
    using namespace mainThreadStall;
    QTimer frame;
    frame.setTimerType(Qt::PreciseTimer);
    frame.setInterval(kFrameIntervalMs);

    QElapsedTimer clock;
    qint64 lastTick = 0;
    qint64 longest = 0;
    int ticks = 0;
    int ticksAfterWork = -1;
    QObject::connect(&frame, &QTimer::timeout, [&]() {
        const qint64 now = clock.elapsed();
        longest = std::max(longest, now - lastTick);
        lastTick = now;
        ++ticks;
        if (ticksAfterWork >= 0) {
            ++ticksAfterWork;
        }
        if (ticks == kLeadInTicks) {
            longest = 0;
            QTimer::singleShot(0, [&]() {
                work();
                ticksAfterWork = 0;
            });
        }
    });

    clock.start();
    frame.start();
    REQUIRE(waitUntil([&]() { return ticksAfterWork >= kTrailingTicks; }, kStallWaitMs));
    frame.stop();
    return longest;
}

//! The longest gap between two ticks of a 16 ms timer on the main thread from
//! a queued call to \a start until \a isDone answers true at a tick: how long
//! the event loop is held while asynchronous work started by \a start lands.
inline qint64 longestMainThreadStallUntil(const std::function<void()>& start,
                                          const std::function<bool()>& isDone)
{
    using namespace mainThreadStall;
    QTimer frame;
    frame.setTimerType(Qt::PreciseTimer);
    frame.setInterval(kFrameIntervalMs);

    QElapsedTimer clock;
    qint64 lastTick = 0;
    qint64 longest = 0;
    int ticks = 0;
    bool started = false;
    bool done = false;
    QObject::connect(&frame, &QTimer::timeout, [&]() {
        const qint64 now = clock.elapsed();
        longest = std::max(longest, now - lastTick);
        lastTick = now;
        ++ticks;
        if (started && isDone()) {
            done = true;
        }
        if (ticks == kLeadInTicks) {
            longest = 0;
            QTimer::singleShot(0, [&]() {
                start();
                started = true;
            });
        }
    });

    clock.start();
    frame.start();
    REQUIRE(waitUntil([&]() { return done; }, kStallWaitMs));
    frame.stop();
    return longest;
}
