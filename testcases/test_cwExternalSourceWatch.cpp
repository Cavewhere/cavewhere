/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The N3 gate (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §5): while the app
// runs, an edit to the file an attachment was copied from flips that
// attachment's status - and does nothing else. The copy in the project
// keeps its bytes, no solve is asked for, and the project stays
// unmodified (the B8 asymmetry).

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwExternalSourceStatusModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QByteArray>
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

QByteArray survexFile(const QByteArray& shots)
{
    return QByteArray("*begin Alpha\n"
                      "*data normal from to tape compass clino\n")
        + shots + "*end Alpha\n";
}

// A one-file Survex source outside the project, written with a chosen
// mtime so an edit is unambiguous. Returns the path.
QString writeSource(const QString& path,
                    const QByteArray& shots,
                    const QDateTime& mtime = QDateTime::currentDateTimeUtc())
{
    return writeFileWithMtime(path, survexFile(shots), mtime);
}

QDateTime laterThanNow()
{
    return QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds);
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

cwExternalSourceStatusModel* statusModelOf(SavedProjectFixture* fixture)
{
    return managerOf(fixture)->sourceStatusModel();
}

// The in-project copy of `ownerId`'s entry file - the file that must not
// move while the source changes underneath it.
QString copyPathOf(SavedProjectFixture* fixture, const QUuid& ownerId,
                   const QString& sourcePath)
{
    const QString dir = managerOf(fixture)->attachmentDir(ownerId);
    REQUIRE_FALSE(dir.isEmpty());
    const QString copyPath = QDir(dir).absoluteFilePath(QFileInfo(sourcePath).fileName());
    REQUIRE(QFile::exists(copyPath));
    return copyPath;
}

// Waits for the watcher event and the sweep behind it to land.
bool waitForStatus(SavedProjectFixture* fixture, const QUuid& ownerId, Status status)
{
    return tryWait(kWatcherWaitMs, [&]() {
        return statusModelOf(fixture)->statusFor(ownerId) == status;
    });
}

struct WatchedFixture {
    std::unique_ptr<SavedProjectFixture> fixture;
    QString sourcePath;
    QString copyPath;
    QUuid ownerId;
};

// A saved project with one attached trip, settled and unmodified: the
// state a user is in with the app open and their editor beside it.
WatchedFixture makeWatchedProject(const QString& projectFileBase)
{
    WatchedFixture watched;
    watched.fixture = makeSavedProject(projectFileBase,
                                       QStringLiteral("WatchCave"),
                                       QStringLiteral("WatchTrip"));
    watched.sourcePath =
        writeSource(sourcePathIn(sourceDirOf(watched.fixture.get()),
                                 QStringLiteral("entry.svx")),
                    kOneShot);

    attachThroughManager(watched.fixture.get(), watched.fixture->trip, watched.sourcePath);
    drainPipelines(watched.fixture.get());
    REQUIRE(watched.fixture->project->save());
    drainPipelines(watched.fixture.get());
    REQUIRE_FALSE(watched.fixture->project->modified());

    watched.ownerId = watched.fixture->trip->id();
    watched.copyPath = copyPathOf(watched.fixture.get(), watched.ownerId, watched.sourcePath);
    REQUIRE(statusModelOf(watched.fixture.get())->statusFor(watched.ownerId)
            == Status::UpToDate);
    REQUIRE(managerOf(watched.fixture.get())->watchedSourceFiles()
            == QStringList { QFileInfo(watched.sourcePath).canonicalFilePath() });
    return watched;
}

} // namespace

TEST_CASE("editing a watched source flips its status and leaves the project alone",
          "[ExternalSourceWatch]")
{
    auto watched = makeWatchedProject(QStringLiteral("watch-source-edit"));
    auto* fixture = watched.fixture.get();

    const QByteArray copiedBytes = fileContents(watched.copyPath);
    QSignalSpy solveSpy(managerOf(fixture), &cwExternalCenterlineManager::solveNeeded);

    SECTION("an in-place save")
    {
        writeSource(watched.sourcePath, kTwoShots, laterThanNow());
    }

    SECTION("a save by rename, the way most editors write")
    {
        const QString temporaryPath =
            sourcePathIn(sourceDirOf(fixture), QStringLiteral("entry.svx.tmp"));
        writeSource(temporaryPath, kTwoShots, laterThanNow());
        REQUIRE(QFile::remove(watched.sourcePath));
        REQUIRE(QFile::rename(temporaryPath, watched.sourcePath));
    }

    CHECK(waitForStatus(fixture, watched.ownerId, Status::Changed));

    // The hard line: the source moved on, and nothing on this side of the
    // project did. The copy is still what the solve reads, no solve was
    // asked for, and the user has nothing unsaved they did not ask for.
    CHECK(fileContents(watched.copyPath) == copiedBytes);
    CHECK(solveSpy.count() == 0);
    CHECK_FALSE(fixture->project->modified());

    // The path stays armed through the change, so the next edit is
    // noticed too.
    CHECK(managerOf(fixture)->watchedSourceFiles()
          == QStringList { QFileInfo(watched.sourcePath).canonicalFilePath() });
}

TEST_CASE("a source deleted while the app runs reads as missing, and its return recovers",
          "[ExternalSourceWatch]")
{
    auto watched = makeWatchedProject(QStringLiteral("watch-source-deleted"));
    auto* fixture = watched.fixture.get();

    const QByteArray copiedBytes = fileContents(watched.copyPath);
    QSignalSpy solveSpy(managerOf(fixture), &cwExternalCenterlineManager::solveNeeded);

    REQUIRE(QFile::remove(watched.sourcePath));
    CHECK(waitForStatus(fixture, watched.ownerId, Status::SourceMissing));
    CHECK(managerOf(fixture)->watchedSourceFiles().isEmpty());

    // The directory the file lived in is still watched, which is what
    // notices it coming back.
    writeSource(watched.sourcePath, kOneShot, laterThanNow());
    CHECK(waitForStatus(fixture, watched.ownerId, Status::UpToDate));
    CHECK(managerOf(fixture)->watchedSourceFiles()
          == QStringList { QFileInfo(watched.sourcePath).canonicalFilePath() });

    CHECK(fileContents(watched.copyPath) == copiedBytes);
    CHECK(solveSpy.count() == 0);
    CHECK_FALSE(fixture->project->modified());
}

TEST_CASE("re-stamping a breadcrumb moves the watch onto the file it now names",
          "[ExternalSourceWatch]")
{
    auto watched = makeWatchedProject(QStringLiteral("watch-breadcrumb-restamp"));
    auto* fixture = watched.fixture.get();

    const QString replacementPath =
        writeSource(sourcePathIn(sourceDirOf(fixture), QStringLiteral("replacement.svx")),
                    kOneShot);
    auto replaceFuture = managerOf(fixture)->replaceCenterline(fixture->trip, replacementPath);
    REQUIRE(AsyncFuture::waitForFinished(replaceFuture, kAttachWaitMs));
    REQUIRE_FALSE(replaceFuture.result().hasError());
    drainPipelines(fixture);

    CHECK(managerOf(fixture)->watchedSourceFiles()
          == QStringList { QFileInfo(replacementPath).canonicalFilePath() });
    CHECK(statusModelOf(fixture)->statusFor(watched.ownerId) == Status::UpToDate);

    // The file the attachment no longer comes from says nothing anymore,
    // while the one it does is heard.
    writeSource(watched.sourcePath, kTwoShots, laterThanNow());
    settleEventLoop(kNothingHappensSettleMs);
    CHECK(statusModelOf(fixture)->statusFor(watched.ownerId) == Status::UpToDate);

    writeSource(replacementPath, kTwoShots, laterThanNow());
    CHECK(waitForStatus(fixture, watched.ownerId, Status::Changed));
}
