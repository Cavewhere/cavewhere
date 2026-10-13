/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
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
#include "cwGridConvergence.h"
#include "cwLinePlotManager.h"
#include "cwLocalProjectionManager.h"
#include "cwNodeWarningModel.h"
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
#include <cmath>
#include <memory>

namespace {

const QString kBareFixFixture = QStringLiteral("survex_bare_fix/bare_fix.svx");
const QString kBareFixFileName = QStringLiteral("bare_fix.svx");
// bare_fix.svx with standard errors after the fix's coordinate.
const QString kStdErrorsFixture =
    QStringLiteral("survex_bare_fix_std_errors/bare_fix_std_errors.svx");
const QString kStdErrorsFileName = QStringLiteral("bare_fix_std_errors.svx");
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
// The fixture's leg A-B: 10 m at azimuth 90, so B sits that far east of A.
const QString kEastStation = QStringLiteral("bare.B");
constexpr double kEastLegMeters = 10.0;
// A native fix far from the file's, past the anchor's re-derive threshold.
const QString kFarFixStation = QStringLiteral("far1");
constexpr double kFarFixEasting = 600000.0;
const QString kOutputSystemPrefix = QStringLiteral("*cs out");
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
                   && !trimmed.startsWith(kOutputSystemPrefix)) {
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

void drainRoot(cwRootData* rootData)
{
    rootData->linePlotManager()->waitToFinish();
    rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
}

//! Saves \a fixture's project as \a fileName in \a destinationDir, closes it,
//! and loads the saved file into a fresh cwRootData.
std::unique_ptr<cwRootData> saveAndReopen(std::unique_ptr<SavedProjectFixture>& fixture,
                                          const QTemporaryDir& destinationDir,
                                          const QString& fileName)
{
    REQUIRE(destinationDir.isValid());
    REQUIRE(fixture->project->saveAs(QDir(destinationDir.path()).filePath(fileName)));
    fixture->project->waitSaveToFinish();
    drainPipelines(fixture.get());
    const QString savedPath = fixture->project->filename();
    fixture.reset();

    auto freshRoot = std::make_unique<cwRootData>();
    freshRoot->project()->loadFile(savedPath);
    freshRoot->project()->waitLoadToFinish();
    drainRoot(freshRoot.get());
    return freshRoot;
}

cwSurveyNode* nodeWithId(cwRootData* rootData, const QUuid& id)
{
    const QList<cwSurveyNode*> nodes = rootData->region()->rootNode()->allNodes();
    const auto found = std::find_if(nodes.cbegin(), nodes.cend(), [&id](const cwSurveyNode* node) {
        return node->id() == id;
    });
    return found == nodes.cend() ? nullptr : *found;
}

//! Waits for the project to gain a frame, then for the solve that frame
//! triggers (its driver sets an output system).
bool waitForSeededFrame(SavedProjectFixture* fixture)
{
    cwGeoReference* geoReference = fixture->project->cavingRegion()->geoReference();
    const bool seeded = tryWait(kAttachWaitMs, [geoReference]() {
        return geoReference->hasCoordinateSystem();
    });
    const bool solvedInFrame = seeded && tryWait(kAttachWaitMs, [fixture]() {
        return linePlotOf(fixture)->driverSource().contains(kOutputSystemPrefix);
    });
    drainPipelines(fixture);
    return solvedInFrame;
}

//! A tree shaped like a large Survex dataset: north.svx includes houping
//! beside it and sanqiao above it, and sanqiao/fixedpts/ents.svx fixes e1
//! with no *cs. north_cs_out.svx swaps in a houping that fixes h1 under its
//! own *cs UTM48N.
QString bareFixTreeFixture(const QString& entry)
{
    return fixturePath(QStringLiteral("survex_bare_fix_tree/north/%1").arg(entry));
}
const QString kTreeBareFixStation = QStringLiteral("sanqiao.e1");
const QString kTreeOwnCsFixStation = QStringLiteral("houping.h1");
// WGS 84 / UTM zone 47N, whose meridian sits 6 degrees west of the fix's.
const QString kUtm47NFrame = QStringLiteral("EPSG:32647");
// Well under the convergence 6 degrees off the meridian gives at the fix.
constexpr double kOffMeridianConvergenceDegrees = 1.0;
// A convergence read at the frame's own anchor: PROJ answers a hair off zero.
constexpr double kAnchorConvergenceMarginDegrees = 1e-6;

//! The convergence the readout should report for the fixture's fix in the
//! project's current frame.
double fixtureConvergenceIn(const QString& frameCS)
{
    const auto convergence = cwGridConvergence::computeAt(fixtureFixIn(frameCS), frameCS);
    REQUIRE_FALSE(convergence.hasError());
    return convergence.value();
}

//! Waits for \a node's grid convergence readout to read Valid.
bool waitForValidConvergence(const cwSurveyNode* node)
{
    return tryWait(kAttachWaitMs, [node]() {
        return node->gridConvergence()->state() == cwGridConvergence::Valid;
    });
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
    auto freshRoot = saveAndReopen(fixture, destinationDir,
                                   QStringLiteral("bare-fix-reopened.cwproj"));

    cwSurveyNode* reopened = nodeWithId(freshRoot.get(), rootId);
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

TEST_CASE("A bare file fix under a cave shows on the region root's warnings and badge",
          "[AttachInclude][BareFixCS]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-roll-up"), true);
    cwCave* main = setUpMainWithNativeFix(fixture.get(), true);
    cwSurveyNode* root = attachSourceThroughManager(fixture.get(), main, fixturePath(kBareFixFixture));
    REQUIRE(root->parentNode() == main);
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));
    REQUIRE(bareFixWarnings(root).size() == 1);

    cwSurveyNode* regionRoot = fixture->project->cavingRegion()->rootNode();
    cwNodeWarningModel regionModel;
    regionModel.setNode(regionRoot);

    const auto rollUpRowFor = [&regionModel](const cwSurveyNode* owner) {
        for (int row = 0; row < regionModel.count(); ++row) {
            const QModelIndex index = regionModel.index(row);
            if (regionModel.data(index, cwNodeWarningModel::NodeRole).value<cwSurveyNode*>() == owner
                && regionModel.data(index, cwNodeWarningModel::MessageRole).toString()
                       .contains(QStringLiteral("no coordinate system"))) {
                return row;
            }
        }
        return -1;
    };

    CHECK(tryWait(kAttachWaitMs, [&]() { return rollUpRowFor(root) >= 0; }));
    const int row = rollUpRowFor(root);
    REQUIRE(row >= 0);
    CHECK(regionModel.data(regionModel.index(row), cwNodeWarningModel::DetailRole).toString()
              .contains(root->name()));

    // The region root carries no warning of its own, so its count is the subtree's.
    CHECK(regionRoot->errorModel()->warningCount() >= 1);
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

TEST_CASE("Choosing a system for a bare file fix gives a project with no frame one, centered on the fix",
          "[AttachInclude][BareFixCS][BareFixFrame]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-seeds-frame"), false);
    cwCavingRegion* region = fixture->project->cavingRegion();
    cwGeoReference* geoReference = region->geoReference();
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));
    REQUIRE_FALSE(geoReference->hasCoordinateSystem());

    chooseFixSystem(root, kUtm48N);
    REQUIRE(waitForSeededFrame(fixture.get()));

    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(geoReference->anchor().id == root->id());
    const QString description = region->localProjection()->anchorDescription();
    INFO("anchor description: " << description.toStdString());
    CHECK(description.contains(kFixedStation));

    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    CHECK(inputSystemLineAtInclude(linePlot->driverSource()) == QStringLiteral("*cs %1").arg(kUtm48N));
    REQUIRE_FALSE(linePlot->hasSolveError());

    // The frame is derived at the fix, so the plot sits at the local origin.
    const cwStationPositionLookup& lookup = root->scopedPositions();
    REQUIRE(lookup.hasPosition(kFixedStation));
    REQUIRE(lookup.hasPosition(kEastStation));
    const QVector3D fixed = lookup.position(kFixedStation);
    CHECK(fixed.x() == Catch::Approx(0.0).margin(kPlacementMarginMeters));
    CHECK(fixed.y() == Catch::Approx(0.0).margin(kPlacementMarginMeters));
    CHECK(fixed.z() == Catch::Approx(kFixElevation).margin(kPlacementMarginMeters));
    const QVector3D east = lookup.position(kEastStation);
    CHECK(east.x() == Catch::Approx(kEastLegMeters).margin(kPlacementMarginMeters));
    CHECK(east.y() == Catch::Approx(0.0).margin(kPlacementMarginMeters));

    CHECK(subtreeAttachedFixFor(root, kFixedStation).readInSystem == kUtm48N);
    CHECK(bareFixWarnings(root).isEmpty());
}

TEST_CASE("A bare file fix with standard errors gives a project with no frame one at its position",
          "[AttachInclude][BareFixCS][BareFixFrame]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-std-errors-seeds-frame"), false);
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kStdErrorsFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get(), kStdErrorsFileName));
    REQUIRE_FALSE(fixture->project->cavingRegion()->geoReference()->hasCoordinateSystem());

    chooseFixSystem(root, kUtm48N);
    REQUIRE(waitForSeededFrame(fixture.get()));

    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    REQUIRE_FALSE(linePlot->hasSolveError());
    const cwStationPositionLookup& lookup = root->scopedPositions();
    REQUIRE(lookup.hasPosition(kFixedStation));
    const QVector3D fixed = lookup.position(kFixedStation);
    CHECK(fixed.x() == Catch::Approx(0.0).margin(kPlacementMarginMeters));
    CHECK(fixed.y() == Catch::Approx(0.0).margin(kPlacementMarginMeters));
    CHECK(fixed.z() == Catch::Approx(kFixElevation).margin(kPlacementMarginMeters));
}

TEST_CASE("Unsetting the system that gave the project its frame clears the frame",
          "[AttachInclude][BareFixCS][BareFixFrame]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-unset-frame"), false);
    cwGeoReference* geoReference = fixture->project->cavingRegion()->geoReference();
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    chooseFixSystem(root, kUtm48N);
    REQUIRE(waitForSeededFrame(fixture.get()));

    chooseFixSystem(root, QString());
    REQUIRE(tryWait(kAttachWaitMs, [geoReference]() {
        return !geoReference->hasCoordinateSystem();
    }));
    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    CHECK(tryWait(kAttachWaitMs, [linePlot]() {
        return !linePlot->driverSource().contains(kOutputSystemPrefix);
    }));
    drainPipelines(fixture.get());

    INFO("driver:\n" << linePlot->driverSource().toStdString());
    CHECK(geoReference->state() == cwGeoReference::Ungeoreferenced);
    REQUIRE_FALSE(linePlot->hasSolveError());
    const cwStationPositionLookup& lookup = root->scopedPositions();
    REQUIRE(lookup.hasPosition(kFixedStation));
    CHECK(lookup.position(kFixedStation).x()
          == Catch::Approx(kFixEasting).margin(kPlacementMarginMeters));
    CHECK(bareFixWarnings(root).isEmpty());
}

TEST_CASE("A frame a bare file fix's system gave the project survives save and reload",
          "[AttachInclude][BareFixCS][BareFixFrame]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-frame-round-trip"), false);
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));
    const QUuid rootId = root->id();

    chooseFixSystem(root, kUtm48N);
    REQUIRE(waitForSeededFrame(fixture.get()));
    const QString savedFrame =
        fixture->project->cavingRegion()->geoReference()->localCoordinateSystem();
    REQUIRE_FALSE(savedFrame.isEmpty());

    QTemporaryDir destinationDir;
    auto freshRoot = saveAndReopen(fixture, destinationDir,
                                   QStringLiteral("bare-fix-frame-reopened.cwproj"));

    cwSurveyNode* reopened = nodeWithId(freshRoot.get(), rootId);
    REQUIRE(reopened != nullptr);
    // The reload's scan lists the file's fix again; the frame holds through it.
    CHECK(tryWait(kAttachWaitMs, [reopened]() {
        return subtreeListsAttachedFix(reopened, kFixedStation);
    }));
    drainRoot(freshRoot.get());

    const cwGeoReference* reopenedGeoReference = freshRoot->region()->geoReference();
    CHECK(reopenedGeoReference->localCoordinateSystem() == savedFrame);
    CHECK(reopenedGeoReference->state() == cwGeoReference::Anchored);
    CHECK(reopenedGeoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(reopenedGeoReference->anchor().id == rootId);
}

TEST_CASE("Choosing a system for a trip attachment's bare fix gives a project with no frame one",
          "[AttachInclude][BareFixCS][BareFixFrame]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-trip-seeds-frame"), false);
    cwGeoReference* geoReference = fixture->project->cavingRegion()->geoReference();
    cwCave* cave = fixture->cave;
    cwTrip* trip = fixture->trip;
    attachThroughManager(fixture.get(), trip, fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));
    REQUIRE_FALSE(geoReference->hasCoordinateSystem());

    chooseFixSystem(trip, kUtm48N);
    REQUIRE(waitForSeededFrame(fixture.get()));

    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(geoReference->anchor().id == trip->id());

    cwLinePlotManager* linePlot = linePlotOf(fixture.get());
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    REQUIRE_FALSE(linePlot->hasSolveError());
    const QString station = trip->scopePrefix() + kFixedStation;
    const cwStationPositionLookup& lookup = cave->scopedPositions();
    REQUIRE(lookup.hasPosition(station));
    CHECK(lookup.position(station).x() == Catch::Approx(0.0).margin(kPlacementMarginMeters));
    CHECK(lookup.position(station).y() == Catch::Approx(0.0).margin(kPlacementMarginMeters));
    CHECK(attachedFixFor(cave, station).readInSystem == kUtm48N);
}

TEST_CASE("A native fix added after a bare file fix's system gave the project its frame leaves the frame alone",
          "[AttachInclude][BareFixCS][BareFixFrame]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-frame-then-native"), false);
    cwGeoReference* geoReference = fixture->project->cavingRegion()->geoReference();
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    chooseFixSystem(root, kUtm48N);
    REQUIRE(waitForSeededFrame(fixture.get()));
    const QString seededFrame = geoReference->localCoordinateSystem();

    addNativeTripWithShot(fixture->cave, QStringLiteral("Far survey"), kFarFixStation,
                          QStringLiteral("far2"));
    addNativeFix(fixture->cave, kFarFixStation, kFarFixEasting);
    drainPipelines(fixture.get());

    CHECK(geoReference->localCoordinateSystem() == seededFrame);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(geoReference->anchor().id == root->id());
}

TEST_CASE("The grid convergence readout converges at a bare file fix once its system is chosen",
          "[AttachInclude][BareFixCS][GridConvergence]")
{
    // A frame one zone west of the fix's, so the fix sits off its meridian
    // and the convergence there is far from zero.
    auto fixture = makeProject(QStringLiteral("bare-fix-convergence"), false);
    fixture->project->cavingRegion()->geoReference()->restore(cwGeoReference::Frozen,
                                                              kUtm47NFrame, {}, QString());
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));
    const cwGridConvergence* convergence = root->gridConvergence();
    REQUIRE(convergence->state() == cwGridConvergence::NoFixStation);

    chooseFixSystem(root, kUtm48N);
    const bool valid = waitForValidConvergence(root);
    drainPipelines(fixture.get());
    INFO("readout: " << convergence->text().toStdString());
    CHECK(valid);
    CHECK(convergence->state() == cwGridConvergence::Valid);
    CHECK(convergence->text().contains(kFixedStation));
    const QString frame = fixture->project->cavingRegion()->geoReference()->localCoordinateSystem();
    const double expected = fixtureConvergenceIn(frame);
    INFO("expected convergence: " << expected);
    REQUIRE(std::abs(expected) > kOffMeridianConvergenceDegrees);
    CHECK(convergence->angle() == Catch::Approx(expected));

    // With no system the fix places nothing again.
    chooseFixSystem(root, QString());
    CHECK(tryWait(kAttachWaitMs, [convergence]() {
        return convergence->state() == cwGridConvergence::NoFixStation;
    }));
}

TEST_CASE("A bare file fix whose system gave the project its frame converges at that fix",
          "[AttachInclude][BareFixCS][BareFixFrame][GridConvergence]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-frame-convergence"), false);
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    chooseFixSystem(root, kUtm48N);
    REQUIRE(waitForSeededFrame(fixture.get()));

    const cwGridConvergence* convergence = root->gridConvergence();
    const bool valid = waitForValidConvergence(root);
    INFO("readout: " << convergence->text().toStdString());
    CHECK(valid);
    CHECK(convergence->text().contains(kFixedStation));
    // The frame is centered on the fix, where grid north is true north.
    CHECK_THAT(convergence->angle(),
               Catch::Matchers::WithinAbs(0.0, kAnchorConvergenceMarginDegrees));
}

TEST_CASE("An attachment owner converges at a bare file fix listed below it",
          "[AttachInclude][BareFixCS][GridConvergence]")
{
    auto fixture = makeProject(QStringLiteral("bare-fix-tree-convergence"), true);
    cwCave* owner = attachSourceUnderRegion(fixture.get(),
                                            bareFixTreeFixture(QStringLiteral("north.svx")));
    drainPipelines(fixture.get());
    REQUIRE(tryWait(kAttachWaitMs, [owner]() {
        return subtreeListsAttachedFix(owner, kTreeBareFixStation);
    }));
    // The fix is listed on a node below the owner, where the system is chosen.
    CHECK_FALSE(attachedFixStations(owner).contains(kTreeBareFixStation));

    chooseFixSystem(owner, kUtm48N);
    REQUIRE(tryWait(kAttachWaitMs, [owner]() {
        return subtreeAttachedFixFor(owner, kTreeBareFixStation).readInSystem == kUtm48N;
    }));
    const bool valid = waitForValidConvergence(owner);
    drainPipelines(fixture.get());

    const cwGridConvergence* convergence = owner->gridConvergence();
    INFO("readout: " << convergence->text().toStdString());
    CHECK(valid);
    CHECK(convergence->text().contains(kTreeBareFixStation));

    // A native cave beside it has no fix of its own and reads none.
    CHECK(fixture->cave->gridConvergence()->state() == cwGridConvergence::NoFixStation);
}

TEST_CASE("An attachment owner converges at a file fix that names its own system",
          "[AttachInclude][BareFixCS][GridConvergence]")
{
    auto fixture = makeProject(QStringLiteral("own-cs-tree-convergence"), true);
    cwCave* owner = attachSourceUnderRegion(fixture.get(),
                                            bareFixTreeFixture(QStringLiteral("north_cs_out.svx")));
    drainPipelines(fixture.get());
    REQUIRE(tryWait(kAttachWaitMs, [owner]() {
        return subtreeListsAttachedFix(owner, kTreeOwnCsFixStation);
    }));
    REQUIRE(subtreeAttachedFixFor(owner, kTreeOwnCsFixStation).coordinateSystem == kUtm48N);

    // No system is chosen: houping's fix places the cave on its own.
    const bool valid = waitForValidConvergence(owner);
    drainPipelines(fixture.get());
    const cwGridConvergence* convergence = owner->gridConvergence();
    INFO("readout: " << convergence->text().toStdString());
    CHECK(valid);
    CHECK(convergence->text().contains(kTreeOwnCsFixStation));
}
