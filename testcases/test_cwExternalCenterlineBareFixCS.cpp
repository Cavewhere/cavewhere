/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

// Our
#include "cwAttachedFixModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwStationPositionLookup.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "ExternalCenterlineTestHelpers.h"

// Async
#include <asyncfuture.h>

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector3D>

// Std
#include <algorithm>
#include <memory>

namespace {

const QString kBareFixFixture = QStringLiteral("survex_bare_fix/bare_fix.svx");
const QString kBareFixFileName = QStringLiteral("bare_fix.svx");
// WGS 84 / UTM zone 48N: the project's frame, and the system the user picks
// for the file's bare fix, by its Survex name.
const QString kUtm48NFrame = QStringLiteral("EPSG:32648");
const QString kUtm48N = QStringLiteral("UTM48N");
// The fixture's *begin bare block and its one *fix.
const QString kFixedStation = QStringLiteral("bare.A");
constexpr double kFixEasting = 500000.0;
constexpr double kFixNorthing = 3000000.0;
constexpr double kFixElevation = 100.0;
constexpr double kPlacementMarginMeters = 1.0;
// cavern's error 438, the one a bare *fix under a *cs out raises.
const QString kInputProjectionError = QStringLiteral("input projection isn't");

// A file whose one fix carries its own *cs, so it solves with no choice made.
const QString kOwnCsFixture = QStringLiteral("survex_native_cs_scope/own_cs_fix.svx");
const QString kOwnCsFileName = QStringLiteral("own_cs_fix.svx");

// Native fixes in the project's frame, clear of the files' fixes.
const QString kMainFixStation = QStringLiteral("m1");
constexpr double kMainFixEasting = 500100.0;
const QString kSubFixStation = QStringLiteral("s1");
constexpr double kSubFixEasting = 500300.0;

cwLinePlotManager* linePlotOf(SavedProjectFixture* fixture)
{
    return fixture->rootData->linePlotManager();
}

//! Waits until the last solve's driver includes \a fileName, so the solve the
//! attach chains has run whether cavern accepted the driver or not.
bool waitForDriverIncludingFixture(SavedProjectFixture* fixture,
                                   const QString& fileName = kBareFixFileName)
{
    const bool included = tryWait(kAttachWaitMs, [fixture, &fileName]() {
        return linePlotOf(fixture)->driverSource().contains(fileName);
    });
    drainPipelines(fixture);
    return included;
}

//! The input *cs line in force at the driver's *include of \a fileName, the
//! way cavern scopes it: a *begin starts with the system its parent has in
//! force, and its *end restores the parent's. "*cs out" lines are skipped.
//! Empty when no input system is in force there.
QString inputSystemLineAtInclude(const QString& driver,
                                 const QString& fileName = kBareFixFileName)
{
    const QStringList lines = driver.split(QLatin1Char('\n'));
    QStringList inForceByBlock = {QString()};
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("*include")) && trimmed.contains(fileName)) {
            return inForceByBlock.constLast();
        }
        if (trimmed.startsWith(QStringLiteral("*begin"))) {
            inForceByBlock.append(inForceByBlock.constLast());
        } else if (trimmed.startsWith(QStringLiteral("*end")) && inForceByBlock.size() > 1) {
            inForceByBlock.removeLast();
        } else if (trimmed.startsWith(QStringLiteral("*cs "))
                   && !trimmed.startsWith(QStringLiteral("*cs out"))) {
            inForceByBlock.last() = trimmed;
        }
    }
    return QString();
}

//! The lines of \a driver's "*begin <label>" block, from that line through
//! its matching *end. Empty when the driver opens no such block.
QStringList blockLines(const QString& driver, const QString& label)
{
    const QStringList lines = driver.split(QLatin1Char('\n'));
    const QString beginLine = QStringLiteral("*begin %1 ").arg(label);
    QStringList block;
    int depth = 0;
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (depth == 0) {
            if (trimmed.startsWith(beginLine)) {
                block.append(trimmed);
                depth = 1;
            }
            continue;
        }
        block.append(trimmed);
        if (trimmed.startsWith(QStringLiteral("*begin"))) {
            ++depth;
        } else if (trimmed.startsWith(QStringLiteral("*end"))) {
            --depth;
            if (depth == 0) {
                return block;
            }
        }
    }
    return block;
}

//! Where the fixture's fix lands in \a frameCS, read in UTM zone 48N.
cwGeoPoint fixtureFixIn(const QString& frameCS)
{
    const auto point = cwCoordinateTransform::transformPoint(
        kUtm48NFrame, frameCS, cwGeoPoint(kFixEasting, kFixNorthing, kFixElevation));
    REQUIRE(point.has_value());
    return *point;
}

std::unique_ptr<SavedProjectFixture> makeProject(const QString& projectFileBase, bool georeferenced)
{
    auto fixture = makeSavedProject(projectFileBase, QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    if (georeferenced) {
        fixture->project->cavingRegion()->geoReference()->restore(cwGeoReference::Frozen,
                                                                  kUtm48NFrame, {}, QString());
    }
    return fixture;
}

//! \a Owner is a cwSurveyNode or a cwTrip; both carry an attachment.
template<typename Owner>
void chooseFixSystem(Owner* owner, const QString& system)
{
    auto centerline = owner->externalCenterline();
    centerline.setFixCoordinateSystem(system);
    owner->setExternalCenterline(centerline);
}

//! A native fix on \a node at \a station, entered in the project's frame.
void addNativeFix(cwSurveyNode* node, const QString& station, double easting)
{
    cwFixStation fix;
    fix.setStationName(station);
    fix.setInputCS(kUtm48NFrame);
    fix.setCoordinate(easting, kFixNorthing, kFixElevation);
    node->fixStations()->appendFixStation(fix);
}

//! The project's one cave, "Main": a native trip m1-m2 whose auto declination
//! is \a autoDeclination, and a native fix on m1 in the project's frame.
cwCave* setUpMainWithNativeFix(SavedProjectFixture* fixture, bool autoDeclination)
{
    cwCave* main = fixture->cave;
    main->setName(QStringLiteral("Main"));
    cwTrip* survey = addNativeTripWithShot(main, QStringLiteral("Main survey"), kMainFixStation,
                                           QStringLiteral("m2"));
    survey->calibrations()->setAutoDeclination(autoDeclination);
    addNativeFix(main, kMainFixStation, kMainFixEasting);
    return main;
}

} // namespace

TEST_CASE("A bare file fix in a georeferenced project with no system chosen fails the solve and warns",
          "[AttachInclude][BareFixCS]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-unset"), true);
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());

    // CaveWhere puts no system of its own around the file's bare fix.
    CHECK(inputSystemLineAtInclude(linePlot->driverSource()).isEmpty());
    CHECK(linePlot->hasSolveError());
    CHECK(linePlot->solveErrorSummary().contains(kInputProjectionError));

    CHECK(bareFixWarnings(root).size() == 1);
    const QString readInSystem = subtreeAttachedFixFor(root, kFixedStation).readInSystem;
    INFO("readInSystem: " << readInSystem.toStdString());
    CHECK(readInSystem.isEmpty());
}

TEST_CASE("A bare file fix reads in the system chosen for its attachment",
          "[AttachInclude][BareFixCS]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-chosen"), true);
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    chooseFixSystem(root, kUtm48N);
    REQUIRE(root->externalCenterline().fixCoordinateSystem() == kUtm48N);

    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    const QString chosenLine = QStringLiteral("*cs %1").arg(kUtm48N);
    CHECK(tryWait(kAttachWaitMs, [&]() {
        return inputSystemLineAtInclude(linePlot->driverSource()) == chosenLine;
    }));
    drainPipelines(fixture.get());

    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    const QString lineAtInclude = inputSystemLineAtInclude(linePlot->driverSource());
    INFO("input *cs at the include: " << lineAtInclude.toStdString());
    CHECK(lineAtInclude == chosenLine);
    REQUIRE_FALSE(linePlot->hasSolveError());

    const cwStationPositionLookup& lookup = root->scopedPositions();
    REQUIRE(lookup.hasPosition(kFixedStation));
    const QVector3D placed = lookup.position(kFixedStation);
    const cwGeoPoint expected = fixtureFixIn(kUtm48NFrame);
    CHECK(placed.x() == Catch::Approx(expected.x).margin(kPlacementMarginMeters));
    CHECK(placed.y() == Catch::Approx(expected.y).margin(kPlacementMarginMeters));
    CHECK(placed.z() == Catch::Approx(expected.z).margin(kPlacementMarginMeters));

    CHECK(bareFixWarnings(root).isEmpty());
    const QString readInSystem = subtreeAttachedFixFor(root, kFixedStation).readInSystem;
    INFO("readInSystem: " << readInSystem.toStdString());
    CHECK(readInSystem == kUtm48N);
}

TEST_CASE("The system chosen for an attachment's bare fixes survives save and reload",
          "[AttachInclude][BareFixCS]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-round-trip"), true);
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));
    const QUuid rootId = root->id();

    chooseFixSystem(root, kUtm48N);
    drainPipelines(fixture.get());

    QTemporaryDir destinationDir;
    REQUIRE(destinationDir.isValid());
    const QString destination =
        QDir(destinationDir.path()).filePath(QStringLiteral("bare-fix-reopened.cwproj"));
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
        if (candidate->id() == rootId) {
            reopened = candidate;
        }
    }
    REQUIRE(reopened != nullptr);
    REQUIRE_FALSE(reopened->externalCenterline().isEmpty());
    const QString reopenedSystem = reopened->externalCenterline().fixCoordinateSystem();
    INFO("fixCoordinateSystem after reload: '" << reopenedSystem.toStdString() << "'");
    CHECK(reopenedSystem == kUtm48N);
}

TEST_CASE("Reloading an attachment from its source keeps the system chosen for its bare fixes",
          "[AttachInclude][BareFixCS]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-reload"), true);
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    chooseFixSystem(root, kUtm48N);
    drainPipelines(fixture.get());

    cwExternalCenterlineManager* manager = managerOf(fixture.get());
    REQUIRE(manager->canReloadFromSource(root));
    auto reloadFuture = manager->reloadFromSource(root);
    REQUIRE(AsyncFuture::waitForFinished(reloadFuture, kAttachWaitMs));
    REQUIRE_FALSE(reloadFuture.result().hasError());
    drainPipelines(fixture.get());

    CHECK(root->externalCenterline().entryFile() == kBareFixFileName);
    CHECK(root->externalCenterline().fixCoordinateSystem() == kUtm48N);
    CHECK(subtreeAttachedFixFor(root, kFixedStation).readInSystem == kUtm48N);
    CHECK(bareFixWarnings(root).isEmpty());
}

TEST_CASE("A bare file fix in a project with no frame solves as written, with no warning",
          "[AttachInclude][BareFixCS]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-no-frame"), false);
    REQUIRE_FALSE(fixture->project->cavingRegion()->geoReference()->hasCoordinateSystem());
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    REQUIRE_FALSE(linePlot->hasSolveError());

    const cwStationPositionLookup& lookup = root->scopedPositions();
    REQUIRE(lookup.hasPosition(kFixedStation));
    CHECK(lookup.position(kFixedStation).x()
          == Catch::Approx(kFixEasting).margin(kPlacementMarginMeters));
    CHECK(bareFixWarnings(root).isEmpty());
}

TEST_CASE("A trip's attachment carries its own system for its bare fixes",
          "[AttachInclude][BareFixCS]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-trip"), true);
    cwCave* cave = fixture->cave;
    cwTrip* trip = fixture->trip;
    attachThroughManager(fixture.get(), trip, fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    {
        INFO("driver:\n" << linePlot->driverSource().toStdString());
        CHECK(inputSystemLineAtInclude(linePlot->driverSource()).isEmpty());
        // The trip's node carries the warning for it.
        CHECK(bareFixWarnings(cave).size() == 1);
    }

    chooseFixSystem(trip, kUtm48N);
    REQUIRE(trip->externalCenterline().fixCoordinateSystem() == kUtm48N);
    CHECK(cave->externalCenterline().fixCoordinateSystem().isEmpty());

    const QString chosenLine = QStringLiteral("*cs %1").arg(kUtm48N);
    CHECK(tryWait(kAttachWaitMs, [&]() {
        return inputSystemLineAtInclude(linePlot->driverSource()) == chosenLine;
    }));
    drainPipelines(fixture.get());

    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    const QString lineAtInclude = inputSystemLineAtInclude(linePlot->driverSource());
    INFO("input *cs at the include: " << lineAtInclude.toStdString());
    CHECK(lineAtInclude == chosenLine);
    CHECK_FALSE(linePlot->hasSolveError());
    CHECK(bareFixWarnings(cave).isEmpty());
    const QString readInSystem =
        attachedFixFor(cave, trip->scopePrefix() + kFixedStation).readInSystem;
    INFO("readInSystem: " << readInSystem.toStdString());
    CHECK(readInSystem == kUtm48N);
}

TEST_CASE("A native fix on the parent node leaves no system in force at a child source root's include",
          "[AttachInclude][BareFixCS][NativeCSScope]")
{
    const bool autoDeclination = GENERATE(true, false);
    INFO("auto declination: " << autoDeclination);

    auto fixture = makeProject(QStringLiteral("bare-fix-parent-native-cs"), true);
    cwCave* main = setUpMainWithNativeFix(fixture.get(), autoDeclination);
    cwSurveyNode* root = attachSourceThroughManager(fixture.get(), main, fixturePath(kBareFixFixture));
    REQUIRE(root->parentNode() == main);
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    const QString driver = linePlot->driverSource();
    INFO("driver:\n" << driver.toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    REQUIRE(driver.contains(QStringLiteral("*fix ") + kMainFixStation));
    CHECK(root->externalCenterline().fixCoordinateSystem().isEmpty());

    CHECK(inputSystemLineAtInclude(driver).isEmpty());
    CHECK(linePlot->hasSolveError());
    CHECK(linePlot->solveErrorSummary().contains(kInputProjectionError));
    CHECK(bareFixWarnings(root).size() == 1);
}

TEST_CASE("A native fix on the node leaves no system in force at its trip attachment's include",
          "[AttachInclude][BareFixCS][NativeCSScope]")
{
    const bool autoDeclination = GENERATE(true, false);
    INFO("auto declination: " << autoDeclination);

    auto fixture = makeProject(QStringLiteral("bare-fix-trip-native-cs"), true);
    cwCave* main = setUpMainWithNativeFix(fixture.get(), autoDeclination);
    attachThroughManager(fixture.get(), fixture->trip, fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    const QString driver = linePlot->driverSource();
    INFO("driver:\n" << driver.toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    REQUIRE(driver.contains(QStringLiteral("*fix ") + kMainFixStation));
    CHECK(fixture->trip->externalCenterline().fixCoordinateSystem().isEmpty());

    CHECK(inputSystemLineAtInclude(driver).isEmpty());
    CHECK(linePlot->hasSolveError());
    CHECK(linePlot->solveErrorSummary().contains(kInputProjectionError));
    CHECK(bareFixWarnings(main).size() == 1);
}

TEST_CASE("A native child cave still fixes in its own system after its parent's fixes end theirs",
          "[AttachInclude][BareFixCS][NativeCSScope]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-native-child-cs"), true);
    cwCave* main = setUpMainWithNativeFix(fixture.get(), true);
    cwCave* sub = addChildNode(main, QStringLiteral("Sub"), cwSurveyNode::Kind::Cave);
    addNativeTripWithShot(sub, QStringLiteral("Sub survey"), kSubFixStation, QStringLiteral("s2"));
    // The same system as Main's fix: the one Sub would find already in force
    // if Main's fixes left it there.
    addNativeFix(sub, kSubFixStation, kSubFixEasting);

    // The file's fix names its own *cs, so nothing chosen still solves.
    attachThroughManager(fixture.get(), fixture->trip, fixturePath(kOwnCsFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get(), kOwnCsFileName));

    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    const QString driver = linePlot->driverSource();
    INFO("driver:\n" << driver.toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());

    // Main's fixes end their system before the trip attachment's include.
    CHECK(inputSystemLineAtInclude(driver, kOwnCsFileName).isEmpty());

    // So Sub's block opens the system again for its own fix.
    const QStringList subBlock = blockLines(driver, QStringLiteral("sub"));
    INFO("Sub's block:\n" << subBlock.join(QLatin1Char('\n')).toStdString());
    const auto subFixLine = std::find_if(subBlock.cbegin(), subBlock.cend(), [](const QString& line) {
        return line.startsWith(QStringLiteral("*fix %1 ").arg(kSubFixStation));
    });
    REQUIRE(subFixLine != subBlock.cend());
    const bool opensSystemBeforeFix =
        std::any_of(subBlock.cbegin(), subFixLine, [](const QString& line) {
            return line.startsWith(QStringLiteral("*cs ")) && !line.startsWith(QStringLiteral("*cs out"));
        });
    CHECK(opensSystemBeforeFix);

    REQUIRE_FALSE(linePlot->hasSolveError());
    CHECK(bareFixWarnings(main).isEmpty());

    // Both fixes are read in UTM zone 48N: Sub's lands its easting offset east
    // of Main's in the project's local frame.
    const cwStationPositionLookup subPositions = sub->scopedPositions();
    const cwStationPositionLookup mainPositions = main->scopedPositions();
    REQUIRE(subPositions.hasPosition(kSubFixStation));
    REQUIRE(mainPositions.hasPosition(kMainFixStation));
    const QVector3D offset =
        subPositions.position(kSubFixStation) - mainPositions.position(kMainFixStation);
    CHECK(offset.x() == Catch::Approx(kSubFixEasting - kMainFixEasting).margin(kPlacementMarginMeters));
    CHECK(offset.y() == Catch::Approx(0.0).margin(kPlacementMarginMeters));
}
