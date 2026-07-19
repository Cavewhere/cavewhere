/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef EXTERNALCENTERLINETESTHELPERS_H
#define EXTERNALCENTERLINETESTHELPERS_H

// Shared fixture helpers for the external-centerline suites
// ([Attach][...], [LinePlotManager][AttachedCenterlinesModel],
// [LinePlotManager][AsyncScan]). Header-only, Catch2-based like the
// other testcases/*Helper*.h headers.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwTrip.h"

// Test helpers
#include "LoadProjectHelper.h"

// Qt
#include <QAbstractItemModel>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>
#include <QVariant>

// Std
#include <functional>

// Watcher events flow through OS notifications and worker scans settle
// on the next event-loop spins. Tests poll a predicate against this
// budget — picked well above the actual settle time so a busy CI under
// valgrind still has headroom; the watcher itself fires within ~10ms.
inline constexpr int kWatcherWaitMs = 5000;

// Per-iteration event-loop budget inside tryWait. Long enough for one
// AsyncFuture continuation to run; short enough that the predicate
// re-evaluates promptly on flaky-CI scheduling jitter.
inline constexpr int kPollEventsBudgetMs = 50;

// Inside a tryWait predicate tests can pump the loop once more so a
// freshly queued continuation the outer pump just missed can land
// before the predicate re-evaluates. Smaller than kPollEventsBudgetMs
// because it only drains what is already queued.
inline constexpr int kInnerPollEventsMs = 10;

inline bool tryWait(int timeoutMs, std::function<bool()> predicate)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate()) {
        if (timer.elapsed() >= timeoutMs) {
            return predicate(); // one last evaluation under the deadline
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, kPollEventsBudgetMs);
    }
    return true;
}

// Path of a file in the external-centerlines test dataset.
inline QString fixturePath(const QString& name)
{
    return testcasesDatasetSourcePath(QStringLiteral("external-centerlines/%1").arg(name));
}

inline QString tempSubdir(const QTemporaryDir& root, const QString& name)
{
    const QDir baseDir(root.path());
    REQUIRE(baseDir.mkpath(name));
    return baseDir.absoluteFilePath(name);
}

// Copies `source` to <attachDir>/<basename(source)> so the worker's
// *include path resolves at solve time. Returns the destination path
// so the test can later edit it directly.
inline QString seedAttachment(const QString& attachDir, const QString& source)
{
    const QString basename = QFileInfo(source).fileName();
    const QString dest = QDir(attachDir).absoluteFilePath(basename);
    REQUIRE_FALSE(source.isEmpty());
    REQUIRE(QFile::exists(source));
    if (QFile::exists(dest)) {
        QFile::remove(dest);
    }
    REQUIRE(QFile::copy(source, dest));
    return dest;
}

// Overwrite a watched file in place (open-truncate-write) — the same
// shape as a user editing the file in an editor that saves in place,
// which QFileSystemWatcher on macOS treats differently from a
// rename-over.
inline void overwriteFile(const QString& path, const QByteArray& content)
{
    QFile f(path);
    REQUIRE(f.open(QFile::WriteOnly | QFile::Truncate));
    REQUIRE(f.write(content) == content.size());
    f.close();
}

inline QByteArray fileContents(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QFile::ReadOnly));
    return file.readAll();
}

inline cwCave* addEmptyCave(cwCavingRegion& region, const QString& name)
{
    cwCave* cave = new cwCave();
    cave->setName(name);
    region.addCave(cave);
    return cave;
}

inline cwTrip* addEmptyTrip(cwCave* cave, const QString& name)
{
    cwTrip* trip = new cwTrip();
    trip->setName(name);
    cave->addTrip(trip);
    return trip;
}

inline cwTrip* addAttachedTrip(cwCave* cave, const QString& name)
{
    cwTrip* trip = addEmptyTrip(cave, name);
    trip->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));
    return trip;
}

inline QVariant roleAt(const QAbstractItemModel* model, int row, int role)
{
    return model->data(model->index(row, 0), role);
}

#endif // EXTERNALCENTERLINETESTHELPERS_H
