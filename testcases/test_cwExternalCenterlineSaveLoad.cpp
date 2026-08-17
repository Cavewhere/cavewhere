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
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"
#include "ProjectFilenameTestHelper.h"

// Qt
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>

namespace {

constexpr const char* kPlantedFileName = "planted.svx";
constexpr const char* kPlantedContent = "; planted survex content\n";

constexpr const char* kOriginalCaveName = "OriginalCave";
constexpr const char* kRenamedCaveName = "RenamedCave";

constexpr const char* kOriginalTripName = "OriginalTrip";
constexpr const char* kRenamedTripName = "RenamedTrip";

// Wide-margin mtime offsets used to drive copyIfNewer branches reliably
// regardless of filesystem timestamp precision (HFS+/APFS/NTFS commonly
// truncate to whole seconds; ext4 has nanoseconds but tools may round).
constexpr qint64 kOneHourSeconds = 3600;
constexpr qint64 kStaleOffsetSeconds = -600;
constexpr qint64 kFreshOffsetSeconds = 120;

// Pause between an mtime baseline and the change under test, so a rewrite of
// the file is unambiguous rather than lost to timestamp resolution.
constexpr unsigned long kMtimeGapMs = 50;

std::unique_ptr<SavedProjectFixture> makeSavedProject(
    const QString& projectFileBase,
    const QString& extension = QStringLiteral(".cwproj"))
{
    return makeSavedProject(projectFileBase,
                            QString::fromLatin1(kOriginalCaveName),
                            QString::fromLatin1(kOriginalTripName),
                            extension);
}

// Creates `<ownerDir>/external-centerline/planted.svx` with kPlantedContent.
// Returns the planted file's absolute path.
QString plantExternalCenterlineFile(const QDir& ownerDir)
{
    const QDir externalDir(ownerDir.absoluteFilePath(QStringLiteral("external-centerline")));
    REQUIRE(QDir().mkpath(externalDir.absolutePath()));

    const QString plantedPath = externalDir.absoluteFilePath(QString::fromLatin1(kPlantedFileName));
    QFile plantedFile(plantedPath);
    REQUIRE(plantedFile.open(QFile::WriteOnly));
    plantedFile.write(kPlantedContent);
    plantedFile.close();
    REQUIRE(QFileInfo::exists(plantedPath));

    return plantedPath;
}

} // namespace

TEST_CASE("trip station prefix round-trips through save/load",
          "[SaveLoad][scope]")
{
    constexpr const char* kPrefix = "A";

    auto fixture = makeSavedProject(QStringLiteral("station-prefix-roundtrip"));
    fixture->trip->setStationPrefix(QString::fromLatin1(kPrefix));
    REQUIRE(fixture->project->save());
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    const QString projectPath = fixture->project->filename();

    // A fresh session opening the project must see the prefix survive
    // serialization (proto Trip.station_prefix) and re-derive isScoped from it.
    auto freshRoot = std::make_unique<cwRootData>();
    freshRoot->project()->loadFile(projectPath);
    freshRoot->project()->waitLoadToFinish();

    auto region = freshRoot->project()->cavingRegion();
    REQUIRE(region->caveCount() == 1);
    cwCave* loadedCave = region->cave(0);
    REQUIRE(loadedCave->tripCount() == 1);
    cwTrip* loadedTrip = loadedCave->trip(0);
    CHECK(loadedTrip->stationPrefix() == QString::fromLatin1(kPrefix));
    CHECK(loadedTrip->isScoped());
}

TEST_CASE("harvested station names never trigger a save",
          "[SaveLoad][External][StationHarvest]")
{
    // externalStations/externalStationsError are scan output — rebuilt from
    // files the project already owns on every recompute. Wiring either to a
    // save would rewrite the trip on disk after every attach, every watched-file
    // edit and every project load, for a value that is never serialized.
    auto fixture = makeSavedProject(QStringLiteral("harvest-no-save"));
    REQUIRE_FALSE(fixture->project->modified());

    const QString tripFile = ProjectFilenameTestHelper::absolutePath(fixture->trip);
    REQUIRE(QFileInfo::exists(tripFile));
    const QDateTime mtimeBefore = QFileInfo(tripFile).lastModified();

    // APFS keeps sub-millisecond mtimes, but a gap this wide makes a rewrite
    // unmistakable rather than a timestamp-resolution argument.
    QThread::msleep(kMtimeGapMs);

    fixture->trip->setExternalStations({QStringLiteral("hanging.h1"),
                                        QStringLiteral("hanging.h2")});
    fixture->trip->setExternalStationsError(QStringLiteral("cavern: error: pretend failure"));

    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    CHECK_FALSE(fixture->project->modified());
    CHECK(QFileInfo(tripFile).lastModified() == mtimeBefore);

    // Control: a field that IS a save trigger moves the same file, so the
    // assertion above can't pass just because nothing ever writes here.
    fixture->trip->setStationPrefix(QStringLiteral("P"));
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    CHECK(QFileInfo(tripFile).lastModified() != mtimeBefore);
}

TEST_CASE("cave and region equates round-trip through save/load",
          "[SaveLoad][scope][equate]")
{
    auto fixture = makeSavedProject(QStringLiteral("equate-roundtrip"));

    // A second cave so the region (cross-cave) tie has two homes to connect.
    auto region = fixture->project->cavingRegion();
    region->addCave();
    cwCave* secondCave = region->cave(1);
    secondCave->setName(QStringLiteral("SecondCave"));
    secondCave->addTrip();
    cwTrip* secondTrip = secondCave->trip(0);

    const QUuid cave0Id = fixture->cave->id();
    const QUuid trip0Id = fixture->trip->id();
    const QUuid cave1Id = secondCave->id();

    // Within-cave tie on cave0: a bare native station equated to a trip-scoped
    // station in the same cave.
    const cwEquate caveEquate({
        cwStationHandle(cwStationHandle::NativeCave, cave0Id, QStringLiteral("1")),
        cwStationHandle(cwStationHandle::Trip, trip0Id, QStringLiteral("1"))
    });
    REQUIRE(fixture->cave->validate(caveEquate));
    fixture->cave->equates()->appendEquate(caveEquate);

    // Cross-cave tie on the region: a station in cave0 equated to one in cave1.
    const cwEquate regionEquate({
        cwStationHandle(cwStationHandle::NativeCave, cave0Id, QStringLiteral("2")),
        cwStationHandle(cwStationHandle::NativeCave, cave1Id, QStringLiteral("7"))
    });
    region->equates()->appendEquate(regionEquate);

    REQUIRE(fixture->project->save());
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    const QString projectPath = fixture->project->filename();

    auto freshRoot = std::make_unique<cwRootData>();
    freshRoot->project()->loadFile(projectPath);
    freshRoot->project()->waitLoadToFinish();

    auto loadedRegion = freshRoot->project()->cavingRegion();
    REQUIRE(loadedRegion->caveCount() == 2);

    // Cave-level equate survives on its owning cave (matched by id, not order).
    cwCave* loadedCave0 = loadedRegion->cave(0)->id() == cave0Id
            ? loadedRegion->cave(0) : loadedRegion->cave(1);
    cwCave* loadedCave1 = loadedCave0 == loadedRegion->cave(0)
            ? loadedRegion->cave(1) : loadedRegion->cave(0);
    REQUIRE(loadedCave0->equates()->count() == 1);
    const cwEquate loadedCaveEquate = loadedCave0->equates()->equateAt(0);
    CHECK(loadedCaveEquate == caveEquate);

    // The within-cave tie must land only on its owning cave, not fan onto the
    // other cave (which would still leave both region/cave counts at 1).
    CHECK(loadedCave1->equates()->count() == 0);

    // Region-level (cross-cave) equate survives on the region.
    REQUIRE(loadedRegion->equates()->count() == 1);
    CHECK(loadedRegion->equates()->equateAt(0) == regionEquate);

    Q_UNUSED(secondTrip);
}

TEST_CASE("Loading a project does not re-save the project file",
          "[SaveLoad][scope][equate]")
{
    // Regression guard for the C3 save-trigger landmine. The region equate
    // save-trigger is wired in cwSaveLoad::connectTreeModel, which runs BEFORE
    // the region is populated. setEquates() (the bulk load path) emits
    // modelReset; if that reset were wired to saveMetadata, load would rewrite
    // the top-level Project file (project->filename()) mid-load with a
    // half-built region — the exact bug that truncated legacy .cw conversions.
    // The trigger must therefore watch only rowsInserted/rowsRemoved, never
    // modelReset. Assert the project file's mtime is unchanged after a clean
    // load of a project that has both cave-level and region-level equates.
    auto fixture = makeSavedProject(QStringLiteral("equate-load-stable"));

    auto region = fixture->project->cavingRegion();
    region->addCave();
    cwCave* secondCave = region->cave(1);
    secondCave->setName(QStringLiteral("SecondCave"));

    const QUuid cave0Id = fixture->cave->id();
    const QUuid trip0Id = fixture->trip->id();
    const QUuid cave1Id = secondCave->id();

    fixture->cave->equates()->appendEquate(cwEquate({
        cwStationHandle(cwStationHandle::NativeCave, cave0Id, QStringLiteral("1")),
        cwStationHandle(cwStationHandle::Trip, trip0Id, QStringLiteral("1"))
    }));
    region->equates()->appendEquate(cwEquate({
        cwStationHandle(cwStationHandle::NativeCave, cave0Id, QStringLiteral("2")),
        cwStationHandle(cwStationHandle::NativeCave, cave1Id, QStringLiteral("7"))
    }));

    REQUIRE(fixture->project->save());
    fixture->project->waitSaveToFinish();
    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    const QString projectFile = fixture->project->filename();
    REQUIRE(QFileInfo::exists(projectFile));
    const QDateTime mtimeBeforeLoad = QFileInfo(projectFile).lastModified();

    // QTemporaryDir lands on APFS (/var/folders), whose mtime resolution is
    // sub-millisecond, so this gap reliably distinguishes a mid-load re-save
    // from the baseline.
    QThread::msleep(kMtimeGapMs);

    auto loaderRoot = std::make_unique<cwRootData>();
    loaderRoot->project()->loadFile(projectFile);
    loaderRoot->project()->waitLoadToFinish();
    loaderRoot->project()->waitSaveToFinish();
    loaderRoot->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // The loaded equates must still be intact...
    auto loadedRegion = loaderRoot->project()->cavingRegion();
    REQUIRE(loadedRegion->caveCount() == 2);
    CHECK(loadedRegion->equates()->count() == 1);

    // ...and the load must not have touched the project file on disk.
    const QDateTime mtimeAfterLoad = QFileInfo(projectFile).lastModified();
    CHECK(mtimeAfterLoad == mtimeBeforeLoad);
}

TEST_CASE("externalCenterlineDir computes <ownerDir>/external-centerline",
          "[SaveLoad][External]")
{
    auto fixture = makeSavedProject(QStringLiteral("dir-accessor"));

    const QDir caveDir = ProjectFilenameTestHelper::dir(fixture->cave);
    REQUIRE(caveDir.exists());

    const QDir externalCaveDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave);
    CHECK(externalCaveDir.absolutePath()
          == caveDir.absoluteFilePath(QStringLiteral("external-centerline")));

    const QDir tripDir = ProjectFilenameTestHelper::dir(fixture->trip);
    const QDir externalTripDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->trip);
    CHECK(externalTripDir.absolutePath()
          == tripDir.absoluteFilePath(QStringLiteral("external-centerline")));

    // ProjectFilenameTestHelper mirror agrees.
    CHECK(ProjectFilenameTestHelper::externalCenterlineDir(fixture->cave).absolutePath()
          == externalCaveDir.absolutePath());
    CHECK(ProjectFilenameTestHelper::externalCenterlineDir(fixture->trip).absolutePath()
          == externalTripDir.absolutePath());
}

TEST_CASE("cave rename cascades through external-centerline/ subdir",
          "[SaveLoad][External]")
{
    auto fixture = makeSavedProject(QStringLiteral("cave-rename-cascade"));

    const QDir originalCaveDir = ProjectFilenameTestHelper::dir(fixture->cave);
    const QString plantedPath = plantExternalCenterlineFile(originalCaveDir);

    fixture->cave->setName(QString::fromLatin1(kRenamedCaveName));
    fixture->project->waitSaveToFinish();

    CHECK_FALSE(QFileInfo::exists(plantedPath));

    const QDir renamedExternalDir =
        ProjectFilenameTestHelper::externalCenterlineDir(fixture->cave);
    const QString renamedPlantedPath =
        renamedExternalDir.absoluteFilePath(QString::fromLatin1(kPlantedFileName));
    REQUIRE(QFileInfo::exists(renamedPlantedPath));

    QFile renamedPlantedFile(renamedPlantedPath);
    REQUIRE(renamedPlantedFile.open(QFile::ReadOnly));
    CHECK(renamedPlantedFile.readAll() == QByteArray(kPlantedContent));
}

TEST_CASE("trip rename cascades through external-centerline/ subdir",
          "[SaveLoad][External]")
{
    auto fixture = makeSavedProject(QStringLiteral("trip-rename-cascade"));

    const QDir originalTripDir = ProjectFilenameTestHelper::dir(fixture->trip);
    const QString plantedPath = plantExternalCenterlineFile(originalTripDir);

    fixture->trip->setName(QString::fromLatin1(kRenamedTripName));
    fixture->project->waitSaveToFinish();

    CHECK_FALSE(QFileInfo::exists(plantedPath));

    const QDir renamedExternalDir =
        ProjectFilenameTestHelper::externalCenterlineDir(fixture->trip);
    const QString renamedPlantedPath =
        renamedExternalDir.absoluteFilePath(QString::fromLatin1(kPlantedFileName));
    REQUIRE(QFileInfo::exists(renamedPlantedPath));
}

TEST_CASE("cave delete removes external-centerline/ subdir",
          "[SaveLoad][External]")
{
    auto fixture = makeSavedProject(QStringLiteral("cave-delete-cascade"));

    const QDir originalCaveDir = ProjectFilenameTestHelper::dir(fixture->cave);
    const QString plantedPath = plantExternalCenterlineFile(originalCaveDir);
    REQUIRE(QFileInfo::exists(plantedPath));

    auto region = fixture->project->cavingRegion();
    region->removeCave(0);
    fixture->project->waitSaveToFinish();

    CHECK_FALSE(QFileInfo::exists(plantedPath));
    CHECK_FALSE(originalCaveDir.exists());
}

// Removing the trip is the only way out of an attachment now that the panel's
// Detach action is gone (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html §4.1),
// so it has to take the copied closure with it in both project formats — a
// bundled .cw keeps its tree extracted in a temporary directory, which is a
// different place than the .cwproj's on-disk tree.
TEST_CASE("trip delete removes external-centerline/ subdir",
          "[SaveLoad][External]")
{
    QString extension;
    SECTION("git-backed .cwproj") { extension = QStringLiteral(".cwproj"); }
    SECTION("bundled .cw") { extension = QStringLiteral(".cw"); }

    auto fixture = makeSavedProject(QStringLiteral("trip-delete-cascade"), extension);

    const QDir originalTripDir = ProjectFilenameTestHelper::dir(fixture->trip);
    const QDir attachmentDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->trip);
    const QString plantedPath = plantExternalCenterlineFile(originalTripDir);
    REQUIRE(QFileInfo::exists(plantedPath));
    REQUIRE(attachmentDir.exists());

    fixture->cave->removeTrip(0);
    fixture->project->waitSaveToFinish();

    CHECK_FALSE(QFileInfo::exists(plantedPath));
    CHECK_FALSE(attachmentDir.exists());
    CHECK_FALSE(originalTripDir.exists());
}

TEST_CASE("enqueueExternalCenterlineCopyIfNewer copies a newer source",
          "[SaveLoad][External]")
{
    auto fixture = makeSavedProject(QStringLiteral("copy-if-newer"));
    const QDir externalDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave);

    const QDateTime base = QDateTime::currentDateTimeUtc().addSecs(-kOneHourSeconds);
    const QString srcPath =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("src-newer.svx"));
    writeFileWithMtime(srcPath, "new content\n", base.addSecs(kFreshOffsetSeconds));

    const QString dstPath = externalDir.absoluteFilePath(QStringLiteral("dest.svx"));
    writeFileWithMtime(dstPath, "stale\n", base);

    fixture->project->saveLoad()->enqueueExternalCenterlineCopyIfNewer(srcPath, dstPath);
    fixture->project->waitSaveToFinish();

    QFile out(dstPath);
    REQUIRE(out.open(QFile::ReadOnly));
    CHECK(out.readAll() == QByteArray("new content\n"));
}

TEST_CASE("enqueueExternalCenterlineCopyIfNewer is a no-op when src and dst match",
          "[SaveLoad][External]")
{
    auto fixture = makeSavedProject(QStringLiteral("copy-if-newer-noop"));
    const QDir externalDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave);

    const QDateTime mtime = QDateTime::currentDateTimeUtc().addSecs(kStaleOffsetSeconds);
    const QString srcPath =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("src-same.svx"));
    writeFileWithMtime(srcPath, "same\n", mtime);

    const QString dstPath = externalDir.absoluteFilePath(QStringLiteral("dest.svx"));
    writeFileWithMtime(dstPath, "same\n", mtime);

    const QDateTime dstMtimeBefore = QFileInfo(dstPath).lastModified();

    fixture->project->saveLoad()->enqueueExternalCenterlineCopyIfNewer(srcPath, dstPath);
    fixture->project->waitSaveToFinish();

    CHECK(QFileInfo(dstPath).lastModified() == dstMtimeBefore);
    QFile out(dstPath);
    REQUIRE(out.open(QFile::ReadOnly));
    CHECK(out.readAll() == QByteArray("same\n"));
}

TEST_CASE("enqueueExternalCenterlineCopyIfNewer creates dest when missing",
          "[SaveLoad][External]")
{
    auto fixture = makeSavedProject(QStringLiteral("copy-if-newer-create"));
    const QDir externalDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave);

    const QString srcPath =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("src-new.svx"));
    writeFileWithMtime(srcPath, "fresh\n", QDateTime::currentDateTimeUtc());

    const QString dstPath = externalDir.absoluteFilePath(QStringLiteral("created.svx"));
    REQUIRE_FALSE(QFileInfo::exists(dstPath));

    fixture->project->saveLoad()->enqueueExternalCenterlineCopyIfNewer(srcPath, dstPath);
    fixture->project->waitSaveToFinish();

    REQUIRE(QFileInfo::exists(dstPath));
    QFile out(dstPath);
    REQUIRE(out.open(QFile::ReadOnly));
    CHECK(out.readAll() == QByteArray("fresh\n"));
}

TEST_CASE("enqueueExternalCenterlineRemoveFile deletes the file",
          "[SaveLoad][External]")
{
    auto fixture = makeSavedProject(QStringLiteral("remove-file"));
    const QDir externalDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave);

    REQUIRE(QDir().mkpath(externalDir.absolutePath()));
    const QString path = externalDir.absoluteFilePath(QStringLiteral("victim.svx"));
    {
        QFile f(path);
        REQUIRE(f.open(QFile::WriteOnly));
        f.write("doomed\n");
    }
    REQUIRE(QFileInfo::exists(path));

    fixture->project->saveLoad()->enqueueExternalCenterlineRemoveFile(path);
    fixture->project->waitSaveToFinish();

    CHECK_FALSE(QFileInfo::exists(path));
}

TEST_CASE("enqueueExternalCenterlineRemoveTree recursively deletes a subtree",
          "[SaveLoad][External]")
{
    auto fixture = makeSavedProject(QStringLiteral("remove-tree"));
    const QDir externalDir =
        fixture->project->saveLoad()->externalCenterlineDir(fixture->cave);

    REQUIRE(QDir().mkpath(externalDir.absoluteFilePath(QStringLiteral("nested"))));
    const QString shallowFile = externalDir.absoluteFilePath(QStringLiteral("shallow.svx"));
    const QString deepFile =
        externalDir.absoluteFilePath(QStringLiteral("nested/deep.svx"));
    for (const QString& path : { shallowFile, deepFile }) {
        QFile f(path);
        REQUIRE(f.open(QFile::WriteOnly));
        f.write("X\n");
    }
    REQUIRE(QFileInfo::exists(shallowFile));
    REQUIRE(QFileInfo::exists(deepFile));
    REQUIRE_FALSE(fixture->project->modified());

    fixture->project->saveLoad()->enqueueExternalCenterlineRemoveTree(externalDir.absolutePath());
    fixture->project->waitSaveToFinish();

    CHECK_FALSE(externalDir.exists());
    CHECK_FALSE(QFileInfo::exists(shallowFile));
    CHECK_FALSE(QFileInfo::exists(deepFile));
    // Removing an attachment tree (the future detach flow) is a project
    // mutation and must dirty the project.
    CHECK(fixture->project->modified());
}
