/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The N2 gate (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §5): one status
// model says, for every attachment this machine remembers a source for,
// whether that source has moved on since it was copied - and a project
// open sweeps them all.

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwExternalSourceStatusModel.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QString>
#include <QUuid>

// Std
#include <memory>

namespace {

using Status = cwExternalSourceStatusModel::Status;

// Wide margin so a planted edit reads as a different mtime whatever the
// filesystem's granularity, which can be a full second.
constexpr int kEditIsNewerSeconds = 3600;

const QByteArray kOneShot("A1 A2 5.0 0 0\n");
const QByteArray kTwoShots("A1 A2 5.0 0 0\nA2 A3 7.25 90 0\n");
// Same byte count as kOneShot, different contents.
const QByteArray kOneShotSameSize("A1 A2 6.0 0 0\n");

// A one-file Survex source outside the project, written with a chosen
// mtime so an edit is unambiguous. Returns the path.
QString writeSource(const QString& path,
                    const QByteArray& shots,
                    const QDateTime& mtime = QDateTime::currentDateTimeUtc())
{
    return writeFileWithMtime(path,
                              QByteArray("*begin Alpha\n"
                                         "*data normal from to tape compass clino\n")
                                  + shots + "*end Alpha\n",
                              mtime);
}

QString sourceDirOf(const SavedProjectFixture* fixture)
{
    const QString dir = QDir(fixture->tempDir.path()).absoluteFilePath(
        QStringLiteral("source"));
    REQUIRE(QDir().mkpath(dir));
    return dir;
}

QString sourcePathIn(const QString& dir, const QString& name)
{
    return QDir(dir).absoluteFilePath(name);
}

cwExternalSourceStatusModel* statusModelOf(const SavedProjectFixture* fixture)
{
    return fixture->rootData->externalCenterlineManager()->sourceStatusModel();
}

// Re-reads every attachment (the same pass a project open runs) and waits
// for it to land.
void sweep(SavedProjectFixture* fixture)
{
    fixture->rootData->externalCenterlineManager()->rescanAttachments();
    drainPipelines(fixture);
}

std::unique_ptr<SavedProjectFixture> makeStatusProject(const QString& projectFileBase)
{
    return makeSavedProject(projectFileBase,
                            QStringLiteral("StatusCave"),
                            QStringLiteral("StatusTrip"));
}

// Saves and drains so the next assertion starts from an unmodified
// project.
void saveAndSettle(SavedProjectFixture* fixture)
{
    REQUIRE(fixture->project->save());
    drainPipelines(fixture);
    REQUIRE_FALSE(fixture->project->modified());
}

const cwExternalSourceSettings::SourceFileFingerprint&
onlyRecord(const cwExternalSourceSettings::SourceFingerprint& fingerprint)
{
    REQUIRE(fingerprint.files.size() == 1);
    return fingerprint.files.at(0);
}

} // namespace

TEST_CASE("the sweep at project open reports a source edited while the project was closed",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-open-sweep"));
    const QString sourcePath =
        writeSource(sourcePathIn(sourceDirOf(fixture.get()), QStringLiteral("entry.svx")),
                    kOneShot);

    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());
    saveAndSettle(fixture.get());

    const QUuid tripId = fixture->trip->id();
    CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::UpToDate);

    const QString projectPath = fixture->project->filename();

    // The evening of edits the app never saw.
    writeSource(sourcePath, kTwoShots,
                QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds));

    auto freshRoot = std::make_unique<cwRootData>();
    freshRoot->project()->loadFile(projectPath);
    freshRoot->project()->waitLoadToFinish();
    freshRoot->linePlotManager()->waitToFinish();
    freshRoot->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    auto* statuses = freshRoot->externalCenterlineManager()->sourceStatusModel();
    REQUIRE(statuses->rowCount() == 1);
    CHECK(roleAt(statuses, 0, cwExternalSourceStatusModel::OwnerIdRole).toUuid() == tripId);
    CHECK(roleAt(statuses, 0, cwExternalSourceStatusModel::SourcePathRole).toString()
          == QFileInfo(sourcePath).absoluteFilePath());
    CHECK(statuses->statusFor(tripId) == Status::Changed);
    CHECK(statuses->sourcePathFor(tripId) == QFileInfo(sourcePath).absoluteFilePath());
}

TEST_CASE("a source that is gone reads as missing rather than changed",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-missing-source"));
    const QString sourcePath =
        writeSource(sourcePathIn(sourceDirOf(fixture.get()), QStringLiteral("entry.svx")),
                    kOneShot);

    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());
    REQUIRE(statusModelOf(fixture.get())->statusFor(fixture->trip->id())
            == Status::UpToDate);

    REQUIRE(QFile::remove(sourcePath));
    sweep(fixture.get());

    CHECK(statusModelOf(fixture.get())->statusFor(fixture->trip->id())
          == Status::SourceMissing);
    // The path is still reported: it is what the user is being told about.
    CHECK(statusModelOf(fixture.get())->sourcePathFor(fixture->trip->id())
          == QFileInfo(sourcePath).absoluteFilePath());
}

TEST_CASE("an attachment this machine remembers no source for stays quiet",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-no-breadcrumb"));
    const QString sourcePath =
        writeSource(sourcePathIn(sourceDirOf(fixture.get()), QStringLiteral("entry.svx")),
                    kOneShot);

    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());
    const QUuid tripId = fixture->trip->id();

    SECTION("no breadcrumb at all - the collaborator who got the project through Git")
    {
        fixture->settings()->clearBreadcrumb(tripId);
        sweep(fixture.get());

        REQUIRE(statusModelOf(fixture.get())->rowCount() == 1);
        CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::NoBreadcrumb);
        CHECK(statusModelOf(fixture.get())->sourcePathFor(tripId).isEmpty());
    }

    SECTION("an entry written before fingerprints existed, whose source has since changed")
    {
        // The path-only writer is what an older CaveWhere recorded.
        fixture->settings()->setBreadcrumbPath(tripId, sourcePath);
        REQUIRE(fixture->settings()->fingerprint(tripId).isEmpty());

        writeSource(sourcePath, kTwoShots,
                    QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds));
        sweep(fixture.get());

        // Never nag on data older than the feature.
        CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::NoBreadcrumb);
        CHECK(statusModelOf(fixture.get())->sourcePathFor(tripId)
              == QFileInfo(sourcePath).absoluteFilePath());
    }
}

TEST_CASE("a touched but identical source stays up to date and refreshes its stored stats",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-touched-identical"));
    const QString sourcePath =
        writeSource(sourcePathIn(sourceDirOf(fixture.get()), QStringLiteral("entry.svx")),
                    kOneShot);

    attachThroughManager(fixture.get(), fixture->trip, sourcePath);
    drainPipelines(fixture.get());
    saveAndSettle(fixture.get());

    const QUuid tripId = fixture->trip->id();
    const auto stamped = fixture->settings()->fingerprint(tripId);
    REQUIRE_FALSE(stamped.isEmpty());

    // The git-checkout case: identical bytes, fresh mtime.
    const QDateTime touched =
        QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds);
    writeSource(sourcePath, kOneShot, touched);
    REQUIRE(onlyRecord(stamped).lastModified != touched.toMSecsSinceEpoch());

    QSignalSpy breadcrumbSpy(fixture->settings(),
                             &cwExternalSourceSettings::breadcrumbsChanged);
    QSignalSpy statusSpy(statusModelOf(fixture.get()),
                         &cwExternalSourceStatusModel::statusesChanged);
    QSignalSpy resetSpy(statusModelOf(fixture.get()),
                        &cwExternalSourceStatusModel::modelReset);
    sweep(fixture.get());

    CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::UpToDate);

    // The refresh is bookkeeping about files nobody changed: it neither
    // dirties the project nor announces itself as a breadcrumb change,
    // and the status model - which every banner and list binds to -
    // stays as quiet as it was, with no reset for them to churn on.
    CHECK(breadcrumbSpy.count() == 0);
    CHECK(statusSpy.count() == 0);
    CHECK(resetSpy.count() == 0);
    CHECK_FALSE(fixture->project->modified());

    // The stats now describe the file as it sits, so the next check takes
    // the pure-stat path instead of reading the file again.
    const cwExternalSourceSettings stored;
    const auto refreshed = stored.fingerprint(tripId);
    const QFileInfo sourceInfo(sourcePath);
    CHECK(onlyRecord(refreshed).lastModified == sourceInfo.lastModified().toMSecsSinceEpoch());
    CHECK(onlyRecord(refreshed).size == sourceInfo.size());
    CHECK(onlyRecord(refreshed).sha256 == onlyRecord(stamped).sha256);

    // Different bytes at the same size and mtime - the fast path's
    // documented blind spot, and so proof that the second check settles
    // on stats alone and reads no file.
    writeSource(sourcePath, kOneShotSameSize, touched);
    REQUIRE(QFileInfo(sourcePath).size() == onlyRecord(refreshed).size);
    sweep(fixture.get());
    CHECK(statusModelOf(fixture.get())->statusFor(tripId) == Status::UpToDate);
}

TEST_CASE("cave-level attachments get their own status row",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-cave-owner"));
    const QString sourceDir = sourceDirOf(fixture.get());

    const QString tripSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("trip.svx")), kOneShot);
    attachThroughManager(fixture.get(), fixture->trip, tripSource);
    drainPipelines(fixture.get());

    // Cave-level attachment: the copy path is trip-only today, so the
    // cave's attachment and its breadcrumb are seeded the way a load
    // would leave them.
    const QString caveSource =
        writeSource(sourcePathIn(sourceDir, QStringLiteral("cave.svx")), kOneShot);
    fixture->cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("cave.svx")));
    fixture->settings()->setBreadcrumb(
        fixture->cave->id(), caveSource,
        cwExternalSourceSettings::computeFingerprint({caveSource}));
    sweep(fixture.get());

    auto* statuses = statusModelOf(fixture.get());
    REQUIRE(statuses->rowCount() == 2);
    CHECK(statuses->statusFor(fixture->cave->id()) == Status::UpToDate);
    CHECK(statuses->statusFor(fixture->trip->id()) == Status::UpToDate);

    writeSource(caveSource, kTwoShots,
                QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds));
    sweep(fixture.get());

    CHECK(statuses->statusFor(fixture->cave->id()) == Status::Changed);
    CHECK(statuses->sourcePathFor(fixture->cave->id())
          == QFileInfo(caveSource).absoluteFilePath());
    // One owner's edit leaves the other owner alone.
    CHECK(statuses->statusFor(fixture->trip->id()) == Status::UpToDate);
}

TEST_CASE("an owner with no attachment has no status row",
          "[ExternalSourceStatus]")
{
    auto fixture = makeStatusProject(QStringLiteral("status-unattached"));
    drainPipelines(fixture.get());

    auto* statuses = statusModelOf(fixture.get());
    CHECK(statuses->rowCount() == 0);
    CHECK(statuses->statusFor(fixture->trip->id()) == Status::NoBreadcrumb);
}
