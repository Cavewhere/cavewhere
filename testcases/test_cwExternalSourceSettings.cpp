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
#include <QUrl>
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
        written.setBreadcrumbPath(projectAOwner, QStringLiteral("/projects/a/entry.svx"));
        written.setBreadcrumbPath(projectBOwner, QStringLiteral("/projects/b/entry.dat"));
    }

    const cwExternalSourceSettings fresh;
    CHECK(fresh.breadcrumbPath(projectAOwner) == QStringLiteral("/projects/a/entry.svx"));
    CHECK(fresh.breadcrumbPath(projectBOwner) == QStringLiteral("/projects/b/entry.dat"));
    CHECK(fresh.hasBreadcrumb(projectAOwner));
    CHECK(fresh.hasBreadcrumb(projectBOwner));
}

TEST_CASE("cwExternalSourceSettings returns no entry for an unrecorded owner",
          "[ExternalSourceSettings]")
{
    cwExternalSourceSettings settings;
    const QUuid recordedOwner = QUuid::createUuid();
    const QString sourcePath = QStringLiteral("/path/recorded.svx");
    settings.setBreadcrumbPath(recordedOwner, sourcePath);

    const QUuid unrelatedOwner = QUuid::createUuid();
    CHECK(settings.breadcrumbPath(unrelatedOwner).isEmpty());
    CHECK_FALSE(settings.hasBreadcrumb(unrelatedOwner));

    CHECK(settings.breadcrumbPath(recordedOwner) == sourcePath);
    CHECK(settings.hasBreadcrumb(recordedOwner));
}

TEST_CASE("cwExternalSourceSettings keeps a breadcrumb whose file no longer exists on disk",
          "[ExternalSourceSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwExternalSourceSettings settings;
    const QUuid ownerId = QUuid::createUuid();
    const QString missingSourcePath =
        QDir(tempDir.path()).filePath(QStringLiteral("definitely-not-here.svx"));
    REQUIRE_FALSE(QFileInfo::exists(missingSourcePath));
    settings.setBreadcrumbPath(ownerId, missingSourcePath);

    // The store returns the breadcrumb as-is - it is a picker default,
    // and where the dialog lands is the picker's call.
    const cwExternalSourceSettings fresh;
    CHECK(fresh.breadcrumbPath(ownerId) == missingSourcePath);
    CHECK(fresh.hasBreadcrumb(ownerId));
}

// The commit-4 gate (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html
// section 6): the breadcrumb pre-fills the Replace dialog's folder, and
// every way of having no usable breadcrumb falls back silently.
TEST_CASE("cwExternalSourceSettings::breadcrumbFolder names the folder holding the picked file",
          "[ExternalSourceSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir sourceDir(tempDir.path());
    const QString sourcePath = sourceDir.filePath(QStringLiteral("entry.svx"));

    cwExternalSourceSettings settings;
    const QUuid ownerId = QUuid::createUuid();
    settings.setBreadcrumbPath(ownerId, sourcePath);

    CHECK(settings.breadcrumbFolder(ownerId)
          == QUrl::fromLocalFile(sourceDir.absolutePath()));
}

TEST_CASE("cwExternalSourceSettings::breadcrumbFolder is empty when there is nothing to open at",
          "[ExternalSourceSettings]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwExternalSourceSettings settings;

    SECTION("no breadcrumb at all") {
        CHECK(settings.breadcrumbFolder(QUuid::createUuid()).isEmpty());
        CHECK(settings.breadcrumbFolder(QUuid()).isEmpty());
    }

    SECTION("a breadcrumb recording no path") {
        const QUuid ownerId = QUuid::createUuid();
        settings.setBreadcrumbPath(ownerId, QString());
        REQUIRE(settings.hasBreadcrumb(ownerId));
        CHECK(settings.breadcrumbFolder(ownerId).isEmpty());
    }

    SECTION("a breadcrumb whose folder is gone") {
        const QUuid ownerId = QUuid::createUuid();
        const QString removedDir =
            QDir(tempDir.path()).filePath(QStringLiteral("gone"));
        REQUIRE(QDir().mkpath(removedDir));
        settings.setBreadcrumbPath(
            ownerId, QDir(removedDir).filePath(QStringLiteral("entry.svx")));
        REQUIRE(QDir(removedDir).removeRecursively());

        CHECK(settings.breadcrumbFolder(ownerId).isEmpty());
    }

    SECTION("a breadcrumb whose file is gone but whose folder remains") {
        // The folder is still the most useful place to start, so it is
        // the one missing-file case that keeps its answer.
        const QUuid ownerId = QUuid::createUuid();
        const QDir sourceDir(tempDir.path());
        const QString missingFile = sourceDir.filePath(QStringLiteral("gone.svx"));
        REQUIRE_FALSE(QFileInfo::exists(missingFile));
        settings.setBreadcrumbPath(ownerId, missingFile);

        CHECK(settings.breadcrumbFolder(ownerId)
              == QUrl::fromLocalFile(sourceDir.absolutePath()));
    }
}

// The key format is the on-disk format: a settings file written before
// the accessors were renamed still resolves through the new API.
TEST_CASE("cwExternalSourceSettings reads a key written by an older version",
          "[ExternalSourceSettings]")
{
    clearExternalCenterlineSettings();

    const QUuid ownerId = QUuid::createUuid();
    const QString sourcePath = QStringLiteral("/projects/legacy/entry.svx");
    {
        QSettings raw;
        raw.setValue(QStringLiteral("externalCenterlineSources/")
                         + ownerId.toString(QUuid::WithoutBraces),
                     sourcePath);
    }

    const cwExternalSourceSettings settings;
    CHECK(settings.breadcrumbPath(ownerId) == sourcePath);
    CHECK(settings.hasBreadcrumb(ownerId));
    REQUIRE(settings.breadcrumbs().size() == 1);
    CHECK(settings.breadcrumbs().first().ownerId == ownerId);
    CHECK(settings.breadcrumbs().first().path == sourcePath);
}

TEST_CASE("cwExternalSourceSettings::setBreadcrumbPath upserts existing owner without duplicating",
          "[ExternalSourceSettings]")
{
    clearExternalCenterlineSettings();

    cwExternalSourceSettings settings;
    const QUuid ownerId = QUuid::createUuid();

    settings.setBreadcrumbPath(ownerId, QStringLiteral("/path/one.svx"));
    settings.setBreadcrumbPath(ownerId, QStringLiteral("/path/two.svx"));

    CHECK(settings.breadcrumbs().size() == 1);
    CHECK(settings.breadcrumbPath(ownerId) == QStringLiteral("/path/two.svx"));
}

TEST_CASE("cwExternalSourceSettings::clearBreadcrumb removes only the matching entry",
          "[ExternalSourceSettings]")
{
    clearExternalCenterlineSettings();

    cwExternalSourceSettings settings;
    const QUuid first = QUuid::createUuid();
    const QUuid second = QUuid::createUuid();

    settings.setBreadcrumbPath(first, QStringLiteral("/path/first.svx"));
    settings.setBreadcrumbPath(second, QStringLiteral("/path/second.svx"));
    REQUIRE(settings.breadcrumbs().size() == 2);

    settings.clearBreadcrumb(first);
    CHECK(settings.breadcrumbs().size() == 1);
    CHECK_FALSE(settings.hasBreadcrumb(first));
    CHECK(settings.breadcrumbPath(second) == QStringLiteral("/path/second.svx"));
}

TEST_CASE("cwExternalSourceSettings emits breadcrumbsChanged only on real mutations",
          "[ExternalSourceSettings]")
{
    cwExternalSourceSettings settings;
    QSignalSpy changedSpy(&settings, &cwExternalSourceSettings::breadcrumbsChanged);
    const QUuid ownerId = QUuid::createUuid();

    settings.setBreadcrumbPath(ownerId, QStringLiteral("/path/one.svx"));
    CHECK(changedSpy.count() == 1);

    // No-op upsert: same value again.
    settings.setBreadcrumbPath(ownerId, QStringLiteral("/path/one.svx"));
    CHECK(changedSpy.count() == 1);

    settings.setBreadcrumbPath(ownerId, QStringLiteral("/path/two.svx"));
    CHECK(changedSpy.count() == 2);

    // No-op clear: entry doesn't exist.
    settings.clearBreadcrumb(QUuid::createUuid());
    CHECK(changedSpy.count() == 2);

    settings.clearBreadcrumb(ownerId);
    CHECK(changedSpy.count() == 3);

    // Null owner is ignored entirely.
    settings.setBreadcrumbPath(QUuid(), QStringLiteral("/path/ignored.svx"));
    CHECK(changedSpy.count() == 3);
}

TEST_CASE("cwExternalSourceSettings listing skips junk keys in the settings group",
          "[ExternalSourceSettings]")
{
    clearExternalCenterlineSettings();

    cwExternalSourceSettings settings;
    const QUuid validOwner = QUuid::createUuid();
    const QString validSource = QStringLiteral("/path/valid.svx");
    settings.setBreadcrumbPath(validOwner, validSource);

    // Plant a key that isn't an owner UUID - a hand-edited or corrupted
    // store must not break the listing.
    {
        QSettings raw;
        raw.setValue(QStringLiteral("externalCenterlineSources/not-a-uuid"),
                     QStringLiteral("/path/junk.svx"));
    }

    CHECK(settings.breadcrumbs().size() == 1);
    CHECK(settings.breadcrumbPath(validOwner) == validSource);
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

        // Record the breadcrumb through the rootData-owned store - it
        // lands in the machine-global QSettings, never in the bundle's
        // extraction tree (which is abandoned on close).
        rootData->externalSourceSettings()->setBreadcrumbPath(tripId, sourcePath);

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
    CHECK(settings.breadcrumbPath(reloadedTrip->id()) == sourcePath);
    CHECK(settings.hasBreadcrumb(reloadedTrip->id()));
}
