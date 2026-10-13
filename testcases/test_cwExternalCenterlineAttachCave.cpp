/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalSourceSettings.h"
#include "cwFutureManagerModel.h"
#include "cwGeoReference.h"
#include "cwLinePlotManager.h"
#include "cwNodeWarningModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSaveLoad.h"
#include "cwSignalSpy.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVector3D>

// Std
#include <cmath>
#include <memory>

namespace {

std::unique_ptr<SavedProjectFixture> makeProject(const QString& projectFileBase)
{
    return makeSavedProject(projectFileBase,
                            QStringLiteral("NativeCave"),
                            QStringLiteral("NativeTrip"));
}

QString blocksFixture()
{
    return fixturePath(QStringLiteral("survex_blocks.svx"));
}

// Wide margin so a planted edit's mtime is unambiguous whatever the
// filesystem's granularity, which can be a full second.
constexpr int kEditIsOlderSeconds = 3600;

// A one-block source the source verbs can attach and re-copy. Only the
// tape length varies across an edit, so two equally long lengths keep
// the byte count and only an unconditional overwrite lands the edit.
QByteArray doghillSource(const QByteArray& tapeLength)
{
    const QByteArray header(
        "*begin doghill\n"
        "*fix d1 0 0 0\n"
        "*data normal from to tape compass clino\n");
    return header + "d1 d2 " + tapeLength + " 0 0\n" + "*end doghill\n";
}

//! Writes `content` into `dir` and hands back the absolute path.
QString writeSurvey(const QTemporaryDir& dir, const QString& name, const QByteArray& content)
{
    const QString path = QDir(dir.path()).absoluteFilePath(name);
    overwriteFile(path, content);
    return path;
}

//! Attaches \a source as a source root and waits for the solve it chains, so
//! the root's solved lookup has something to read.
cwCave* attachAndSolve(SavedProjectFixture* fixture, const QString& source)
{
    cwCave* root = attachSourceUnderRegion(fixture, source);
    drainPipelines(fixture);
    REQUIRE(tryWait(kAttachWaitMs, [root]() {
        return !root->scopedPositions().positions().isEmpty();
    }));
    return root;
}

} // namespace

TEST_CASE("uniqueCaveName sanitizes and dedupes against existing cave names",
          "[Attach]")
{
    cwCavingRegion region;
    addEmptyCave(region, QStringLiteral("dusk"));

    //Naming a cave after a file has to survive whatever the filename holds:
    //cwCave::setName silently rejects both an unsanitized name and a collision.
    CHECK(region.uniqueCaveName(QStringLiteral("upper:cave"))
          == QStringLiteral("upper_cave"));
    CHECK(region.uniqueCaveName(QStringLiteral("dusk")) == QStringLiteral("dusk 2"));
    CHECK(region.uniqueCaveName(QStringLiteral("DUSK")) == QStringLiteral("DUSK 2"));
    CHECK(region.uniqueCaveName(QStringLiteral("dawn")) == QStringLiteral("dawn"));
}


TEST_CASE("source replace refuses a source inside the project's data folder",
          "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("source-attach-in-project"));
    cwCave* cave = attachSourceUnderRegion(fixture.get(), blocksFixture());
    const int tripCountBefore = cave->allTrips().size();
    drainPipelines(fixture.get());

    const QString copyPath = fixture->saveLoad()
        ->externalCenterlineDir(cave)
        .absoluteFilePath(QStringLiteral("survex_blocks.svx"));
    REQUIRE(QFileInfo::exists(copyPath));

    auto future = managerOf(fixture.get())->replaceSource(cave, copyPath);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE(future.result().hasError());
    const QString message = future.result().errorMessage();
    INFO("refusal message: " << message.toStdString());
    CHECK(message.contains(QFileInfo(copyPath).absoluteFilePath()));
    CHECK(message.contains(QStringLiteral("Pick the original")));

    // The attachment the root already had is exactly as it was.
    CHECK(cave->externalCenterline().entryFile() == QStringLiteral("survex_blocks.svx"));
    CHECK(cave->allTrips().size() == tripCountBefore);
    drainPipelines(fixture.get());
}

TEST_CASE("cancelAttach before the scan lands leaves the target untouched",
          "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("source-attach-cancel"));
    cwSurveyNode* target = fixture->project->cavingRegion()->rootNode();
    const int childCountBefore = target->childNodeCount();
    const QUuid ownerId = target->id();

    auto future = managerOf(fixture.get())->attachSource(target, blocksFixture());
    // Same stack as the call, so the flag is provably set before the
    // attach's single consult point.
    managerOf(fixture.get())->cancelAttach(ownerId);
    CHECK(managerOf(fixture.get())->isOwnerBusy(ownerId));

    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    CHECK(future.isCanceled());

    settleEventLoop(kNothingHappensSettleMs);
    CHECK_FALSE(managerOf(fixture.get())->isOwnerBusy(ownerId));
    CHECK(target->childNodeCount() == childCountBefore);
    CHECK(fixture->project->cavingRegion()->externalSources()->count() == 0);
    CHECK(fixture->settings()->breadcrumbPath(ownerId).isEmpty());
}

TEST_CASE("source reload re-copies the remembered source and re-solves",
          "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("source-reload"));
    auto manager = managerOf(fixture.get());

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QByteArray sourceBytes = doghillSource("10.0");
    const QByteArray editedBytes = doghillSource("11.0");
    const QString source =
        writeSurvey(sourceDir, QStringLiteral("doghill.svx"), sourceBytes);

    cwCave* cave = attachSourceUnderRegion(fixture.get(), source);
    drainPipelines(fixture.get());
    REQUIRE(cave->allTrips().size() == 1);
    const QUuid doghillTripId = cave->allTrips().first()->id();

    const QString copyPath = fixture->saveLoad()
        ->externalCenterlineDir(cave)
        .absoluteFilePath(QStringLiteral("doghill.svx"));
    REQUIRE(fileContents(copyPath) == sourceBytes);

    // The source moves on after the copy was taken. Same byte count and an
    // mtime the copy already beats, so only an unconditional overwrite
    // lands the edit.
    REQUIRE(editedBytes.size() == sourceBytes.size());
    writeFileWithMtime(source, editedBytes,
                       QDateTime::currentDateTimeUtc().addSecs(-kEditIsOlderSeconds));

    CHECK(manager->canReloadFromSource(cave));

    cwSignalSpy solveSpy(manager, &cwExternalCenterlineManager::solveNeeded);
    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);

    auto reloadFuture = manager->reloadFromSource(cave);
    REQUIRE(AsyncFuture::waitForFinished(reloadFuture, kAttachWaitMs));
    REQUIRE_FALSE(reloadFuture.result().hasError());
    drainPipelines(fixture.get());

    CHECK(fileContents(copyPath) == editedBytes);
    CHECK(cave->externalCenterline().entryFile() == QStringLiteral("doghill.svx"));
    CHECK(solveSpy.count() > 0);

    // The derived trip the attach created is reconciled rather than
    // duplicated: the block set did not change.
    REQUIRE(cave->allTrips().size() == 1);
    CHECK(cave->allTrips().first()->id() == doghillTripId);

    REQUIRE(attachSpy.count() == 1);
    const auto report = attachSpy.at(0).at(0).value<cwExternalCenterlineReport>();
    CHECK(report.success());
    CHECK(report.ownerId() == cave->id());
}

TEST_CASE("source reload is refused when this machine has no source to copy",
          "[Attach]")
{
    auto fixture = makeProject(QStringLiteral("source-reload-refusals"));
    cwCave* cave = fixture->cave;
    auto manager = managerOf(fixture.get());

    // A node that was never attached has nothing to reload.
    CHECK_FALSE(manager->canReloadFromSource(cave));
    CHECK_FALSE(manager->canReloadFromSource(static_cast<cwSurveyNode*>(nullptr)));

    cwSignalSpy attachSpy(manager, &cwExternalCenterlineManager::attachCompleted);

    auto refused = manager->reloadFromSource(cave);
    REQUIRE(refused.isFinished());
    CHECK(refused.result().hasError());
    CHECK(refused.result().errorMessage().contains(QStringLiteral("no source file")));
    CHECK_FALSE(manager->isOwnerBusy(cave->id()));

    auto refusedNull = manager->reloadFromSource(static_cast<cwSurveyNode*>(nullptr));
    REQUIRE(refusedNull.isFinished());
    CHECK(refusedNull.result().hasError());
    CHECK(refusedNull.result().errorMessage()
              .contains(QStringLiteral("reload: source root is null")));

    // Both refusals report through the attach bridge, deferred.
    constexpr int kRefusalReportCount = 2;
    while (attachSpy.count() < kRefusalReportCount && attachSpy.wait(kAttachWaitMs)) {
    }
    REQUIRE(attachSpy.count() == kRefusalReportCount);

    const auto noSourceReport = attachSpy.at(0).at(0).value<cwExternalCenterlineReport>();
    CHECK_FALSE(noSourceReport.success());
    CHECK(noSourceReport.ownerId() == cave->id());
    CHECK(noSourceReport.errorMessage().contains(QStringLiteral("no source file")));

    const auto nullReport = attachSpy.at(1).at(0).value<cwExternalCenterlineReport>();
    CHECK_FALSE(nullReport.success());
    CHECK(nullReport.ownerId().isNull());
    CHECK(nullReport.errorMessage().contains(QStringLiteral("reload: source root is null")));

    CHECK(cave->externalCenterline().isEmpty());

    drainPipelines(fixture.get());
}

TEST_CASE("deleting a source root forgets its own remembered source",
          "[Attach][Breadcrumb]")
{
    auto fixture = makeProject(QStringLiteral("source-delete-breadcrumb"));
    cwCave* cave = attachSourceUnderRegion(fixture.get(), blocksFixture());
    drainPipelines(fixture.get());

    const QUuid caveId = cave->id();
    QList<QUuid> ownerIds({caveId});
    for (const cwSurveyNode* node : cave->allNodes()) {
        ownerIds.append(node->id());
    }
    for (const cwTrip* trip : cave->allTrips()) {
        ownerIds.append(trip->id());
    }
    REQUIRE(ownerIds.size() > 1);

    // What the store actually holds going in: the root's own breadcrumb, and
    // whatever the derived records picked up (none of their own).
    QList<QUuid> ownersWithBreadcrumb;
    for (const QUuid& ownerId : ownerIds) {
        if (fixture->settings()->hasBreadcrumb(ownerId)) {
            ownersWithBreadcrumb.append(ownerId);
        }
    }
    REQUIRE(ownersWithBreadcrumb.contains(caveId));
    REQUIRE(fixture->settings()->breadcrumbPath(caveId)
            == QFileInfo(blocksFixture()).absoluteFilePath());
    REQUIRE_FALSE(fixture->settings()->fingerprint(caveId).isEmpty());

    //The root and its subtree can be destroyed on the spot, so nothing may
    //touch them after this.
    auto future = managerOf(fixture.get())->deleteSourceRoot(cave);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    CHECK_FALSE(future.result().hasError());
    QCoreApplication::processEvents();

    for (const QUuid& ownerId : ownerIds) {
        INFO("owner: " << ownerId.toString().toStdString());
        CHECK_FALSE(fixture->settings()->hasBreadcrumb(ownerId));
        CHECK(fixture->settings()->breadcrumbPath(ownerId).isEmpty());
        CHECK(fixture->settings()->fingerprint(ownerId).isEmpty());
    }

    drainPipelines(fixture.get());
}

TEST_CASE("closing the project keeps a source root's remembered source",
          "[Attach][Breadcrumb]")
{
    auto fixture = makeProject(QStringLiteral("source-close-breadcrumb"));
    cwCave* cave = attachSourceUnderRegion(fixture.get(), blocksFixture());
    drainPipelines(fixture.get());

    const QUuid caveId = cave->id();
    const QString storedPath = fixture->settings()->breadcrumbPath(caveId);
    const auto storedFingerprint = fixture->settings()->fingerprint(caveId);
    REQUIRE_FALSE(storedPath.isEmpty());
    REQUIRE_FALSE(storedFingerprint.isEmpty());

    //Project close and load-replacing-the-region both run this. The closed
    //project still owns its source, so the breadcrumb must survive.
    fixture->project->cavingRegion()->clearCaves();
    QCoreApplication::processEvents();

    CHECK(fixture->settings()->hasBreadcrumb(caveId));
    CHECK(fixture->settings()->breadcrumbPath(caveId) == storedPath);
    CHECK(fixture->settings()->fingerprint(caveId) == storedFingerprint);

    drainPipelines(fixture.get());
}


namespace {

const QString kZoneNoDatumMak = QStringLiteral("compass_zone_no_datum.mak");
const QString kUtm13N = QStringLiteral("EPSG:32613");
// A local transverse Mercator centered near the fixture's fix.
const QString kBoulderFrameCS = QStringLiteral(
    "+proj=tmerc +lat_0=40.0254 +lon_0=-105.2581 +k=1 +x_0=0 +y_0=0 "
    "+datum=WGS84 +units=m +no_defs +type=crs");
// North American 1927, UTM zone 13N: the system the fixture's zone reads in
// under Compass's default datum.
const QString kNad27Utm13N = QStringLiteral("EPSG:26713");
// survex METRES_PER_FOOT.
constexpr double kMetersPerFoot = 0.3048;
// What the fixture's .mak fixes A1 at, in feet.
constexpr double kFixEastingFeet = 1568241.5;
constexpr double kFixNorthingFeet = 14534120.7;
constexpr double kFixElevationFeet = 5429.8;
// Absorbs the float a solved position is stored in.
constexpr double kFixMarginMeters = 5.0;
// The NAD27 -> WGS84 shift near the fixture's fix is ~210 m (PROJ cs2cs:
// 478000 4430000 EPSG:26713 -> 477953.925 4430208.847 EPSG:32613); any shift
// beyond this says the fix went through its datum.
constexpr double kMinimumDatumShiftMeters = 50.0;

QStringList defaultDatumWarnings(const cwSurveyNode* node)
{
    return node->errorModel()->errors()->warningMessagesForTypeIds(
        {static_cast<int>(cwErrorTypeId::AttachedFileDefaultDatum)});
}

//! Where the fixture's A1 belongs in \a frameCS: its .mak coordinate, read in
//! North American 1927 at UTM zone 13N.
cwGeoPoint fixtureA1In(const QString& frameCS)
{
    const auto point = cwCoordinateTransform::transformPoint(
        kNad27Utm13N, frameCS,
        cwGeoPoint(kFixEastingFeet * kMetersPerFoot, kFixNorthingFeet * kMetersPerFoot,
                   kFixElevationFeet * kMetersPerFoot));
    REQUIRE(point.has_value());
    return *point;
}

} // namespace

TEST_CASE("A Compass .mak that names a UTM zone but no datum is read in North American 1927",
          "[Attach][CS]")
{
    const bool localFrame = GENERATE(false, true);
    const QString frameCS = localFrame ? kBoulderFrameCS : kUtm13N;
    INFO("frame: " << frameCS.toStdString());

    auto fixture = makeProject(QStringLiteral("source-compass-default-datum"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    region->geoReference()->restore(cwGeoReference::Frozen, frameCS, {}, QString());

    const QString source = fixturePath(kZoneNoDatumMak);
    const QByteArray sourceBytes = fileContents(source);
    REQUIRE_FALSE(sourceBytes.contains('&'));

    cwCave* cave = attachAndSolve(fixture.get(), source);
    drainPipelines(fixture.get());

    // The project's copy is the source's bytes; the default datum lives in
    // the driver's translation of it.
    const QString copyPath =
        fixture->saveLoad()->externalCenterlineDir(cave).absoluteFilePath(kZoneNoDatumMak);
    CHECK(fileContents(copyPath) == sourceBytes);
    CHECK(fileContents(source) == sourceBytes);

    // The driver writes A1's fix under North American 1927, UTM zone 13N, so
    // cavern moves it through the datum shift into the frame.
    INFO("driver:\n" << fixture->rootData->linePlotManager()->driverSource().toStdString());
    INFO("cavern log:\n" << fixture->rootData->linePlotManager()->cavernLog().toStdString());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
    const cwStationPositionLookup& lookup = cave->scopedPositions();
    const QString a1 = QStringLiteral("compass_zone_no_datum.a1");
    REQUIRE(lookup.hasPosition(a1));
    const QVector3D placed = lookup.position(a1);
    const cwGeoPoint expected = fixtureA1In(frameCS);
    CHECK(placed.x() == Catch::Approx(expected.x).margin(kFixMarginMeters));
    CHECK(placed.y() == Catch::Approx(expected.y).margin(kFixMarginMeters));
    CHECK(placed.z() == Catch::Approx(expected.z).margin(kFixMarginMeters));
    if (!localFrame) {
        const double shift = std::hypot(expected.x - kFixEastingFeet * kMetersPerFoot,
                                        expected.y - kFixNorthingFeet * kMetersPerFoot);
        CHECK(shift > kMinimumDatumShiftMeters);
    }

    const QStringList warnings = defaultDatumWarnings(cave);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.first()
          == QStringLiteral("compass_zone_no_datum.mak names UTM zone 13N but no datum; CaveWhere "
                            "used Compass's default, North American 1927 — add a & line to the "
                            "file to choose another."));

    // A click on it opens the Fix Stations page, where the file's row shows
    // the system the default gave it.
    cwNodeWarningModel warningModel;
    warningModel.setNode(cave);
    REQUIRE(warningModel.rowCount() == 1);
    const QModelIndex warningRow = warningModel.index(0);
    CHECK(warningModel.data(warningRow, cwNodeWarningModel::MessageRole).toString()
          == warnings.first());
    CHECK(warningModel.data(warningRow, cwNodeWarningModel::TargetRole).toInt()
          == static_cast<int>(cwNodeWarningModel::Target::FixStationRow));

    SECTION("Reload copies the source byte for byte again")
    {
        auto reloadFuture = managerOf(fixture.get())->reloadFromSource(cave);
        REQUIRE(AsyncFuture::waitForFinished(reloadFuture, kAttachWaitMs));
        REQUIRE_FALSE(reloadFuture.result().hasError());
        drainPipelines(fixture.get());

        CHECK(fileContents(copyPath) == sourceBytes);
        CHECK(fileContents(source) == sourceBytes);
        CHECK(defaultDatumWarnings(cave).size() == 1);
    }
}

TEST_CASE("A Compass .mak is copied byte for byte", "[Attach][CS]")
{
    auto fixture = makeProject(QStringLiteral("source-compass-explicit-datum"));

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QByteArray datBytes = fileContents(fixturePath(QStringLiteral("compass_zone_no_datum.dat")));
    writeSurvey(sourceDir, QStringLiteral("compass_zone_no_datum.dat"), datBytes);
    const bool explicitDatum = GENERATE(false, true);
    const QByteArray datumLine =
        explicitDatum ? QByteArrayLiteral("&North American 1983;\n") : QByteArray();
    const QByteArray makBytes = datumLine + fileContents(fixturePath(kZoneNoDatumMak));
    const QString source = writeSurvey(sourceDir, kZoneNoDatumMak, makBytes);

    cwCave* cave = attachAndSolve(fixture.get(), source);
    drainPipelines(fixture.get());

    CHECK(fileContents(fixture->saveLoad()->externalCenterlineDir(cave).absoluteFilePath(
              kZoneNoDatumMak))
          == makBytes);
    CHECK(defaultDatumWarnings(cave).size() == (explicitDatum ? 0 : 1));
}

// The owner's report (2026-10-03): attach, save, quit, reopen, and the
// attached-file header no longer offers Reload. The fresh cwRootData stands
// in for the next launch; the per-machine store is the same one.
TEST_CASE("source reload stays offered after the project is saved and reopened",
          "[Attach][Reload]")
{
    auto fixture = makeProject(QStringLiteral("source-reload-reopen"));

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString source = QDir(sourceDir.path()).absoluteFilePath(QStringLiteral("blocks.svx"));
    REQUIRE(QFile::copy(blocksFixture(), source));

    cwCave* cave = attachSourceUnderRegion(fixture.get(), source);
    drainPipelines(fixture.get());
    const QUuid caveId = cave->id();
    REQUIRE(managerOf(fixture.get())->canReloadFromSource(cave));

    QTemporaryDir destinationDir;
    REQUIRE(destinationDir.isValid());
    QString destination;
    SECTION("bundled .cw") {
        destination = QDir(destinationDir.path()).filePath(QStringLiteral("reopened.cw"));
    }
    SECTION(".cwproj directory") {
        destination = QDir(destinationDir.path()).filePath(QStringLiteral("reopened.cwproj"));
    }

    REQUIRE(fixture->project->saveAs(destination));
    fixture->project->waitSaveToFinish();
    drainPipelines(fixture.get());
    const QString savedPath = fixture->project->filename();
    fixture.reset();

    auto freshRoot = std::make_unique<cwRootData>();
    freshRoot->project()->loadFile(savedPath);
    freshRoot->project()->waitLoadToFinish();
    freshRoot->linePlotManager()->waitToFinish();
    freshRoot->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    cwSurveyNode* reopened = nullptr;
    for (cwSurveyNode* candidate : freshRoot->region()->rootNode()->allNodes()) {
        if (candidate->id() == caveId) {
            reopened = candidate;
        }
    }
    REQUIRE(reopened != nullptr);
    REQUIRE_FALSE(reopened->externalCenterline().isEmpty());
    CHECK(freshRoot->externalSourceSettings()->breadcrumbPath(caveId)
          == QFileInfo(source).absoluteFilePath());
    REQUIRE(freshRoot->externalCenterlineManager()->canReloadFromSource(reopened));
}


namespace {

//! A tree shaped like a large Survex dataset: an entry beside one include and
//! below another, whose sanqiao/fixedpts/ents.svx fixes e1 with no *cs.
QString bareFixTreeFixture(const QString& entry)
{
    return fixturePath(QStringLiteral("survex_bare_fix_tree/north/%1").arg(entry));
}

const QString kBareFixStation = QStringLiteral("sanqiao.e1");
const QString kBareFixFile = QStringLiteral("sanqiao/fixedpts/ents.svx");
constexpr int kBareFixLine = 3;
constexpr double kBareFixEasting = 500100.0;
constexpr double kBareFixNorthing = 3000100.0;
constexpr double kBareFixElevation = 300.0;
// Placement is exact up to the float the solved lookup stores.
constexpr double kBareFixMarginMeters = 0.1;

//! The read-in note \a node's Fix Stations page shows on its row for
//! \a station, read through the concatenated table the page binds on the node
//! that lists the fix: \a node or one under it, or the attached rows of a
//! derived trip of one.
QString readInWarningInTable(const cwSurveyNode* node, const QString& station)
{
    for (const cwSurveyNode* holder : nodeAndDescendants(node)) {
        QList<const QAbstractItemModel*> tables({holder->fixStationTable()});
        for (const cwTrip* trip : holder->trips()) {
            tables.append(trip->attachedFixes());
        }
        for (const QAbstractItemModel* table : std::as_const(tables)) {
            const QHash<int, QByteArray> roles = table->roleNames();
            const int stationRole = roles.key(QByteArrayLiteral("stationName"), -1);
            const int warningRole = roles.key(QByteArrayLiteral("readInWarning"), -1);
            REQUIRE(stationRole >= 0);
            REQUIRE(warningRole >= 0);
            for (int row = 0; row < table->rowCount(); ++row) {
                const QModelIndex index = table->index(row, 0);
                if (table->data(index, stationRole).toString() == station) {
                    return table->data(index, warningRole).toString();
                }
            }
        }
    }
    FAIL("No fix station table in the subtree holds a row for " << station.toStdString());
    return QString();
}

} // namespace

TEST_CASE("An attachment on a Section lists its file's fixes",
          "[Attach][ExternalCenterline][cwAttachedFixModel]")
{
    auto fixture = makeProject(QStringLiteral("section-attached-fixes"));
    cwCave* cave = fixture->cave;
    cwCave* section = addChildNode(cave, QStringLiteral("Upper level"),
                                   cwSurveyNode::Kind::Folder);

    SECTION("attached as a source root under the Section")
    {
        fixture->project->waitSaveToFinish();
        REQUIRE(section->isSection());

        cwSurveyNode* root = attachSourceThroughManager(fixture.get(), section, blocksFixture());
        drainPipelines(fixture.get());
        REQUIRE(root->parentNode() == section);

        // The file fixes d1 inside *begin doghill, so the derived node lists it.
        const cwSurveyNode* doghill = childNodeNamed(root, QStringLiteral("doghill"));
        REQUIRE(doghill != nullptr);
        CHECK(attachedFixStations(root).isEmpty());
        CHECK(attachedFixStations(doghill) == QStringList({QStringLiteral("doghill.d1")}));
        CHECK(doghill->fixStationCount() == 1);
        CHECK(attachedFixStations(section).isEmpty());
        CHECK(attachedFixStations(cave).isEmpty());

        // The root's file is part of the solve too.
        INFO("driver:\n" << fixture->rootData->linePlotManager()->driverSource().toStdString());
        CHECK_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
        CHECK(root->scopedPositions().hasPosition(QStringLiteral("doghill.d1")));
    }

    SECTION("attached to a trip under the Section")
    {
        cwTrip* trip = addEmptyTrip(section, QStringLiteral("Dome climb"));
        fixture->project->waitSaveToFinish();

        attachThroughManager(fixture.get(), trip, blocksFixture());
        drainPipelines(fixture.get());

        CHECK(attachedFixStations(section)
              == QStringList({trip->scopePrefix() + QStringLiteral("doghill.d1")}));
        CHECK(attachedFixStations(cave).isEmpty());
        INFO("driver:\n" << fixture->rootData->linePlotManager()->driverSource().toStdString());
        CHECK_FALSE(fixture->rootData->linePlotManager()->hasSolveError());
        CHECK(section->scopedPositions().hasPosition(trip->scopePrefix()
                                                           + QStringLiteral("doghill.d1")));
    }
}

TEST_CASE("A file's *fix with no *cs reads in the system chosen for its attachment and says so",
          "[Attach][CS][ExternalCenterline][LinePlotManager]")
{
    auto fixture = makeProject(QStringLiteral("source-bare-fix-tree"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    region->geoReference()->restore(cwGeoReference::Frozen, kUtm13N, {}, QString());

    cwCave* cave = attachSourceUnderRegion(fixture.get(), bareFixTreeFixture(QStringLiteral("north.svx")));
    drainPipelines(fixture.get());

    cwLinePlotManager* linePlot = fixture->rootData->linePlotManager();

    {
        // With no system chosen the driver names none, and cavern refuses the
        // fix under the project's *cs out.
        INFO("driver:\n" << linePlot->driverSource().toStdString());
        INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
        REQUIRE(tryWait(kAttachWaitMs, [linePlot]() { return linePlot->hasSolveError(); }));
        CHECK(linePlot->solveErrorSummary().contains(QStringLiteral("input projection isn't")));
        const QStringList warnings = bareFixWarnings(cave);
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.first().contains(
            QStringLiteral("fixes %1 without a coordinate system; choose one on the Fix Stations "
                           "page or add *cs to the file.").arg(kBareFixStation)));
        CHECK(subtreeAttachedFixFor(cave, kBareFixStation).readInSystem.isEmpty());
        CHECK(readInWarningInTable(cave, kBareFixStation).isEmpty());
    }

    auto centerline = cave->externalCenterline();
    centerline.setFixCoordinateSystem(kUtm13N);
    cave->setExternalCenterline(centerline);
    REQUIRE(tryWait(kAttachWaitMs, [cave]() {
        return cave->scopedPositions().hasPosition(kBareFixStation);
    }));
    drainPipelines(fixture.get());

    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    REQUIRE_FALSE(linePlot->hasSolveError());
    CHECK(bareFixWarnings(cave).isEmpty());

    // The fix's numbers land as written in the chosen system, here the frame.
    const cwStationPositionLookup& lookup = cave->scopedPositions();
    const QVector3D placed = lookup.position(kBareFixStation);
    CHECK(placed.x() == Catch::Approx(kBareFixEasting).margin(kBareFixMarginMeters));
    CHECK(placed.y() == Catch::Approx(kBareFixNorthing).margin(kBareFixMarginMeters));
    CHECK(placed.z() == Catch::Approx(kBareFixElevation).margin(kBareFixMarginMeters));
    CHECK(lookup.hasPosition(QStringLiteral("houping.h2")));

    // The row names the system, the file, and the line that left the fix
    // without one.
    const cwAttachedFix fix = subtreeAttachedFixFor(cave, kBareFixStation);
    CHECK(fix.filePath == kBareFixFile);
    CHECK(fix.line == kBareFixLine);
    CHECK(fix.readInSystem == kUtm13N);
    const QString expected =
        QStringLiteral("Read in %1, the system chosen for north.svx: %2 line %3 names no "
                       "coordinate system for this fix.")
            .arg(kUtm13N, kBareFixFile).arg(kBareFixLine);
    CHECK(fix.readInWarning() == expected);
    CHECK(readInWarningInTable(cave, kBareFixStation) == expected);

    SECTION("a project that loses its frame takes one again from the chosen system")
    {
        region->geoReference()->clear();
        REQUIRE(tryWait(kAttachWaitMs, [region]() {
            return region->geoReference()->hasCoordinateSystem();
        }));
        drainPipelines(fixture.get());
        INFO("driver:\n" << linePlot->driverSource().toStdString());
        CHECK(region->geoReference()->anchor().kind == cwGeoReference::Anchor::Attachment);
        CHECK(region->geoReference()->anchor().id == cave->id());
        CHECK_FALSE(linePlot->hasSolveError());
        CHECK(subtreeAttachedFixFor(cave, kBareFixStation).readInSystem == kUtm13N);
        CHECK(bareFixWarnings(cave).isEmpty());
    }
}

TEST_CASE("A file's *fix with no *cs reads in the *cs out the file names itself",
          "[Attach][CS][ExternalCenterline][LinePlotManager]")
{
    auto fixture = makeProject(QStringLiteral("source-bare-fix-cs-out"));
    REQUIRE_FALSE(fixture->project->cavingRegion()->geoReference()->hasCoordinateSystem());

    cwCave* cave = attachAndSolve(fixture.get(), bareFixTreeFixture(QStringLiteral("north_cs_out.svx")));
    drainPipelines(fixture.get());

    // Without the read-in, houping's *cs out leaves sanqiao's fix with no
    // input system, and cavern fails the whole run on it.
    cwLinePlotManager* linePlot = fixture->rootData->linePlotManager();
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    REQUIRE_FALSE(linePlot->hasSolveError());
    CHECK(linePlot->driverSource().contains(QStringLiteral("*cs out UTM48N\n*cs UTM48N\n*include")));

    const cwStationPositionLookup& lookup = cave->scopedPositions();
    REQUIRE(lookup.hasPosition(kBareFixStation));
    CHECK(lookup.position(kBareFixStation).x()
          == Catch::Approx(kBareFixEasting).margin(kBareFixMarginMeters));
    CHECK(lookup.hasPosition(QStringLiteral("houping.h2")));

    // The harvest reads it the same way, so the file's stations are known.
    CHECK(cave->externalStations().contains(cwStation::canonicalKey(kBareFixStation)));

    const cwAttachedFix fix = subtreeAttachedFixFor(cave, kBareFixStation);
    CHECK(fix.readInWarning()
          == QStringLiteral("Read in UTM48N, the system the file's *cs out names: %1 line %2 "
                            "names no coordinate system for this fix.")
                 .arg(kBareFixFile).arg(kBareFixLine));
    // houping's fix names its own system.
    CHECK(subtreeAttachedFixFor(cave, QStringLiteral("houping.h1")).readInWarning().isEmpty());
}

TEST_CASE("A file whose every *fix names its *cs is attached as written",
          "[Attach][CS][ExternalCenterline][LinePlotManager]")
{
    auto fixture = makeProject(QStringLiteral("source-own-cs-fix"));

    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString source = writeSurvey(sourceDir, QStringLiteral("own_cs.svx"),
        "*cs out UTM48N\n"
        "*begin own\n"
        "*cs UTM48N\n"
        "*fix a1 500000 3000000 100\n"
        "*data normal from to tape compass clino\n"
        "a1 a2 10.0 0 0\n"
        "*end own\n");

    cwCave* cave = attachAndSolve(fixture.get(), source);
    drainPipelines(fixture.get());

    cwLinePlotManager* linePlot = fixture->rootData->linePlotManager();
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    REQUIRE_FALSE(linePlot->hasSolveError());
    // The driver puts no system of its own around the file.
    CHECK_FALSE(linePlot->driverSource().contains(QStringLiteral("*cs")));
    CHECK(managerOf(fixture.get())->solveInputs().externalOutputSystems.isEmpty());
    CHECK(subtreeAttachedFixFor(cave, QStringLiteral("own.a1")).readInWarning().isEmpty());
}
