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
#include "cwErrorListModel.h"
#include "cwExternalSourceSettings.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwTrip.h"
#include "LoadProjectHelper.h"

// Qt
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>

// Std
#include <memory>

namespace {

// Wipes the store's QSettings group so counting tests start from a
// known-empty state. Safe for concurrent CI processes - the test
// harness gives every process a pid-unique application name, so each
// process has its own QSettings backing store.
void clearExternalCenterlineSettings()
{
    QSettings settings;
    settings.remove(QStringLiteral("externalCenterlineSources"));
}

} // namespace

TEST_CASE("cwExternalSourceSettings entries persist to a fresh instance via the shared store",
          "[ExternalSourceSettings]")
{
    // Owners from two unrelated projects share the one per-machine store -
    // owner UUIDs are globally unique, so there is no per-project namespace.
    // The same mechanics cover the no-GC contract: nothing prunes entries,
    // so an owner whose project was deleted long ago resolves identically.
    const QUuid projectAOwner = QUuid::createUuid();
    const QUuid projectBOwner = QUuid::createUuid();
    {
        cwExternalSourceSettings written;
        written.setSourcePath(projectAOwner, QStringLiteral("/projects/a/entry.svx"));
        written.setSourcePath(projectBOwner, QStringLiteral("/projects/b/entry.dat"));
    }

    const cwExternalSourceSettings fresh;
    CHECK(fresh.sourcePathFor(projectAOwner) == QStringLiteral("/projects/a/entry.svx"));
    CHECK(fresh.sourcePathFor(projectBOwner) == QStringLiteral("/projects/b/entry.dat"));
    CHECK(fresh.isLiveLink(projectAOwner));
    CHECK(fresh.isLiveLink(projectBOwner));
}

TEST_CASE("cwExternalSourceSettings returns no entry for an unrecorded owner",
          "[ExternalSourceSettings]")
{
    cwExternalSourceSettings settings;
    const QUuid recordedOwner = QUuid::createUuid();
    const QString sourcePath = QStringLiteral("/path/recorded.svx");
    settings.setSourcePath(recordedOwner, sourcePath);

    const QUuid unrelatedOwner = QUuid::createUuid();
    CHECK(settings.sourcePathFor(unrelatedOwner).isEmpty());
    CHECK_FALSE(settings.hasSource(unrelatedOwner));

    CHECK(settings.sourcePathFor(recordedOwner) == sourcePath);
    CHECK(settings.hasSource(recordedOwner));
}

TEST_CASE("cwExternalSourceSettings::isLiveLink is true iff a non-empty sourcePath is recorded",
          "[ExternalSourceSettings]")
{
    cwExternalSourceSettings settings;
    const QUuid liveLinkOwner = QUuid::createUuid();
    const QUuid importOwner = QUuid::createUuid();

    settings.setSourcePath(liveLinkOwner, QStringLiteral("/path/live.svx"));
    settings.setSourcePath(importOwner, QString());

    CHECK(settings.isLiveLink(liveLinkOwner));
    // Import-mode entry: recorded, but no path.
    CHECK_FALSE(settings.isLiveLink(importOwner));
    CHECK(settings.hasSource(importOwner));
    // No entry at all, and the null owner.
    CHECK_FALSE(settings.isLiveLink(QUuid::createUuid()));
    CHECK_FALSE(settings.isLiveLink(QUuid()));
}

TEST_CASE("cwExternalSourceSettings keeps an entry whose sourcePath no longer exists on disk",
          "[ExternalSourceSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwExternalSourceSettings settings;
    const QUuid ownerId = QUuid::createUuid();
    const QString missingSourcePath =
        QDir(tempDir.path()).filePath(QStringLiteral("definitely-not-here.svx"));
    REQUIRE_FALSE(QFileInfo::exists(missingSourcePath));
    settings.setSourcePath(ownerId, missingSourcePath);

    // The store returns the entry as-is - surfacing the missing-source
    // banner is the consumer's responsibility, not the store's.
    const cwExternalSourceSettings fresh;
    CHECK(fresh.sourcePathFor(ownerId) == missingSourcePath);
    CHECK(fresh.hasSource(ownerId));
}

TEST_CASE("cwExternalSourceSettings::setSourcePath upserts existing owner without duplicating",
          "[ExternalSourceSettings]")
{
    clearExternalCenterlineSettings();

    cwExternalSourceSettings settings;
    const QUuid ownerId = QUuid::createUuid();

    settings.setSourcePath(ownerId, QStringLiteral("/path/one.svx"));
    settings.setSourcePath(ownerId, QStringLiteral("/path/two.svx"));

    CHECK(settings.externalCenterlineSources().size() == 1);
    CHECK(settings.sourcePathFor(ownerId) == QStringLiteral("/path/two.svx"));
}

TEST_CASE("cwExternalSourceSettings::clearSourcePath removes only the matching entry",
          "[ExternalSourceSettings]")
{
    clearExternalCenterlineSettings();

    cwExternalSourceSettings settings;
    const QUuid first = QUuid::createUuid();
    const QUuid second = QUuid::createUuid();

    settings.setSourcePath(first, QStringLiteral("/path/first.svx"));
    settings.setSourcePath(second, QStringLiteral("/path/second.svx"));
    REQUIRE(settings.externalCenterlineSources().size() == 2);

    settings.clearSourcePath(first);
    CHECK(settings.externalCenterlineSources().size() == 1);
    CHECK_FALSE(settings.hasSource(first));
    CHECK(settings.sourcePathFor(second) == QStringLiteral("/path/second.svx"));
}

TEST_CASE("cwExternalSourceSettings emits externalCenterlineSourcesChanged only on real mutations",
          "[ExternalSourceSettings]")
{
    cwExternalSourceSettings settings;
    QSignalSpy changedSpy(&settings, &cwExternalSourceSettings::externalCenterlineSourcesChanged);
    const QUuid ownerId = QUuid::createUuid();

    settings.setSourcePath(ownerId, QStringLiteral("/path/one.svx"));
    CHECK(changedSpy.count() == 1);

    // No-op upsert: same value again.
    settings.setSourcePath(ownerId, QStringLiteral("/path/one.svx"));
    CHECK(changedSpy.count() == 1);

    settings.setSourcePath(ownerId, QStringLiteral("/path/two.svx"));
    CHECK(changedSpy.count() == 2);

    // No-op clear: entry doesn't exist.
    settings.clearSourcePath(QUuid::createUuid());
    CHECK(changedSpy.count() == 2);

    settings.clearSourcePath(ownerId);
    CHECK(changedSpy.count() == 3);

    // Null owner is ignored entirely.
    settings.setSourcePath(QUuid(), QStringLiteral("/path/ignored.svx"));
    CHECK(changedSpy.count() == 3);
}

TEST_CASE("cwExternalSourceSettings listing skips junk keys in the settings group",
          "[ExternalSourceSettings]")
{
    clearExternalCenterlineSettings();

    cwExternalSourceSettings settings;
    const QUuid validOwner = QUuid::createUuid();
    const QString validSource = QStringLiteral("/path/valid.svx");
    settings.setSourcePath(validOwner, validSource);

    // Plant a key that isn't an owner UUID - a hand-edited or corrupted
    // store must not break the listing.
    {
        QSettings raw;
        raw.setValue(QStringLiteral("externalCenterlineSources/not-a-uuid"),
                     QStringLiteral("/path/junk.svx"));
    }

    CHECK(settings.externalCenterlineSources().size() == 1);
    CHECK(settings.sourcePathFor(validOwner) == validSource);
}

TEST_CASE("cwExternalSourceSettings entries survive a bundled .cw close and reopen by owner UUID",
          "[ExternalSourceSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString bundlePath =
        QDir(tempDir.path()).filePath(QStringLiteral("bundle-roundtrip.cw"));
    const QString sourcePath = QStringLiteral("/Users/alice/topodroid/entry.svx");

    QUuid tripId;
    QString firstExtractionDir;
    {
        auto rootData = std::make_unique<cwRootData>();
        auto* project = rootData->project();
        rootData->account()->setName(QStringLiteral("ExternalSourceSettings Tester"));
        rootData->account()->setEmail(QStringLiteral("localsettings.tester@example.com"));

        auto* region = project->cavingRegion();
        region->addCave();
        cwCave* cave = region->cave(0);
        cave->setName(QStringLiteral("Bundle Cave"));
        cave->addTrip();
        cwTrip* trip = cave->trip(0);
        trip->setName(QStringLiteral("Bundle Trip"));

        REQUIRE(project->saveAs(bundlePath));
        project->waitSaveToFinish();
        REQUIRE(project->fileType() == cwProject::BundledGitFileType);

        tripId = trip->id();
        REQUIRE_FALSE(tripId.isNull());

        // Record the live-link entry through the rootData-owned store -
        // it lands in the machine-global QSettings, never in the bundle's
        // extraction tree (which is abandoned on close).
        rootData->externalSourceSettings()->setSourcePath(tripId, sourcePath);

        firstExtractionDir = project->dataRootDir().absolutePath();
    }

    auto reloaded = fileToProject(bundlePath);
    REQUIRE(reloaded->errorModel()->size() == 0);
    REQUIRE(reloaded->cavingRegion()->caveCount() == 1);
    REQUIRE(reloaded->cavingRegion()->cave(0)->tripCount() == 1);
    cwTrip* reloadedTrip = reloaded->cavingRegion()->cave(0)->trip(0);
    REQUIRE(reloadedTrip->id() == tripId);
    REQUIRE(reloaded->dataRootDir().absolutePath() != firstExtractionDir);

    // A fresh extraction path and a fresh store instance, but the owner
    // UUID still resolves through the machine-global QSettings.
    const cwExternalSourceSettings settings;
    CHECK(settings.sourcePathFor(reloadedTrip->id()) == sourcePath);
    CHECK(settings.isLiveLink(reloadedTrip->id()));
}
