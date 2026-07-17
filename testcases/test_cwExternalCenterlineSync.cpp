/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterlineScanner.h"
#include "cwExternalCenterlineSync.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwTrip.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {

constexpr qint64 kOneHourSeconds = 3600;
constexpr qint64 kFreshOffsetSeconds = 120;
// Plenty of headroom for AsyncFuture continuations to drain through the
// save-job queue under valgrind / busy CI; the actual reconcile finishes
// in milliseconds.
constexpr int kReconcileWaitMs = 5000;

struct SavedProjectFixture {
    QTemporaryDir tempDir;
    std::unique_ptr<cwRootData> rootData;
    cwProject* project = nullptr;
    cwCave* cave = nullptr;
    cwTrip* trip = nullptr;
};

std::unique_ptr<SavedProjectFixture> makeSavedProject(const QString& projectFileBase)
{
    auto fixture = std::make_unique<SavedProjectFixture>();
    REQUIRE(fixture->tempDir.isValid());

    fixture->rootData = std::make_unique<cwRootData>();
    fixture->project = fixture->rootData->project();

    auto region = fixture->project->cavingRegion();
    region->addCave();
    fixture->cave = region->cave(0);
    fixture->cave->setName(QStringLiteral("SyncCave"));
    fixture->cave->addTrip();
    fixture->trip = fixture->cave->trip(0);
    fixture->trip->setName(QStringLiteral("SyncTrip"));

    const QString projectPath =
        QDir(fixture->tempDir.path()).filePath(projectFileBase + QStringLiteral(".cwproj"));
    REQUIRE(fixture->project->saveAs(projectPath));
    fixture->project->waitSaveToFinish();
    // Drain the queued fileSaved delivery so modified() is settled false
    // before tests take a baseline (same idiom as test_cwLazLayerSaveLoad).
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    return fixture;
}

QString writeFile(const QString& path, const QByteArray& content, const QDateTime& mtime)
{
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    {
        QFile f(path);
        REQUIRE(f.open(QFile::WriteOnly));
        f.write(content);
    }
    QFile f(path);
    REQUIRE(f.open(QFile::ReadWrite));
    REQUIRE(f.setFileTime(mtime, QFileDevice::FileModificationTime));
    return path;
}

// Shared content for all generated dep files keeps file size and content
// identical across entries, so test plants of matching dests don't have
// to track per-file content.
constexpr const char* kFakeContent = "; fake survex content\n";

// Builds a fake external dependency tree under sourceRoot and returns a
// ScanResult with canonical paths. entryRelative is the entry file path
// relative to sourceRoot; siblings is an additional list of relative dep
// paths. All written files share kFakeContent.
cwExternalCenterlineScanner::ScanResult makeScan(const QString& sourceRoot,
                                                  const QString& entryRelative,
                                                  const QStringList& siblings,
                                                  const QDateTime& mtime)
{
    cwExternalCenterlineScanner::ScanResult result;
    const QDir root(sourceRoot);
    const QString entryAbs = root.absoluteFilePath(entryRelative);
    writeFile(entryAbs, QByteArray(kFakeContent), mtime);
    result.dependencies.append(QFileInfo(entryAbs).canonicalFilePath());

    for (const QString& sib : siblings) {
        const QString sibAbs = root.absoluteFilePath(sib);
        writeFile(sibAbs, QByteArray(kFakeContent), mtime);
        result.dependencies.append(QFileInfo(sibAbs).canonicalFilePath());
    }
    return result;
}

QStringList listFilesUnder(const QDir& dir)
{
    QStringList out;
    if (!dir.exists()) {
        return out;
    }
    QDirIterator it(dir.absolutePath(),
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        out.append(it.next());
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

TEST_CASE("computePlan copies every dep into an empty attachment dir",
          "[Sync][Reconcile]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString srcRoot = QDir(tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));

    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const auto scan = makeScan(srcRoot,
                                QStringLiteral("entry.svx"),
                                { QStringLiteral("nested/child.svx"),
                                  QStringLiteral("sibling.svx") },
                                mtime);

    const QString attachmentDir = QDir(tempDir.path()).filePath(QStringLiteral("external"));

    const auto plan = cwExternalCenterlineSync::computePlan(scan, attachmentDir);

    CHECK(plan.copies.size() == 3);
    CHECK(plan.expectedFiles.size() == 3);
    CHECK(plan.removes.isEmpty());
    CHECK(plan.warnings.isEmpty());

    const QDir attachDirObj(attachmentDir);
    CHECK(plan.copies.at(0).second == attachDirObj.absoluteFilePath(QStringLiteral("entry.svx")));
    CHECK(plan.copies.at(1).second == attachDirObj.absoluteFilePath(QStringLiteral("nested/child.svx")));
    CHECK(plan.copies.at(2).second == attachDirObj.absoluteFilePath(QStringLiteral("sibling.svx")));
}

TEST_CASE("computePlan is a no-op when sources match destinations by size+mtime",
          "[Sync][Reconcile]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString srcRoot = QDir(tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));

    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const auto scan = makeScan(srcRoot,
                                QStringLiteral("entry.svx"),
                                { QStringLiteral("sibling.svx") },
                                mtime);

    const QString attachmentDir = QDir(tempDir.path()).filePath(QStringLiteral("external"));
    // Plant identical files at the destinations.
    const QDir attachDirObj(attachmentDir);
    for (const QString& dep : scan.dependencies) {
        const QString rel = QFileInfo(scan.dependencies.first()).absoluteDir().relativeFilePath(dep);
        const QString dest = attachDirObj.absoluteFilePath(rel);
        writeFile(dest, QByteArray(kFakeContent), mtime); // same content+size as sources
    }

    const auto plan = cwExternalCenterlineSync::computePlan(scan, attachmentDir);

    CHECK(plan.copies.isEmpty());
    CHECK(plan.removes.isEmpty());
    CHECK(plan.expectedFiles.size() == 2);
    CHECK(plan.warnings.isEmpty());
}

TEST_CASE("computePlan copies only the source whose mtime is newer than dest",
          "[Sync][Reconcile]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString srcRoot = QDir(tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));

    const QDateTime base = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const auto scan = makeScan(srcRoot,
                                QStringLiteral("entry.svx"),
                                { QStringLiteral("sibling.svx") },
                                base);

    // Bump entry.svx mtime forward (it now needs a fresh copy).
    const QString entryAbs = scan.dependencies.first();
    {
        QFile f(entryAbs);
        REQUIRE(f.open(QFile::ReadWrite));
        REQUIRE(f.setFileTime(base.addSecs(kFreshOffsetSeconds),
                               QFileDevice::FileModificationTime));
    }

    const QString attachmentDir = QDir(tempDir.path()).filePath(QStringLiteral("external"));
    const QDir attachDirObj(attachmentDir);
    // Plant identical-content but older-mtime files at both destinations.
    const QString entryDestPath = attachDirObj.absoluteFilePath(QStringLiteral("entry.svx"));
    const QString sibDestPath = attachDirObj.absoluteFilePath(QStringLiteral("sibling.svx"));
    writeFile(entryDestPath, QByteArray(kFakeContent), base);
    writeFile(sibDestPath, QByteArray(kFakeContent), base);

    const auto plan = cwExternalCenterlineSync::computePlan(scan, attachmentDir);

    REQUIRE(plan.copies.size() == 1);
    CHECK(plan.copies.first().first == entryAbs);
    CHECK(plan.copies.first().second == entryDestPath);
    CHECK(plan.removes.isEmpty());
    CHECK(plan.expectedFiles.size() == 2);
}

TEST_CASE("computePlan garbage-collects files not in the new closure",
          "[Sync][Reconcile]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString srcRoot = QDir(tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));

    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const auto scan = makeScan(srcRoot,
                                QStringLiteral("entry.svx"),
                                {},
                                mtime);

    const QString attachmentDir = QDir(tempDir.path()).filePath(QStringLiteral("external"));
    const QDir attachDirObj(attachmentDir);

    // Plant the (still-current) entry and a stale file from a previous scan.
    writeFile(attachDirObj.absoluteFilePath(QStringLiteral("entry.svx")),
              QByteArray(kFakeContent),
              mtime);
    const QString stalePath = attachDirObj.absoluteFilePath(QStringLiteral("dropped.svx"));
    writeFile(stalePath, QByteArrayLiteral("; dropped\n"), mtime);

    const auto plan = cwExternalCenterlineSync::computePlan(scan, attachmentDir);

    CHECK(plan.copies.isEmpty());
    REQUIRE(plan.removes.size() == 1);
    CHECK(plan.removes.first() == QDir::cleanPath(stalePath));
    CHECK(plan.expectedFiles.size() == 1);
    CHECK(plan.warnings.isEmpty());
}

TEST_CASE("computePlan warns when a dep escapes the attachment dir",
          "[Sync][Reconcile]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString srcRoot = QDir(tempDir.path()).filePath(QStringLiteral("src/inner"));
    REQUIRE(QDir().mkpath(srcRoot));
    const QString outsideRoot = QDir(tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(outsideRoot));

    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    cwExternalCenterlineScanner::ScanResult scan;
    const QString entryAbs = QDir(srcRoot).filePath(QStringLiteral("entry.svx"));
    writeFile(entryAbs, QByteArrayLiteral("; entry\n"), mtime);
    scan.dependencies.append(QFileInfo(entryAbs).canonicalFilePath());

    // Sibling lives in the parent dir of entry's parent: ../escaped.svx
    const QString escapedAbs = QDir(outsideRoot).filePath(QStringLiteral("escaped.svx"));
    writeFile(escapedAbs, QByteArrayLiteral("; escaped\n"), mtime);
    scan.dependencies.append(QFileInfo(escapedAbs).canonicalFilePath());

    const QString attachmentDir = QDir(tempDir.path()).filePath(QStringLiteral("external"));
    const auto plan = cwExternalCenterlineSync::computePlan(scan, attachmentDir);

    REQUIRE(plan.copies.size() == 1);
    CHECK(plan.copies.first().first == scan.dependencies.first());
    REQUIRE(plan.warnings.size() == 1);
    CHECK(plan.warnings.first().contains(QStringLiteral("escaped.svx")));
    CHECK(plan.expectedFiles.size() == 1);
}

TEST_CASE("computePlan returns empty plan when scan has no dependencies",
          "[Sync][Reconcile]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    cwExternalCenterlineScanner::ScanResult empty;
    const auto plan = cwExternalCenterlineSync::computePlan(
        empty, QDir(tempDir.path()).filePath(QStringLiteral("external")));
    CHECK(plan.copies.isEmpty());
    CHECK(plan.removes.isEmpty());
    CHECK(plan.expectedFiles.isEmpty());
    CHECK(plan.warnings.isEmpty());
}

TEST_CASE("reconcile copies missing files into attachment dir",
          "[Sync][Reconcile]")
{
    auto fixture = makeSavedProject(QStringLiteral("reconcile-initial"));
    const QString attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave).absolutePath();

    const QString srcRoot = QDir(fixture->tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));
    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const auto scan = makeScan(srcRoot,
                                QStringLiteral("entry.svx"),
                                { QStringLiteral("nested/child.svx") },
                                mtime);

    auto future = cwExternalCenterlineSync::reconcile(
        fixture->project->saveLoad(), scan, attachmentDir);
    fixture->project->waitSaveToFinish();
    REQUIRE(AsyncFuture::waitForFinished(future, kReconcileWaitMs));

    REQUIRE(future.isFinished());
    REQUIRE_FALSE(future.result().hasError());

    const QDir attachDirObj(attachmentDir);
    CHECK(QFileInfo::exists(attachDirObj.absoluteFilePath(QStringLiteral("entry.svx"))));
    CHECK(QFileInfo::exists(attachDirObj.absoluteFilePath(QStringLiteral("nested/child.svx"))));
}

TEST_CASE("reconcile removes stranded files inside the attachment dir",
          "[Sync][Reconcile]")
{
    auto fixture = makeSavedProject(QStringLiteral("reconcile-gc"));
    const QString attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave).absolutePath();

    const QString srcRoot = QDir(fixture->tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));
    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const auto scan = makeScan(srcRoot,
                                QStringLiteral("entry.svx"),
                                {},
                                mtime);

    // Plant a stranded file from a previous scan.
    const QDir attachDirObj(attachmentDir);
    REQUIRE(QDir().mkpath(attachmentDir));
    const QString stranded = attachDirObj.absoluteFilePath(QStringLiteral("ghost.svx"));
    writeFile(stranded, QByteArrayLiteral("; ghost\n"), mtime);
    REQUIRE(QFileInfo::exists(stranded));

    auto future = cwExternalCenterlineSync::reconcile(
        fixture->project->saveLoad(), scan, attachmentDir);
    fixture->project->waitSaveToFinish();
    REQUIRE(AsyncFuture::waitForFinished(future, kReconcileWaitMs));

    REQUIRE(future.isFinished());
    CHECK_FALSE(QFileInfo::exists(stranded));
    CHECK(QFileInfo::exists(attachDirObj.absoluteFilePath(QStringLiteral("entry.svx"))));
}

TEST_CASE("reconcile is a no-op when source and dest already match",
          "[Sync][Reconcile]")
{
    auto fixture = makeSavedProject(QStringLiteral("reconcile-noop"));
    const QString attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave).absolutePath();

    const QString srcRoot = QDir(fixture->tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));
    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const auto scan = makeScan(srcRoot,
                                QStringLiteral("entry.svx"),
                                {},
                                mtime);

    // Plant matching dest before reconciling.
    const QDir attachDirObj(attachmentDir);
    const QString destPath = attachDirObj.absoluteFilePath(QStringLiteral("entry.svx"));
    writeFile(destPath, QByteArray(kFakeContent), mtime);
    const QDateTime destMtimeBefore = QFileInfo(destPath).lastModified();

    auto future = cwExternalCenterlineSync::reconcile(
        fixture->project->saveLoad(), scan, attachmentDir);
    fixture->project->waitSaveToFinish();
    REQUIRE(AsyncFuture::waitForFinished(future, kReconcileWaitMs));

    REQUIRE(future.isFinished());
    REQUIRE(QFileInfo::exists(destPath));
    CHECK(QFileInfo(destPath).lastModified() == destMtimeBefore);
}

TEST_CASE("two reconcile calls in sequence leave a consistent final state",
          "[Sync][Reconcile]")
{
    // Sequential reconciles model the realistic flow: the caller (lineplot
    // manager, Phase 1 commit 9) chains the second reconcile through the
    // AsyncFuture::Restarter after the first one settles. computePlan reads
    // the on-disk attachment dir at call time, so it must observe the first
    // batch's filesystem effects before deciding the second batch's GC set.
    // Calling both back-to-back without awaiting would race -
    // scan B's plan would see no childA yet and skip its remove.
    auto fixture = makeSavedProject(QStringLiteral("reconcile-twice"));
    const QString attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave).absolutePath();

    const QString srcRoot = QDir(fixture->tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));
    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);

    // First scan: entry + childA.
    auto scanA = makeScan(srcRoot,
                          QStringLiteral("entry.svx"),
                          { QStringLiteral("childA.svx") },
                          mtime);
    auto futureA = cwExternalCenterlineSync::reconcile(
        fixture->project->saveLoad(), scanA, attachmentDir);
    fixture->project->waitSaveToFinish();
    REQUIRE(AsyncFuture::waitForFinished(futureA, kReconcileWaitMs));

    // Second scan (closure shrunk): entry only. childA should be GC'd.
    cwExternalCenterlineScanner::ScanResult scanB;
    scanB.dependencies.append(scanA.dependencies.first());
    auto futureB = cwExternalCenterlineSync::reconcile(
        fixture->project->saveLoad(), scanB, attachmentDir);
    fixture->project->waitSaveToFinish();
    REQUIRE(AsyncFuture::waitForFinished(futureB, kReconcileWaitMs));

    const QDir attachDirObj(attachmentDir);
    CHECK(QFileInfo::exists(attachDirObj.absoluteFilePath(QStringLiteral("entry.svx"))));
    CHECK_FALSE(QFileInfo::exists(attachDirObj.absoluteFilePath(QStringLiteral("childA.svx"))));

    // Final state: only the entry remains under attachmentDir.
    const QStringList files = listFilesUnder(attachDirObj);
    REQUIRE(files.size() == 1);
    CHECK(files.first() == attachDirObj.absoluteFilePath(QStringLiteral("entry.svx")));
}

TEST_CASE("reconcile that copies files flips the project modified bit",
          "[Sync][Reconcile]")
{
    // A live-link refresh landing mid-session mutates the project's
    // on-disk shape; without the modified flip the refresh is silently
    // lost in a bundled .cw on quit-without-save (master plan §5.4).
    auto fixture = makeSavedProject(QStringLiteral("reconcile-dirty-copy"));
    REQUIRE_FALSE(fixture->project->modified());

    const QString attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave).absolutePath();

    const QString srcRoot = QDir(fixture->tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));
    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const auto scan = makeScan(srcRoot,
                                QStringLiteral("entry.svx"),
                                {},
                                mtime);

    auto future = cwExternalCenterlineSync::reconcile(
        fixture->project->saveLoad(), scan, attachmentDir);
    fixture->project->waitSaveToFinish();
    REQUIRE(AsyncFuture::waitForFinished(future, kReconcileWaitMs));

    CHECK(fixture->project->modified());
}

TEST_CASE("no-op reconcile leaves the modified bit untouched",
          "[Sync][Reconcile]")
{
    auto fixture = makeSavedProject(QStringLiteral("reconcile-dirty-noop"));
    REQUIRE_FALSE(fixture->project->modified());

    const QString attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave).absolutePath();

    const QString srcRoot = QDir(fixture->tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));
    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const auto scan = makeScan(srcRoot,
                                QStringLiteral("entry.svx"),
                                {},
                                mtime);

    // Plant a matching destination so the plan has no copies and no removes.
    const QDir attachDirObj(attachmentDir);
    writeFile(attachDirObj.absoluteFilePath(QStringLiteral("entry.svx")),
              QByteArray(kFakeContent),
              mtime);

    auto future = cwExternalCenterlineSync::reconcile(
        fixture->project->saveLoad(), scan, attachmentDir);
    fixture->project->waitSaveToFinish();
    REQUIRE(AsyncFuture::waitForFinished(future, kReconcileWaitMs));

    CHECK_FALSE(fixture->project->modified());
}

TEST_CASE("GC-only reconcile flips the project modified bit",
          "[Sync][Reconcile]")
{
    auto fixture = makeSavedProject(QStringLiteral("reconcile-dirty-gc"));
    REQUIRE_FALSE(fixture->project->modified());

    const QString attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave).absolutePath();

    const QString srcRoot = QDir(fixture->tempDir.path()).filePath(QStringLiteral("src"));
    REQUIRE(QDir().mkpath(srcRoot));
    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const auto scan = makeScan(srcRoot,
                                QStringLiteral("entry.svx"),
                                {},
                                mtime);

    // Matching entry (no copy needed) plus a stranded file from a previous
    // scan, so the plan is removes-only.
    const QDir attachDirObj(attachmentDir);
    writeFile(attachDirObj.absoluteFilePath(QStringLiteral("entry.svx")),
              QByteArray(kFakeContent),
              mtime);
    const QString stranded = attachDirObj.absoluteFilePath(QStringLiteral("ghost.svx"));
    writeFile(stranded, QByteArrayLiteral("; ghost\n"), mtime);

    auto future = cwExternalCenterlineSync::reconcile(
        fixture->project->saveLoad(), scan, attachmentDir);
    fixture->project->waitSaveToFinish();
    REQUIRE(AsyncFuture::waitForFinished(future, kReconcileWaitMs));

    CHECK_FALSE(QFileInfo::exists(stranded));
    CHECK(fixture->project->modified());
}

TEST_CASE("reconcile returns a failed future when saveLoad is null",
          "[Sync][Reconcile]")
{
    cwExternalCenterlineScanner::ScanResult scan;
    auto future = cwExternalCenterlineSync::reconcile(nullptr, scan, QStringLiteral("/tmp/none"));
    REQUIRE(AsyncFuture::waitForFinished(future, kReconcileWaitMs));
    CHECK(future.result().hasError());
}
