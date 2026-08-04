/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// B7 (plans/EXTERNAL_FILE_PHASE2.html §16): an in-project attachment whose
// *include reaches outside the project's data root cannot travel with the
// project, so it is dropped from the solve rather than exported. The rule is
// deliberately about the data root and not the attachment dir — an *include
// pointing at another owner's attachment still resolves on every machine that
// has the project, and stays legal.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalCenterlineSync.h"
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwFutureManagerModel.h"
#include "cwStationHandle.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwSurvexExporterRegion.h"
#include "cwTrip.h"

// Tests
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSet>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>

// Std
#include <memory>

namespace {

// From <dataRoot>/<Cave>/trips/<Trip>/external-centerline, five levels of
// climbing clears the data root and lands in the project root beside it.
// Four stops exactly on the data root. The pair is what pins the boundary to
// dataRootDir() rather than projectRootDir().
constexpr auto kEscapesDataRoot = "../../../../../outside.svx";
constexpr auto kStaysInDataRoot = "../../../../sibling.svx";

const QByteArray kIncludedSurvey =
    "*begin Included\n"
    "*fix I1 0 0 0\n"
    "*data normal from to tape compass clino\n"
    "I1 I2 10.0 0 0\n"
    "*end Included\n";

QByteArray entryIncluding(const char* relativePath)
{
    return QByteArray("*begin Entry\n*include \"")
        + relativePath
        + "\"\n*end Entry\n";
}

struct ContainmentFixture {
    QTemporaryDir tempDir;
    std::unique_ptr<cwRootData> rootData;
    cwProject* project = nullptr;
    cwTrip* trip = nullptr;
    QString attachmentDir;
    QString dataRootDir;
    // The directory holding the .cwproj file, one level above the data
    // root. Only the escaping fixture needs it, to prove the escape lands
    // between the two boundaries rather than off the end of the tree.
    QString projectRootDir;
    QString entryPath;
    // Where the entry's *include actually landed, resolved against the
    // attachment dir the same way the scanner resolves it.
    QString includeTargetPath;
};

// overwriteFile with the parent directory created first, since these
// fixtures write include targets that sit outside any existing tree.
void writeFile(const QString& path, const QByteArray& content)
{
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    overwriteFile(path, content);
}

// A saved project holding one trip whose attachment entry file *includes
// `includeTarget`, with the included survey written at `includeLandsAt`
// (resolved against the attachment dir) so the scanner can follow it.
std::unique_ptr<ContainmentFixture> makeFixture(const char* includeTarget)
{
    auto fixture = std::make_unique<ContainmentFixture>();
    REQUIRE(fixture->tempDir.isValid());

    fixture->rootData = std::make_unique<cwRootData>();
    fixture->project = fixture->rootData->project();

    auto region = fixture->project->cavingRegion();
    region->addCave();
    cwCave* cave = region->cave(0);
    cave->setName(QStringLiteral("Containment"));
    cave->addTrip();
    fixture->trip = cave->trip(0);
    fixture->trip->setName(QStringLiteral("Attached"));
    fixture->trip->setExternalCenterline(cwExternalCenterline(QStringLiteral("entry.svx")));

    const QString projectPath =
        QDir(fixture->tempDir.path()).filePath(QStringLiteral("containment.cwproj"));
    REQUIRE(fixture->project->saveAs(projectPath));
    fixture->project->waitSaveToFinish();

    cwSaveLoad* saveLoad = fixture->project->saveLoad();
    fixture->attachmentDir =
        saveLoad->externalCenterlineDir(fixture->trip).absolutePath();
    fixture->dataRootDir = saveLoad->dataRootDir().absolutePath();
    // cwSaveLoad::projectRootDir() is private; the .cwproj file's own
    // directory is the same place.
    fixture->projectRootDir = QFileInfo(fixture->project->filename()).absolutePath();
    fixture->entryPath = QDir(fixture->attachmentDir).filePath(QStringLiteral("entry.svx"));
    fixture->includeTargetPath = QDir::cleanPath(
        QDir(fixture->attachmentDir).absoluteFilePath(QString::fromLatin1(includeTarget)));

    writeFile(fixture->entryPath, entryIncluding(includeTarget));
    // The scanner only reports dependencies that resolve, so the include
    // target has to exist for the containment check to see it at all.
    writeFile(fixture->includeTargetPath, kIncludedSurvey);

    // The scanner stores canonical dependency paths, and a QTemporaryDir on
    // macOS sits under /var, which is a symlink to /private/var. Comparing
    // watchedFiles() against uncanonicalized paths would make every
    // membership assertion here false — including the negative ones, which
    // would then pass without testing anything.
    fixture->entryPath = QFileInfo(fixture->entryPath).canonicalFilePath();
    fixture->includeTargetPath = QFileInfo(fixture->includeTargetPath).canonicalFilePath();
    fixture->dataRootDir = QFileInfo(fixture->dataRootDir).canonicalFilePath();
    fixture->projectRootDir = QFileInfo(fixture->projectRootDir).canonicalFilePath();
    REQUIRE_FALSE(fixture->entryPath.isEmpty());
    REQUIRE_FALSE(fixture->includeTargetPath.isEmpty());
    REQUIRE_FALSE(fixture->projectRootDir.isEmpty());

    fixture->rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    return fixture;
}

// Runs the manager over the fixture's project with saveLoad wired, which is
// what supplies the containment boundary.
void scanFixture(cwLinePlotManager& manager, ContainmentFixture& fixture)
{
    manager.externalCenterlineManager()->setSaveLoad(fixture.project->saveLoad());
    manager.setRegion(fixture.project->cavingRegion());
    manager.waitToFinish();
}

} // namespace

// Also tagged [Sync] because isContainedIn lives in cwExternalCenterlineSync
// alongside computePlan, whose unit tests run under that tag.
TEST_CASE("isContainedIn answers where a path lives relative to a boundary",
          "[ExternalCenterline][Containment][Sync]")
{
    using cwExternalCenterlineSync::isContainedIn;

    QTemporaryDir root;
    REQUIRE(root.isValid());

    const QString boundary = tempSubdir(root, QStringLiteral("data"));
    const QString inside = QDir(boundary).filePath(QStringLiteral("nested/file.svx"));
    writeFile(inside, kIncludedSurvey);

    const QString outside = QDir(root.path()).filePath(QStringLiteral("outside.svx"));
    writeFile(outside, kIncludedSurvey);

    CHECK(isContainedIn(inside, boundary));
    CHECK(isContainedIn(boundary, boundary));
    CHECK_FALSE(isContainedIn(outside, boundary));

    // Lexical climbing out of the boundary is the shape a hand-edited
    // *include takes.
    CHECK_FALSE(isContainedIn(QDir(boundary).filePath(QStringLiteral("../outside.svx")),
                              boundary));

    // A file that is not on disk still answers by location; the check has to
    // work before anything has been copied in.
    CHECK(isContainedIn(QDir(boundary).filePath(QStringLiteral("absent.svx")), boundary));
    CHECK_FALSE(isContainedIn(QDir(root.path()).filePath(QStringLiteral("absent.svx")),
                              boundary));

    // A boundary reached through a symlink has to compare equal to the
    // scanner's canonical dependency paths — on macOS a QTemporaryDir under
    // /tmp is exactly this case, and getting it wrong makes every dependency
    // look like an escape.
    const QString linkedBoundary = QDir(root.path()).filePath(QStringLiteral("link-to-data"));
    REQUIRE(QFile::link(boundary, linkedBoundary));
    CHECK(isContainedIn(inside, linkedBoundary));
    CHECK_FALSE(isContainedIn(outside, linkedBoundary));
    // A missing file under the symlinked boundary: the deterministic form of
    // the case above, which on macOS is otherwise only reached incidentally
    // through /var being a symlink.
    CHECK(isContainedIn(QDir(linkedBoundary).filePath(QStringLiteral("absent.svx")),
                        boundary));

    // A ".." that crosses a symlink must be resolved by the filesystem, not
    // by string math. `escape` points out of the boundary, so "escape/.."
    // is the symlink's parent — outside — even though it reads as `boundary`
    // if the ".." is collapsed textually first.
    const QString escapeLink = QDir(boundary).filePath(QStringLiteral("escape"));
    REQUIRE(QFile::link(root.path(), escapeLink));
    CHECK_FALSE(isContainedIn(QDir(boundary).filePath(QStringLiteral("escape/../absent.svx")),
                              boundary));

    CHECK_FALSE(isContainedIn(QString(), boundary));
    CHECK_FALSE(isContainedIn(inside, QString()));
}

TEST_CASE("An *include reaching outside the data root drops the attachment from the solve",
          "[ExternalCenterline][Containment]")
{
    auto fixture = makeFixture(kEscapesDataRoot);

    const QString escapingPath = fixture->includeTargetPath;
    REQUIRE(QFile::exists(escapingPath));
    // The escape clears the data root while staying inside the project root,
    // which is what makes this a test of the boundary and not of the project.
    // Both halves are asserted: without the second, a layout change that
    // moved the data root would push this path off the end of the tree and
    // the test would still pass while no longer testing the boundary.
    REQUIRE_FALSE(cwExternalCenterlineSync::isContainedIn(escapingPath, fixture->dataRootDir));
    REQUIRE(cwExternalCenterlineSync::isContainedIn(escapingPath, fixture->projectRootDir));

    cwLinePlotManager manager;
    scanFixture(manager, *fixture);

    auto* external = manager.externalCenterlineManager();

    // Excluded from the solve, so the driver writes no *include for it.
    CHECK(external->solveInputs().excludedExternalOwners.contains(fixture->trip->id()));

    // Named in the banner, with the offending path in the message so the user
    // can find it.
    const QString error = fixture->trip->externalStationsError();
    CHECK_FALSE(error.isEmpty());
    CHECK(error.contains(QStringLiteral("outside.svx")));

    // The escaping file stays out of the watch set — the project cannot use
    // it, so an edit to it must not re-solve. The entry file stays IN, and
    // that asymmetry is the point: the error message asks the user to edit
    // the entry, and watching it is the only thing that makes the fix take
    // effect without reopening the project.
    const QStringList watched = external->watchedFiles();
    CHECK_FALSE(watched.contains(escapingPath));
    CHECK(watched.contains(fixture->entryPath));

    // No station names reached the trip. The harvest is skipped rather than
    // run-and-discarded, because it invokes cavern and cavern would follow
    // the *include out of the project.
    CHECK(fixture->trip->externalStations().isEmpty());

    // Facts about the file are still reported even though it is excluded —
    // the row is what the user clicks to find the offending attachment.
    CHECK(external->fileOwnsDeclination(fixture->trip->id()) == false);
}

TEST_CASE("An *include that stays inside the data root keeps the attachment",
          "[ExternalCenterline][Containment]")
{
    auto fixture = makeFixture(kStaysInDataRoot);

    const QString siblingPath = fixture->includeTargetPath;
    REQUIRE(QFile::exists(siblingPath));
    REQUIRE(siblingPath == QDir(fixture->dataRootDir).filePath(QStringLiteral("sibling.svx")));
    REQUIRE(cwExternalCenterlineSync::isContainedIn(siblingPath, fixture->dataRootDir));

    cwLinePlotManager manager;
    scanFixture(manager, *fixture);

    auto* external = manager.externalCenterlineManager();

    // Reaching outside the attachment dir is legal as long as the target
    // travels with the project.
    CHECK_FALSE(external->solveInputs().excludedExternalOwners.contains(fixture->trip->id()));
    CHECK(fixture->trip->externalStationsError().isEmpty());

    const QStringList watched = external->watchedFiles();
    CHECK(watched.contains(siblingPath));
    CHECK(watched.contains(fixture->entryPath));
}

TEST_CASE("Fixing the *include clears the exclusion without reopening the project",
          "[ExternalCenterline][Containment]")
{
    auto fixture = makeFixture(kEscapesDataRoot);

    cwLinePlotManager manager;
    scanFixture(manager, *fixture);

    auto* external = manager.externalCenterlineManager();
    REQUIRE(external->solveInputs().excludedExternalOwners.contains(fixture->trip->id()));
    REQUIRE_FALSE(fixture->trip->externalStationsError().isEmpty());

    // Do what the error message asks: point the *include at a copy inside
    // the project. The entry file is watched, so this is the whole user
    // action — no reopen, no re-attach.
    const QString localCopy =
        QDir(fixture->attachmentDir).filePath(QStringLiteral("included.svx"));
    writeFile(localCopy, kIncludedSurvey);
    overwriteFile(fixture->entryPath, entryIncluding("included.svx"));

    // The watcher fires on the edit and schedules the recompute that clears
    // the exclusion. Waiting on the outcome rather than on a fixed delay,
    // since the watcher's delivery time is the platform's business.
    REQUIRE(tryWait(kWatcherWaitMs, [&] {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kInnerPollEventsMs);
        return !external->solveInputs().excludedExternalOwners.contains(fixture->trip->id());
    }));

    manager.waitToFinish();
    CHECK(fixture->trip->externalStationsError().isEmpty());
    // Back in the solve means the names are readable again.
    CHECK_FALSE(fixture->trip->externalStations().isEmpty());
}

TEST_CASE("An excluded owner costs its own survey, not the whole region export",
          "[ExternalCenterline][Containment]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwSurvexExporterRegion::Options options;
    // The attachment dir is deliberately withheld as well as the owner being
    // excluded. That is the real shape — an owner dropped before its dir was
    // ever published — and it is what makes this test bite: routing exclusion
    // through writeExternalInclude's absent-key path returns false, which
    // aborts the entire region export and takes every other cave down with
    // it. With the dir present, this assertion would hold either way.
    options.excludedExternalOwners = QSet<QUuid> { setup.attached->id() };

    const QString outputPath = QDir(tempRoot.path()).filePath(QStringLiteral("driver.svx"));
    const auto result =
        cwSurvexExporterRegion::exportRegion(region.data(), outputPath, options);

    REQUIRE_FALSE(result.hasError());

    const QByteArray driver = fileContents(outputPath);
    CHECK_FALSE(driver.contains("*include"));
    // Nothing of the excluded trip is emitted — not the *include, and not
    // the *begin wrapper that would otherwise be left dangling.
    CHECK_FALSE(driver.contains(tripScopeLabel(setup.attached).toUtf8()));
    // The native trip in the same cave still exported.
    CHECK(driver.contains("A1"));
}

TEST_CASE("An excluded owner's ties are dropped rather than left dangling",
          "[ExternalCenterline][Containment]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    // Tie the attached trip to the native cave's A1, then exclude the trip.
    setup.cave->equates()->appendEquate(cwEquate({
        cwStationHandle(cwStationHandle::Trip, setup.attached->id(),
                        QStringLiteral("simple.a1")),
        cwStationHandle(cwStationHandle::NativeCave, setup.cave->id(),
                        QStringLiteral("A1"))
    }));

    cwSurvexExporterRegion::Options options;
    options.tripAttachmentDirs = setup.tripDirs;
    options.excludedExternalOwners = QSet<QUuid> { setup.attached->id() };

    const QString outputPath = QDir(tempRoot.path()).filePath(QStringLiteral("driver.svx"));
    const auto result =
        cwSurvexExporterRegion::exportRegion(region.data(), outputPath, options);
    REQUIRE_FALSE(result.hasError());

    // Cavern binds an unknown *equate operand by CREATING the station and a
    // zero-length leg rather than rejecting the name, so emitting this tie
    // would put a phantom station under the excluded trip's label at A1's
    // coordinate — and the decode would route it back to the survey that was
    // deliberately left out.
    const QByteArray driver = fileContents(outputPath);
    CHECK_FALSE(driver.contains("*equate"));
}

TEST_CASE("An excluded cave-level attachment emits no empty survey block",
          "[ExternalCenterline][Containment]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Escaped"));
    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("entry.svx")));

    cwSurvexExporterRegion::Options options;
    options.excludedExternalOwners = QSet<QUuid> { cave->id() };

    const QString outputPath = QDir(tempRoot.path()).filePath(QStringLiteral("driver.svx"));
    const auto result =
        cwSurvexExporterRegion::exportRegion(region.data(), outputPath, options);
    REQUIRE_FALSE(result.hasError());

    // An empty *begin/*end pair would be worse than nothing: cavern fatals
    // with "No survey data" on a driver that declares only empty scopes, so
    // the one excluded cave would cost the whole solve rather than itself.
    const QByteArray driver = fileContents(outputPath);
    CHECK_FALSE(driver.contains("*include"));
    CHECK_FALSE(driver.contains("Escaped"));
}
