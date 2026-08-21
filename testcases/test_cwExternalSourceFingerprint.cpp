/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The N1 gate (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §5): every copy
// into the project stamps the breadcrumb entry with the fingerprint of
// the source's whole *include dependency set, and an entry written
// before fingerprints existed reads back as "none recorded".

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterlineAttach.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <QUuid>

// Std
#include <memory>

namespace {

// Wide margin so a hand-planted edit reads as a different mtime whatever
// the filesystem's granularity, which can be a full second.
constexpr int kEditIsNewerSeconds = 3600;

using SourceFingerprint = cwExternalSourceSettings::SourceFingerprint;
using SourceFileFingerprint = cwExternalSourceSettings::SourceFileFingerprint;

QString sha256Of(const QString& path)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(fileContents(path));
    return QString::fromLatin1(hash.result().toHex());
}

// A two-file Survex source outside the project: an entry file that
// *includes a sibling, so the fingerprint has to cover more than the
// file the user picked. Returns the entry file's path.
QString writeTwoFileSource(const QString& dir,
                           const QString& entryName,
                           const QString& blockName,
                           const QByteArray& includedShots)
{
    const QString includedName = blockName.toLower() + QStringLiteral("_inner.svx");
    const QString entryPath = QDir(dir).absoluteFilePath(entryName);
    const QString includedPath = QDir(dir).absoluteFilePath(includedName);

    const QDateTime mtime = QDateTime::currentDateTimeUtc();
    writeFileWithMtime(includedPath,
                       QByteArray("*begin ") + blockName.toLatin1() + "Inner\n"
                           + "*data normal from to tape compass clino\n"
                           + includedShots
                           + "*end " + blockName.toLatin1() + "Inner\n",
                       mtime);
    writeFileWithMtime(entryPath,
                       QByteArray("*begin ") + blockName.toLatin1() + "\n"
                           + "*include \"" + includedName.toLatin1() + "\"\n"
                           + "*end " + blockName.toLatin1() + "\n",
                       mtime);
    return entryPath;
}

// Checks the stored fingerprint against the files on disk: one record
// per dependency, in the scanner's order, each with the file's real
// size and SHA-256.
void checkFingerprintMatchesFiles(const SourceFingerprint& fingerprint,
                                  const QStringList& expectedFiles)
{
    REQUIRE(fingerprint.files.size() == expectedFiles.size());
    for (int i = 0; i < expectedFiles.size(); ++i) {
        const SourceFileFingerprint& record = fingerprint.files.at(i);
        const QString expectedPath = expectedFiles.at(i);
        INFO("record " << i << " path: " << record.path.toStdString());
        CHECK(QFileInfo(record.path).canonicalFilePath()
              == QFileInfo(expectedPath).canonicalFilePath());
        CHECK(record.size == QFileInfo(expectedPath).size());
        CHECK(record.sha256 == sha256Of(expectedPath));
        CHECK(record.lastModified
              == QFileInfo(expectedPath).lastModified().toMSecsSinceEpoch());
    }
}

void attachAndDrain(SavedProjectFixture* fixture, cwTrip* trip,
                    const QString& sourcePath)
{
    attachThroughManager(fixture, trip, sourcePath);
    drainPipelines(fixture);
}

std::unique_ptr<SavedProjectFixture> makeFingerprintProject(const QString& projectFileBase)
{
    return makeSavedProject(projectFileBase,
                            QStringLiteral("FingerprintCave"),
                            QStringLiteral("FingerprintTrip"));
}

QString sourceDirOf(const SavedProjectFixture* fixture, const QString& name)
{
    const QString dir = QDir(fixture->tempDir.path()).absoluteFilePath(name);
    REQUIRE(QDir().mkpath(dir));
    return dir;
}

} // namespace

TEST_CASE("attach stamps a fingerprint covering every file the source includes",
          "[ExternalSourceFingerprint]")
{
    auto fixture = makeFingerprintProject(QStringLiteral("fingerprint-attach"));

    const QString sourcePath =
        writeTwoFileSource(sourceDirOf(fixture.get(), QStringLiteral("source")),
                           QStringLiteral("entry.svx"), QStringLiteral("Alpha"),
                           QByteArray("A1 A2 5.0 0 0\n"));
    const QString includedPath =
        QDir(QFileInfo(sourcePath).absolutePath()).absoluteFilePath(
            QStringLiteral("alpha_inner.svx"));

    attachAndDrain(fixture.get(), fixture->trip, sourcePath);

    // A fresh store instance: the fingerprint is on disk, not in memory.
    const cwExternalSourceSettings stored;
    checkFingerprintMatchesFiles(stored.fingerprint(fixture->trip->id()),
                                 {sourcePath, includedPath});
    CHECK(stored.breadcrumbPath(fixture->trip->id())
          == QFileInfo(sourcePath).absoluteFilePath());
}

TEST_CASE("replace and reload re-stamp the fingerprint to the source they copied",
          "[ExternalSourceFingerprint]")
{
    auto fixture = makeFingerprintProject(QStringLiteral("fingerprint-restamp"));
    auto manager = managerOf(fixture.get());
    const QString sourceDir = sourceDirOf(fixture.get(), QStringLiteral("source"));

    const QString firstSource =
        writeTwoFileSource(sourceDir, QStringLiteral("first.svx"),
                           QStringLiteral("First"), QByteArray("F1 F2 5.0 0 0\n"));
    attachAndDrain(fixture.get(), fixture->trip, firstSource);
    const SourceFingerprint afterAttach =
        fixture->settings()->fingerprint(fixture->trip->id());
    REQUIRE_FALSE(afterAttach.isEmpty());

    SECTION("replace points the fingerprint at the new source's closure")
    {
        const QString secondSource =
            writeTwoFileSource(sourceDir, QStringLiteral("second.svx"),
                               QStringLiteral("Second"),
                               QByteArray("S1 S2 7.0 90 0\nS2 S3 3.0 45 0\n"));
        const QString secondIncluded =
            QDir(sourceDir).absoluteFilePath(QStringLiteral("second_inner.svx"));

        auto future = manager->replaceCenterline(fixture->trip, secondSource);
        REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
        REQUIRE_FALSE(future.result().hasError());
        drainPipelines(fixture.get());

        const cwExternalSourceSettings stored;
        checkFingerprintMatchesFiles(stored.fingerprint(fixture->trip->id()),
                                     {secondSource, secondIncluded});
    }

    SECTION("reload re-stamps the edited source it copied back in")
    {
        // Edit the included file, not the entry file: a dependency edit
        // is as real a change as an entry-file edit. The mtime is moved
        // deliberately - writing it now can land in the same
        // filesystem-granularity second as the first stamp.
        const QString includedPath =
            QDir(sourceDir).absoluteFilePath(QStringLiteral("first_inner.svx"));
        writeFileWithMtime(includedPath,
                           QByteArray("*begin FirstInner\n"
                                      "*data normal from to tape compass clino\n"
                                      "F1 F2 5.0 0 0\n"
                                      "F2 F3 9.25 180 0\n"
                                      "*end FirstInner\n"),
                           QDateTime::currentDateTimeUtc().addSecs(kEditIsNewerSeconds));

        REQUIRE(manager->canReloadFromSource(fixture->trip));
        auto future = manager->reloadFromSource(fixture->trip);
        REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
        REQUIRE_FALSE(future.result().hasError());
        drainPipelines(fixture.get());

        const cwExternalSourceSettings stored;
        const SourceFingerprint afterReload = stored.fingerprint(fixture->trip->id());
        checkFingerprintMatchesFiles(afterReload, {firstSource, includedPath});
        CHECK(afterReload != afterAttach);
    }
}

TEST_CASE("two owners keep their own fingerprints", "[ExternalSourceFingerprint]")
{
    auto fixture = makeFingerprintProject(QStringLiteral("fingerprint-two-owners"));
    fixture->cave->addTrip();
    cwTrip* secondTrip = fixture->cave->trip(1);
    secondTrip->setName(QStringLiteral("SecondTrip"));
    REQUIRE(secondTrip->id() != fixture->trip->id());

    const QString sourceDir = sourceDirOf(fixture.get(), QStringLiteral("source"));
    const QString firstSource =
        writeTwoFileSource(sourceDir, QStringLiteral("one.svx"),
                           QStringLiteral("One"), QByteArray("O1 O2 5.0 0 0\n"));
    const QString secondSource =
        writeTwoFileSource(sourceDir, QStringLiteral("two.svx"),
                           QStringLiteral("Two"),
                           QByteArray("T1 T2 4.0 0 0\nT2 T3 6.0 90 0\n"));

    attachAndDrain(fixture.get(), fixture->trip, firstSource);
    attachAndDrain(fixture.get(), secondTrip, secondSource);

    const cwExternalSourceSettings stored;
    checkFingerprintMatchesFiles(
        stored.fingerprint(fixture->trip->id()),
        {firstSource, QDir(sourceDir).absoluteFilePath(QStringLiteral("one_inner.svx"))});
    checkFingerprintMatchesFiles(
        stored.fingerprint(secondTrip->id()),
        {secondSource, QDir(sourceDir).absoluteFilePath(QStringLiteral("two_inner.svx"))});

    // Clearing one owner leaves the other's fingerprint alone.
    fixture->settings()->clearBreadcrumb(fixture->trip->id());
    CHECK(stored.fingerprint(fixture->trip->id()).isEmpty());
    CHECK_FALSE(stored.fingerprint(secondTrip->id()).isEmpty());
}

TEST_CASE("an entry written before fingerprints existed reads back as none recorded",
          "[ExternalSourceFingerprint]")
{
    const QUuid ownerId = QUuid::createUuid();
    const QString sourcePath = QStringLiteral("/projects/legacy/entry.svx");
    {
        QSettings raw;
        raw.setValue(QStringLiteral("externalCenterlineSources/")
                         + ownerId.toString(QUuid::WithoutBraces),
                     sourcePath);
    }

    const cwExternalSourceSettings settings;
    CHECK(settings.hasBreadcrumb(ownerId));
    CHECK(settings.breadcrumbPath(ownerId) == sourcePath);
    CHECK(settings.fingerprint(ownerId).isEmpty());

    // Same empty answer for an owner with no entry at all, so the
    // checker sees one "nothing to compare against" state.
    CHECK(settings.fingerprint(QUuid::createUuid()).isEmpty());
    CHECK(settings.fingerprint(QUuid()).isEmpty());
}

TEST_CASE("setBreadcrumbPath records a path with no fingerprint",
          "[ExternalSourceFingerprint]")
{
    cwExternalSourceSettings settings;
    const QUuid ownerId = QUuid::createUuid();

    SourceFingerprint planted;
    planted.files.append({QStringLiteral("/source/entry.svx"), 12, 34,
                          QStringLiteral("abc123")});
    settings.setBreadcrumb(ownerId, QStringLiteral("/source/entry.svx"), planted);
    REQUIRE(settings.fingerprint(ownerId) == planted);

    // The path-only writer describes a source nothing has fingerprinted,
    // so the stale records go with the old path.
    settings.setBreadcrumbPath(ownerId, QStringLiteral("/source/other.svx"));
    CHECK(settings.breadcrumbPath(ownerId) == QStringLiteral("/source/other.svx"));
    CHECK(settings.fingerprint(ownerId).isEmpty());
}
