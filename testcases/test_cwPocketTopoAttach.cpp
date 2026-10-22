/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "ExternalCenterlineTestHelpers.h"
#include "cwAttachedCenterlinesModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwExternalIncludeWriter.h"
#include "cwExternalStationHarvest.h"
#include "cwFileDeclination.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwLinePlotManager.h"
#include "cwLinePlotTask.h"
#include "cwPocketTopoFile.h"
#include "cwPocketTopoTranslator.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwScopeLabels.h"
#include "cwStation.h"
#include "cwSurvexCS.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterRegion.h"
#include "cwSurvexExporterUtils.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

// Test helpers
#include "PocketTopoFixtureWriter.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QDate>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

using cwPocketTopoFile::Reference;
using cwPocketTopoFile::Shot;
using cwPocketTopoFile::StationId;
using cwPocketTopoFile::Trip;
using PocketTopoFixtureWriter::Fixture;

namespace {

constexpr int kMillimetersPerMeter = 1000;
constexpr qint16 kNoTrip = Shot::kNoTrip;
// 1234 / 65536 of a circle, 6.78 degrees.
constexpr qint16 kEastDeclinationUnits = 1234;
constexpr double kFullCircleDegrees = 360.0;
constexpr double kAngleUnitsPerCircle = 65536.0;
constexpr double kEastDeclinationDegrees = kEastDeclinationUnits * kFullCircleDegrees / kAngleUnitsPerCircle;
constexpr double kInjectedDeclination = 4.5;
constexpr double kManualDeclination = 2.0;

// A reference on 1.0, in millimeters.
constexpr qint64 kFixEastMm = 1000000;
constexpr qint64 kFixNorthMm = 2000000;
constexpr qint32 kFixAltitudeMm = 300000;

// Where the node's own fix puts 1.0, in WGS84 UTM zone 13N.
constexpr double kNodeFixEasting = 478010.0;
constexpr double kNodeFixNorthing = 4430020.0;
constexpr double kNodeFixElevation = 1650.0;

const QString kUtm13N = QStringLiteral("EPSG:32613");
const QString kFileName = QStringLiteral("cave.top");
const QString kFirstTripName = QStringLiteral("2026-02-05 Robin survey");
const QString kSecondTripName = QStringLiteral("2026-03-14");
const QString kThirdTripName = QStringLiteral("2026-04-01 Lower series");
const QString kRenamedFirstTrip = QStringLiteral("Entrance series");

StationId dotted(quint16 major, quint16 minor)
{
    return StationId::dotted(major, minor);
}

Trip trip(const QDate& date, const QString& comment, qint16 declination = 0)
{
    Trip result;
    result.ticks = PocketTopoFixtureWriter::ticks(date);
    result.comment = comment;
    result.declination = declination;
    return result;
}

Shot shot(StationId from, StationId to, double meters, double azimuth, double clino, qint16 tripIndex)
{
    Shot result;
    result.from = from;
    result.to = to;
    result.distance = qint32(qRound(meters * kMillimetersPerMeter));
    result.azimuth = PocketTopoFixtureWriter::azimuthUnits(azimuth);
    result.inclination = PocketTopoFixtureWriter::angleUnits(clino);
    result.tripIndex = tripIndex;
    return result;
}

Shot splay(StationId from, double meters, double azimuth, double clino, qint16 tripIndex)
{
    return shot(from, StationId(), meters, azimuth, clino, tripIndex);
}

Reference reference(StationId station, qint64 eastMm, qint64 northMm, qint32 altitudeMm)
{
    Reference result;
    result.station = station;
    result.east = eastMm;
    result.north = northMm;
    result.altitude = altitudeMm;
    return result;
}

/**
 * Two trips. The first, manual declination, holds a triple on 1.0-1.1 (one
 * reading taken backwards), a splay from 1.0, and 1.1-1.2. The second, Auto
 * with no comment, joins 1.2 to series 2 with a zero-length leg and runs
 * 2.0-2.1 with a splay from 2.1.
 */
Fixture twoTripFixture()
{
    Fixture fixture;
    fixture.document.trips = {
        trip(QDate(2026, 2, 5), QStringLiteral("Robin survey\rBecka"), kEastDeclinationUnits),
        trip(QDate(2026, 3, 14), QString(), Trip::kAutoDeclination),
    };
    fixture.document.shots = {
        splay(dotted(1, 0), 0.0, 0.0, 0.0, kNoTrip),
        shot(dotted(1, 0), dotted(1, 1), 3.38, 271.35, -1.51, 0),
        shot(dotted(1, 0), dotted(1, 1), 3.38, 271.35, -1.69, 0),
        shot(dotted(1, 1), dotted(1, 0), 3.38, 91.36, 1.77, 0),
        splay(dotted(1, 0), 1.54, 333.43, 5.91, 0),
        shot(dotted(1, 1), dotted(1, 2), 3.06, 301.39, -4.19, 0),
        shot(dotted(1, 2), dotted(2, 0), 0.0, 0.0, 0.0, 1),
        shot(dotted(2, 0), dotted(2, 1), 4.0, 10.0, 0.0, 1),
        splay(dotted(2, 1), 1.0, 90.0, 0.0, 1),
    };
    return fixture;
}

//! twoTripFixture() after a third trip carries series 2 on to 2.2.
Fixture grownFixture()
{
    Fixture fixture = twoTripFixture();
    fixture.document.trips.append(trip(QDate(2026, 4, 1), QStringLiteral("Lower series")));
    fixture.document.shots.append(shot(dotted(2, 1), dotted(2, 2), 6.0, 45.0, -10.0, 2));
    return fixture;
}

QString writeFixture(const QString& dir, const Fixture& fixture, const QString& fileName = kFileName)
{
    const QString path = QDir(dir).absoluteFilePath(fileName);
    REQUIRE(PocketTopoFixtureWriter::writeFile(fixture, path));
    return path;
}

QStringList canonical(const QStringList& names)
{
    QStringList keys;
    for (const QString& name : names) {
        keys.append(cwStation::canonicalKey(name));
    }
    keys.sort();
    return keys;
}

//! Fails unless every one of \a lines appears in \a text, in that order.
void checkInOrder(const QString& text, const QStringList& lines)
{
    qsizetype from = 0;
    for (const QString& line : lines) {
        INFO("expected, in order: " << line.toStdString());
        const qsizetype at = text.indexOf(line, from);
        CHECK(at >= 0);
        if (at >= 0) {
            from = at + line.size();
        }
    }
}

QString triplet(double easting, double northing, double elevation)
{
    QString text;
    QTextStream stream(&text);
    cwSurvexExporterUtils::writeCoordTriplet(stream, easting, northing, elevation);
    stream.flush();
    return text;
}

//! A source root holding twoTripFixture()'s two derived trips, as the attach
//! builds it, with the copy in \a attachmentDir.
struct DriverSetup {
    cwCave* cave = nullptr;
    cwTrip* first = nullptr;
    cwTrip* second = nullptr;
};

cwTrip* addDerivedTrip(cwCave* cave, const QString& name, const QString& sourcePath,
                       const cwFileDeclination& declination)
{
    cwTrip* trip = new cwTrip();
    trip->setName(name);
    trip->setSourcePath(sourcePath);
    trip->setFileDeclination(declination);
    cave->addTrip(trip);
    return trip;
}

DriverSetup addPocketTopoRoot(cwCavingRegion& region)
{
    DriverSetup setup;
    setup.cave = new cwCave();
    setup.cave->setName(QStringLiteral("cave"));
    setup.cave->setKind(cwSurveyNodeKind::Kind::PocketTopoFile);
    setup.cave->setExternalCenterline(cwExternalCenterline(kFileName));
    setup.cave->setExternalStations({QStringLiteral("1.0"), QStringLiteral("1.1"), QStringLiteral("1.2"),
                                     QStringLiteral("2.0"), QStringLiteral("2.1")});
    region.addCave(setup.cave);

    cwFileDeclination manual;
    manual.kind = cwFileDeclination::Kind::Value;
    manual.degrees = kEastDeclinationDegrees;
    cwFileDeclination automatic;
    automatic.kind = cwFileDeclination::Kind::Auto;
    setup.first = addDerivedTrip(setup.cave, kFirstTripName, QStringLiteral("#1"), manual);
    setup.second = addDerivedTrip(setup.cave, kSecondTripName, QStringLiteral("#2"), automatic);
    return setup;
}

QString driverFor(const cwCavingRegion& region, const cwSurvexExporterRegion::Options& options,
                  const QString& globalCS)
{
    const cwCavingRegionData regionData = region.data();
    const cwSurvexExporterCaveTask::DriverTree tree(regionData.caves, cwScopeLabels(regionData));
    cwSurvexExporterCaveTask task;
    task.setExportOptions(options);

    QString driver;
    QTextStream stream(&driver);
    for (const cwCaveData& node : regionData.caves) {
        REQUIRE(task.writeNode(stream, node, tree, globalCS));
    }
    stream.flush();
    INFO("errors: " << task.errors().join(QStringLiteral("\n")).toStdString());
    CHECK(task.errors().isEmpty());
    return driver;
}

std::unique_ptr<SavedProjectFixture> makeProject(const QString& projectFileBase)
{
    return makeSavedProject(projectFileBase, QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
}

cwSurveyNode* attachTop(SavedProjectFixture* fixture, const QString& source)
{
    cwSurveyNode* root = attachSourceThroughManager(
        fixture, fixture->project->cavingRegion()->rootNode(), source);
    drainPipelines(fixture);
    return root;
}

cwTrip* tripWithSourcePath(const cwSurveyNode* node, const QString& sourcePath)
{
    for (cwTrip* trip : node->trips()) {
        if (trip->sourcePath() == sourcePath) {
            return trip;
        }
    }
    return nullptr;
}

//! The names \a trip's solved stations go by, sorted.
QStringList solvedNames(const cwTrip* trip)
{
    QStringList names;
    for (const auto& station : trip->solvedStations()) {
        names.append(station.first);
    }
    names.sort();
    return names;
}

void checkSolved(SavedProjectFixture* fixture)
{
    const cwLinePlotManager* linePlot = fixture->rootData->linePlotManager();
    INFO("solve error: " << linePlot->solveErrorMessage().toStdString());
    INFO("driver:\n" << linePlot->driverSource().toStdString());
    CHECK_FALSE(linePlot->hasSolveError());
}

} // namespace

TEST_CASE("The include writer writes *include for a file cavern reads", "[Exporter][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString svx = QDir(dir.path()).absoluteFilePath(QStringLiteral("cave.svx"));

    cwSurvexCS::SidecarWriter sidecars;
    cwSurvexExporterUtils::CsScope scope(sidecars);
    QString text;
    QTextStream stream(&text);
    CHECK(cwExternalIncludeWriter::write(stream, svx, {}, scope));
    stream.flush();
    CHECK(text == QStringLiteral("*include \"%1\"\n").arg(svx));
}

TEST_CASE("The include writer writes a .top's translation in place of *include",
          "[Exporter][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString top = writeFixture(dir.path(), twoTripFixture());

    cwPocketTopoTranslator::Input input;
    input.injectedDeclinations = {{1, kInjectedDeclination}};

    cwSurvexCS::SidecarWriter expectedSidecars;
    cwSurvexExporterUtils::CsScope expectedScope(expectedSidecars);
    cwPocketTopoTranslator::Input expectedInput = input;
    expectedInput.topPath = top;
    const std::optional<QString> expected = cwPocketTopoTranslator::translate(expectedInput, expectedScope);
    REQUIRE(expected.has_value());

    cwSurvexCS::SidecarWriter sidecars;
    cwSurvexExporterUtils::CsScope scope(sidecars);
    QString text;
    QTextStream stream(&text);
    // topPath is the writer's to fill.
    input.topPath = QStringLiteral("elsewhere.top");
    CHECK(cwExternalIncludeWriter::write(stream, top, input, scope));
    stream.flush();
    INFO("written:\n" << text.toStdString());
    CHECK(text == *expected);
    CHECK_FALSE(text.contains(QStringLiteral("*include")));
    CHECK(text.contains(QStringLiteral("; trip #2 : 2026-03-14\n*date 2026.03.14\n*declination 4.50 degrees\n")));

    SECTION("A .top it cannot read writes nothing")
    {
        const QString broken = QDir(dir.path()).absoluteFilePath(QStringLiteral("broken.top"));
        QFile file(broken);
        REQUIRE(file.open(QFile::WriteOnly));
        file.write(QByteArrayLiteral("not a top file"));
        file.close();

        QString brokenText;
        QTextStream brokenStream(&brokenText);
        CHECK_FALSE(cwExternalIncludeWriter::write(brokenStream, broken, {}, scope));
        brokenStream.flush();
        CHECK(brokenText.isEmpty());
    }
}

TEST_CASE("The driver writes an attached .top's translation inside the node's block",
          "[Exporter][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    writeFixture(dir.path(), twoTripFixture());

    cwCavingRegion region;
    const DriverSetup setup = addPocketTopoRoot(region);
    cwSurvexExporterRegion::Options options;
    options.caveAttachmentDirs.insert(setup.cave->id(), dir.path());
    options.sourceUnitInjectedDeclinations.insert(setup.second->id(), kInjectedDeclination);

    const QString driver = driverFor(region, options, QString());
    INFO("driver:\n" << driver.toStdString());

    const QString label = cwScopeLabels(region.data()).label(setup.cave->id());
    REQUIRE_FALSE(label.isEmpty());
    CHECK_FALSE(driver.contains(QStringLiteral(".top\"")));
    CHECK_FALSE(driver.contains(QStringLiteral("*include")));
    checkInOrder(driver, {QStringLiteral("*begin %1").arg(label),
                          QStringLiteral("*units tape metres\n"),
                          QStringLiteral("*data normal from to tape compass clino\n"),
                          // The manual trip keeps the file's value.
                          QStringLiteral("; trip #1 : 2026-02-05 Robin survey\n"
                                         "*date 2026.02.05\n"
                                         "*declination 6.78 degrees\n"),
                          // The Auto trip, with no location, takes its derived
                          // trip's resolved value.
                          QStringLiteral("; trip #2 : 2026-03-14\n"
                                         "*date 2026.03.14\n"
                                         "*declination 4.50 degrees\n"),
                          QStringLiteral("*equate 1.2 2.0\n"),
                          QStringLiteral("*end %1").arg(label)});
}

TEST_CASE("The driver gives a located .top node's Auto trips IGRF declination",
          "[Exporter][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    writeFixture(dir.path(), twoTripFixture());

    cwCavingRegion region;
    const DriverSetup setup = addPocketTopoRoot(region);
    cwFixStation fix;
    fix.setStationName(QStringLiteral("1.0"));
    fix.setInputCS(kUtm13N);
    fix.setCoordinate(kNodeFixEasting, kNodeFixNorthing, kNodeFixElevation);
    setup.cave->fixStations()->appendFixStation(fix);

    cwSurvexExporterRegion::Options options;
    options.caveAttachmentDirs.insert(setup.cave->id(), dir.path());
    options.sourceUnitInjectedDeclinations.insert(setup.second->id(), kInjectedDeclination);

    const QString driver = driverFor(region, options, kUtm13N);
    INFO("driver:\n" << driver.toStdString());

    CHECK_FALSE(driver.contains(QStringLiteral("*include")));
    CHECK_FALSE(driver.contains(QStringLiteral("*declination 4.50 degrees")));
    checkInOrder(driver, {QStringLiteral("*fix 1.0 ")
                              + triplet(kNodeFixEasting, kNodeFixNorthing, kNodeFixElevation),
                          QStringLiteral("; trip #1 : 2026-02-05 Robin survey\n"
                                         "*date 2026.02.05\n"
                                         "*declination 6.78 degrees\n"),
                          QStringLiteral("; trip #2 : 2026-03-14\n"
                                         "*date 2026.03.14\n"),
                          QStringLiteral("*declination auto ")
                              + triplet(kNodeFixEasting, kNodeFixNorthing, kNodeFixElevation),
                          QStringLiteral("2.0 2.1 ")});
}

TEST_CASE("buildInput resolves the declination of each derived trip whose file leaves it to auto",
          "[Exporter][PocketTopo]")
{
    cwCavingRegion region;
    const DriverSetup setup = addPocketTopoRoot(region);
    for (cwTrip* trip : {setup.first, setup.second}) {
        trip->calibrations()->setAutoDeclination(false);
    }
    setup.first->calibrations()->setDeclinationManual(kManualDeclination);
    setup.second->calibrations()->setDeclinationManual(kInjectedDeclination);
    REQUIRE(setup.second->calibrations()->declination() == kInjectedDeclination);

    cwCave* native = new cwCave();
    native->setName(QStringLiteral("Native"));
    region.addCave(native);
    cwTrip* nativeTrip = new cwTrip();
    nativeTrip->setName(QStringLiteral("Native trip"));
    nativeTrip->calibrations()->setAutoDeclination(false);
    nativeTrip->calibrations()->setDeclinationManual(kManualDeclination);
    native->addTrip(nativeTrip);

    cwLinePlotTask::ExternalCenterlineInputs external;
    external.caveAttachmentDirs.insert(setup.cave->id(), QStringLiteral("/attachments"));
    const cwLinePlotTask::Input input = cwLinePlotTask::buildInput(&region, external);

    // The manual trip's file owns its value, and a native trip has no file.
    const QHash<QUuid, double> expected = {{setup.second->id(), kInjectedDeclination}};
    CHECK(input.sourceUnitInjectedDeclinations == expected);
    CHECK(input.tripInjectedDeclinations.isEmpty());
}

TEST_CASE("harvest reads a .top's station names through its translation", "[Harvest][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString top = writeFixture(dir.path(), twoTripFixture());

    const auto names = cwExternalStationHarvest::harvest(top);
    INFO("harvest: " << names.errorMessage().toStdString());
    REQUIRE_FALSE(names.hasError());
    CHECK(names.value() == canonical({QStringLiteral("1.0"), QStringLiteral("1.1"), QStringLiteral("1.2"),
                                      QStringLiteral("2.0"), QStringLiteral("2.1")}));
}

TEST_CASE("harvestWithFixes lists a .top's references as the stations it fixes",
          "[Harvest][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    Fixture fixture = twoTripFixture();
    fixture.document.references = {reference(dotted(1, 0), kFixEastMm, kFixNorthMm, kFixAltitudeMm)};
    const QString top = writeFixture(dir.path(), fixture);

    const auto stations = cwExternalStationHarvest::harvestWithFixes(top);
    INFO("harvest: " << stations.errorMessage().toStdString());
    REQUIRE_FALSE(stations.hasError());
    CHECK(stations.value().names.contains(cwStation::canonicalKey(QStringLiteral("2.1"))));
    CHECK(stations.value().fixedNames == QStringList({cwStation::canonicalKey(QStringLiteral("1.0"))}));
}

TEST_CASE("harvestComponents reads a .top's component that only a seed places",
          "[Harvest][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    Fixture fixture;
    fixture.document.trips = {trip(QDate(2026, 2, 5), QStringLiteral("Two pieces"))};
    // Series 1 and series 3 share no station.
    fixture.document.shots = {
        shot(dotted(1, 0), dotted(1, 1), 5.0, 0.0, 0.0, 0),
        shot(dotted(3, 0), dotted(3, 1), 5.0, 90.0, 0.0, 0),
    };
    const QString top = writeFixture(dir.path(), fixture);

    const auto names = cwExternalStationHarvest::harvestComponents(top, {QStringLiteral("3.0")});
    INFO("harvest: " << names.errorMessage().toStdString());
    REQUIRE_FALSE(names.hasError());
    CHECK(names.value() == canonical({QStringLiteral("3.0"), QStringLiteral("3.1")}));
}

TEST_CASE("Attaching a .top names and dates a trip per PocketTopo trip and solves it",
          "[Attach][PocketTopo]")
{
    auto fixture = makeProject(QStringLiteral("pockettopo-attach"));
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    cwSurveyNode* root = attachTop(fixture.get(), writeFixture(sourceDir.path(), twoTripFixture()));

    CHECK(root->kind() == cwSurveyNode::Kind::PocketTopoFile);
    CHECK(root->isSourceRoot());
    CHECK(root->childNodeCount() == 0);
    CHECK(tripNames(root) == QStringList({kFirstTripName, kSecondTripName}));
    const cwTrip* first = tripWithSourcePath(root, QStringLiteral("#1"));
    const cwTrip* second = tripWithSourcePath(root, QStringLiteral("#2"));
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    CHECK(first->date().date() == QDate(2026, 2, 5));
    CHECK(second->date().date() == QDate(2026, 3, 14));

    // The harvest lists the root's stations, so a tie-in can name them.
    const cwAttachedCenterlinesModel* rows = managerOf(fixture.get())->attachedCenterlinesModel();
    INFO("row error: " << rows->errorFor(root->id()).toStdString());
    CHECK(rows->errorFor(root->id()).isEmpty());
    CHECK(root->externalStations()
          == canonical({QStringLiteral("1.0"), QStringLiteral("1.1"), QStringLiteral("1.2"),
                        QStringLiteral("2.0"), QStringLiteral("2.1")}));

    checkSolved(fixture.get());
    CHECK_FALSE(fixture->rootData->linePlotManager()->driverSource().contains(QStringLiteral(".top\"")));
    CHECK(solvedNames(first)
          == QStringList({QStringLiteral("1.0"), QStringLiteral("1.1"), QStringLiteral("1.2")}));
    CHECK(solvedNames(second).contains(QStringLiteral("2.1")));
}

TEST_CASE("An attached .top's Auto trip takes the declination set on its derived trip",
          "[Attach][PocketTopo]")
{
    auto fixture = makeProject(QStringLiteral("pockettopo-injected-declination"));
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    cwSurveyNode* root = attachTop(fixture.get(), writeFixture(sourceDir.path(), twoTripFixture()));

    cwTrip* second = tripWithSourcePath(root, QStringLiteral("#2"));
    REQUIRE(second != nullptr);
    second->calibrations()->setAutoDeclination(false);
    second->calibrations()->setDeclinationManual(kInjectedDeclination);
    drainPipelines(fixture.get());

    checkSolved(fixture.get());
    const QString driver = fixture->rootData->linePlotManager()->driverSource();
    INFO("driver:\n" << driver.toStdString());
    CHECK(driver.contains(QStringLiteral("; trip #1 : 2026-02-05 Robin survey\n"
                                         "*date 2026.02.05\n"
                                         "*declination 6.78 degrees\n")));
    CHECK(driver.contains(QStringLiteral("; trip #2 : 2026-03-14\n"
                                         "*date 2026.03.14\n"
                                         "*declination 4.50 degrees\n")));
}

TEST_CASE("Reloading a grown .top adds the new trip and keeps the earlier trips and their names",
          "[Attach][PocketTopo]")
{
    auto fixture = makeProject(QStringLiteral("pockettopo-reload-grown"));
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString source = writeFixture(sourceDir.path(), twoTripFixture());
    cwSurveyNode* root = attachTop(fixture.get(), source);

    cwTrip* first = tripWithSourcePath(root, QStringLiteral("#1"));
    cwTrip* second = tripWithSourcePath(root, QStringLiteral("#2"));
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    const QUuid firstId = first->id();
    const QUuid secondId = second->id();
    first->setName(kRenamedFirstTrip);
    fixture->project->waitSaveToFinish();

    writeFixture(sourceDir.path(), grownFixture());
    auto future = managerOf(fixture.get())->reloadFromSource(root);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("reload: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());

    CHECK(tripNames(root) == QStringList({kRenamedFirstTrip, kSecondTripName, kThirdTripName}));
    CHECK(tripWithSourcePath(root, QStringLiteral("#1"))->id() == firstId);
    CHECK(tripWithSourcePath(root, QStringLiteral("#2"))->id() == secondId);
    const cwTrip* third = tripWithSourcePath(root, QStringLiteral("#3"));
    REQUIRE(third != nullptr);
    CHECK(third->date().date() == QDate(2026, 4, 1));

    checkSolved(fixture.get());
    CHECK(root->externalStations().contains(cwStation::canonicalKey(QStringLiteral("2.2"))));
    CHECK(solvedNames(third) == QStringList({QStringLiteral("2.1"), QStringLiteral("2.2")}));
}
