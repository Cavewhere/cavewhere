/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "ExternalCenterlineTestHelpers.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCompassMakFile.h"
#include "cwCompassMakTranslator.h"
#include "cwExternalCenterline.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwScopeLabels.h"
#include "cwSurvexCS.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterRegion.h"
#include "cwSurvexExporterUtils.h"
#include "cwTrip.h"

//Qt includes
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

const QString kZoneNoDatumMak = QStringLiteral("compass_zone_no_datum.mak");
const QString kUtm13N = QStringLiteral("EPSG:32613");
const QString kWgs84LatLong = QStringLiteral("EPSG:4326");
// survex METRES_PER_FOOT.
constexpr double kMetersPerFoot = 0.3048;
// The fixture's A1 fix, in feet.
constexpr double kFixEastingFeet = 1568241.5;
constexpr double kFixNorthingFeet = 14534120.7;
constexpr double kFixElevationFeet = 5429.8;
// The fixture's base location ('@'), in meters.
constexpr double kBaseEasting = 478000.0;
constexpr double kBaseNorthing = 4430000.0;
constexpr double kBaseElevation = 1655.0;
// Where the node's own fixes put a station, in WGS84 UTM zone 13N.
constexpr double kNodeFixEasting = 478010.0;
constexpr double kNodeFixNorthing = 4430020.0;
constexpr double kNodeFixElevation = 1650.0;

//! "<e> <n> <z>" as the driver spells a coordinate.
QString triplet(double easting, double northing, double elevation)
{
    QString text;
    QTextStream stream(&text);
    cwSurvexExporterUtils::writeCoordTriplet(stream, easting, northing, elevation);
    stream.flush();
    return text;
}

QString fixtureDir()
{
    return QFileInfo(fixturePath(kZoneNoDatumMak)).absolutePath();
}

QString canonicalFixture(const QString& name)
{
    return QFileInfo(fixturePath(name)).canonicalFilePath();
}

cwCave* addAttachedCave(cwCavingRegion& region, const QString& name, const QString& entryFile)
{
    cwCave* cave = new cwCave();
    cave->setName(name);
    cave->setExternalCenterline(cwExternalCenterline(entryFile));
    region.addCave(cave);
    return cave;
}

QString driverFor(const cwCavingRegion& region, const cwSurvexExporterRegion::Options& options,
                  const QString& globalCS, QStringList* errors = nullptr)
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
    if (errors != nullptr) {
        *errors = task.errors();
    }
    return driver;
}

//! Fails unless every one of \a lines appears in \a driver, in that order.
void checkInOrder(const QString& driver, const QStringList& lines)
{
    qsizetype from = 0;
    for (const QString& line : lines) {
        INFO("expected, in order: " << line.toStdString());
        const qsizetype at = driver.indexOf(line, from);
        CHECK(at >= 0);
        if (at >= 0) {
            from = at + line.size();
        }
    }
}

cwFixStation utmFix(const QString& station, double easting, double northing, double elevation)
{
    cwFixStation fix;
    fix.setStationName(station);
    fix.setInputCS(kUtm13N);
    fix.setCoordinate(easting, northing, elevation);
    return fix;
}

} // namespace

TEST_CASE("A Compass datum and UTM zone map to the system cavern reads them in",
          "[cwSurvexExporter][Compass]")
{
    using cwCompassMakFile::utmCoordinateSystem;
    CHECK(utmCoordinateSystem(QStringLiteral("North American 1927"), 13) == QStringLiteral("EPSG:26713"));
    CHECK(utmCoordinateSystem(QStringLiteral("North American 1983"), 16) == QStringLiteral("EPSG:26916"));
    CHECK(utmCoordinateSystem(QStringLiteral("WGS 1984"), -20) == QStringLiteral("EPSG:32720"));
    CHECK(utmCoordinateSystem(QStringLiteral("North American 1927"), -13)
          == QStringLiteral("+proj=utm +zone=13 +south +datum=NAD27 +units=m +no_defs +type=crs"));
    CHECK(utmCoordinateSystem(QStringLiteral("UTM"), 13).isEmpty());
    CHECK(utmCoordinateSystem(QStringLiteral("North American 1927"), 0).isEmpty());
}

TEST_CASE("The driver reads a Compass .mak's fix through its datum and zone",
          "[cwSurvexExporter][Compass]")
{
    cwCavingRegion region;
    cwCave* cave = addAttachedCave(region, QStringLiteral("Zone"), kZoneNoDatumMak);
    cwSurvexExporterRegion::Options options;
    options.caveAttachmentDirs.insert(cave->id(), fixtureDir());

    const QString datInclude =
        QStringLiteral("*include \"%1\"").arg(canonicalFixture(QStringLiteral("compass_zone_no_datum.dat")));
    const QString fixLine = QStringLiteral("*fix A1 ")
        + triplet(kFixEastingFeet * kMetersPerFoot, kFixNorthingFeet * kMetersPerFoot,
                  kFixElevationFeet * kMetersPerFoot);

    SECTION("in a georeferenced run")
    {
        const QString driver = driverFor(region, options, kUtm13N);
        INFO("driver:\n" << driver.toStdString());

        CHECK_FALSE(driver.contains(QStringLiteral(".mak\"")));
        // The file's zone in Compass's default datum, North American 1927, and
        // its base location as the declination location.
        checkInOrder(driver, {QStringLiteral("*case preserve"),
                              QStringLiteral("*begin compass_zone_no_datum\n"),
                              QStringLiteral("*cs EPSG:26713\n"),
                              QStringLiteral("*declination auto ")
                                  + triplet(kBaseEasting, kBaseNorthing, kBaseElevation),
                              datInclude,
                              fixLine,
                              QStringLiteral("*entrance A1\n"),
                              QStringLiteral("*end compass_zone_no_datum\n")});
    }

    SECTION("with no output system, the fix is the raw coordinate and no location is set")
    {
        const QString driver = driverFor(region, options, QString());
        INFO("driver:\n" << driver.toStdString());

        CHECK_FALSE(driver.contains(QStringLiteral("*cs")));
        CHECK_FALSE(driver.contains(QStringLiteral("*declination auto")));
        checkInOrder(driver, {QStringLiteral("*begin compass_zone_no_datum\n"), datInclude, fixLine,
                              QStringLiteral("*entrance A1\n")});
    }
}

TEST_CASE("The driver gives each DAT of a .mak its own block and joins them at link stations",
          "[cwSurvexExporter][Compass]")
{
    cwCavingRegion region;
    cwCave* cave = addAttachedCave(region, QStringLiteral("Multi"), QStringLiteral("compass_multi.mak"));
    cwSurvexExporterRegion::Options options;
    options.caveAttachmentDirs.insert(cave->id(), fixtureDir());

    const QString driver = driverFor(region, options, QString());
    INFO("driver:\n" << driver.toStdString());

    CHECK_FALSE(driver.contains(QStringLiteral(".mak\"")));
    checkInOrder(driver,
                 {QStringLiteral("*begin compass_simple\n"),
                  QStringLiteral("*include \"%1\"").arg(canonicalFixture(QStringLiteral("compass_simple.dat"))),
                  QStringLiteral("*end compass_simple\n"),
                  QStringLiteral("*begin compass_other\n"),
                  QStringLiteral("*include \"%1\"").arg(canonicalFixture(QStringLiteral("compass_other.dat"))),
                  QStringLiteral("*end compass_other\n"),
                  QStringLiteral("*equate compass_simple.A2 compass_other.A2\n")});
    // A1 is linked by the first file, which has no earlier file to join; B1
    // and B2 are in no earlier file.
    CHECK(driver.count(QStringLiteral("*equate")) == 1);
}

TEST_CASE("The driver writes a node fix on a .mak station inside the station's block",
          "[cwSurvexExporter][Compass]")
{
    cwCavingRegion region;
    cwCave* cave = addAttachedCave(region, QStringLiteral("Zone"), kZoneNoDatumMak);
    cave->setExternalStations({QStringLiteral("compass_zone_no_datum.A1"),
                               QStringLiteral("compass_zone_no_datum.A2"),
                               QStringLiteral("compass_zone_no_datum.A3")});
    // Typed in another case than the file's: the block keeps Compass's case,
    // so the fix has to take the file's spelling to reach its station.
    cave->fixStations()->appendFixStation(
        utmFix(QStringLiteral("compass_zone_no_datum.a2"), kNodeFixEasting, kNodeFixNorthing, kNodeFixElevation));
    cave->fixStations()->appendFixStation(
        utmFix(QStringLiteral("compass_zone_no_datum.a1"), kNodeFixEasting, kNodeFixNorthing, kNodeFixElevation));

    cwSurvexExporterRegion::Options options;
    options.caveAttachmentDirs.insert(cave->id(), fixtureDir());
    options.externalFixedStations.insert(cave->id(), {QStringLiteral("compass_zone_no_datum.A1")});

    QStringList errors;
    const QString driver = driverFor(region, options, kUtm13N, &errors);
    INFO("driver:\n" << driver.toStdString());

    CHECK_FALSE(driver.contains(QStringLiteral("*fix compass_zone_no_datum.")));
    checkInOrder(driver, {QStringLiteral("*begin compass_zone_no_datum\n"),
                          QStringLiteral("*fix A1 "),
                          QStringLiteral("*cs EPSG:32613\n"),
                          QStringLiteral("*fix A2 ") + triplet(kNodeFixEasting, kNodeFixNorthing, kNodeFixElevation),
                          QStringLiteral("*end compass_zone_no_datum\n")});
    // The file fixes A1 itself, so the node's fix of it is refused.
    CHECK(driver.count(QStringLiteral("*fix A1 ")) == 1);
    CHECK(errors.contains(QStringLiteral(
        "compass_zone_no_datum.mak already fixes compass_zone_no_datum.a1; remove that fix from "
        "the file or fix another station")));
}

TEST_CASE("The driver translates a trip-attached .mak inside the trip's block",
          "[cwSurvexExporter][Compass]")
{
    cwCavingRegion region;
    cwCave* host = new cwCave();
    host->setName(QStringLiteral("Host"));
    region.addCave(host);
    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Attached"));
    trip->setExternalCenterline(cwExternalCenterline(kZoneNoDatumMak));
    host->addTrip(trip);

    cwSurvexExporterRegion::Options options;
    options.tripAttachmentDirs.insert(trip->id(), fixtureDir());

    const QString driver = driverFor(region, options, kUtm13N);
    INFO("driver:\n" << driver.toStdString());

    QString tripLabel = trip->scopePrefix();
    REQUIRE_FALSE(tripLabel.isEmpty());
    tripLabel.chop(1);
    CHECK_FALSE(driver.contains(QStringLiteral(".mak\"")));
    checkInOrder(driver, {QStringLiteral("*begin %1").arg(tripLabel),
                          QStringLiteral("*begin compass_zone_no_datum\n"),
                          QStringLiteral("*cs EPSG:26713\n"),
                          QStringLiteral("*fix A1 "),
                          QStringLiteral("*end compass_zone_no_datum\n"),
                          QStringLiteral("*end %1").arg(tripLabel)});
}

TEST_CASE("The driver includes a .mak as it is when Survex cannot spell its names",
          "[cwSurvexExporter][Compass]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir dir(tempDir.path());
    REQUIRE(QFile::copy(fixturePath(QStringLiteral("compass_zone_no_datum.dat")),
                        dir.absoluteFilePath(QStringLiteral("Upper.Cave.dat"))));
    // cavern names this DAT's survey "upper.cave", which a Survex *begin
    // would read as two levels.
    QFile mak(dir.absoluteFilePath(QStringLiteral("dotted.mak")));
    REQUIRE(mak.open(QFile::WriteOnly));
    mak.write(QByteArrayLiteral("$13;\n#Upper.Cave.dat,A1[m,478000,4430000,1655];\n"));
    mak.close();

    cwCavingRegion region;
    cwCave* cave = addAttachedCave(region, QStringLiteral("Dotted"), QStringLiteral("dotted.mak"));
    cwSurvexExporterRegion::Options options;
    options.caveAttachmentDirs.insert(cave->id(), tempDir.path());

    const QString driver = driverFor(region, options, kUtm13N);
    INFO("driver:\n" << driver.toStdString());

    CHECK(driver.contains(QStringLiteral("dotted.mak\"")));
    CHECK_FALSE(driver.contains(QStringLiteral("*begin upper")));
}

TEST_CASE("The driver reads a .mak fix that names no datum or zone in the output system",
          "[cwSurvexExporter][Compass]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir dir(tempDir.path());
    REQUIRE(QFile::copy(fixturePath(QStringLiteral("compass_zone_no_datum.dat")),
                        dir.absoluteFilePath(QStringLiteral("cave.dat"))));
    const QString makPath = dir.absoluteFilePath(QStringLiteral("plain.mak"));
    QFile mak(makPath);
    REQUIRE(mak.open(QFile::WriteOnly));
    mak.write(QByteArrayLiteral("#cave.dat,A1[m,478000,4430000,1655];\n"));
    mak.close();

    // A node fix written before the translation can leave another system in
    // force, here latitude and longitude.
    cwSurvexCS::SidecarWriter sidecars;
    cwSurvexExporterUtils::CsScope scope(sidecars);
    QString preamble;
    QTextStream preambleStream(&preamble);
    scope.ensure(preambleStream, kWgs84LatLong);

    const std::optional<QString> text = cwCompassMakTranslator::translate({makPath, kUtm13N, {}}, scope);
    REQUIRE(text.has_value());
    INFO("translation:\n" << text->toStdString());

    checkInOrder(*text, {QStringLiteral("*begin cave\n"),
                         QStringLiteral("*cs EPSG:32613\n"),
                         QStringLiteral("*fix A1 ") + triplet(kBaseEasting, kBaseNorthing, kBaseElevation),
                         QStringLiteral("*end cave\n")});
}

TEST_CASE("The driver includes a .mak as it is when a fix bracket is unreadable",
          "[cwSurvexExporter][Compass]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir dir(tempDir.path());
    REQUIRE(QFile::copy(fixturePath(QStringLiteral("compass_zone_no_datum.dat")),
                        dir.absoluteFilePath(QStringLiteral("cave.dat"))));
    // Two numbers where cavern reads three: cavern reports the bracket.
    QFile mak(dir.absoluteFilePath(QStringLiteral("short.mak")));
    REQUIRE(mak.open(QFile::WriteOnly));
    mak.write(QByteArrayLiteral("$13;\n#cave.dat,A1[m,478000,4430000];\n"));
    mak.close();

    cwCavingRegion region;
    cwCave* cave = addAttachedCave(region, QStringLiteral("Short"), QStringLiteral("short.mak"));
    cwSurvexExporterRegion::Options options;
    options.caveAttachmentDirs.insert(cave->id(), tempDir.path());

    const QString driver = driverFor(region, options, kUtm13N);
    INFO("driver:\n" << driver.toStdString());

    CHECK(driver.contains(QStringLiteral("short.mak\"")));
    CHECK_FALSE(driver.contains(QStringLiteral("*begin cave")));
}
