#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "cwWallsImporter.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "wallsunits.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwCave.h"
#include "cwSurveyChunk.h"
#include "cwTreeImportData.h"
#include "cwTreeImportDataNode.h"
#include "LoadProjectHelper.h"

#include <QMutex>
#include <QMutexLocker>
#include <QStringList>

#include <algorithm>
#include <memory>

typedef UnitizedDouble<Length> ULength;
typedef UnitizedDouble<Angle> UAngle;

using namespace dewalls;

TEST_CASE( "importCalibrations", "[cwWallsImporter]" ) {
    WallsUnits units;
    cwTrip trip;

    Length::Unit dUnit = Length::Feet;
    ULength incd(5, Length::Meters);
    UAngle inca(42, Angle::Gradians);
    UAngle incab(0.253, Angle::Radians);
    UAngle incv(36, Angle::PercentGrade);
    UAngle incvb(25, Angle::Degrees);
    UAngle decl(24, Angle::Gradians);

    units.setDUnit(dUnit);
    units.setIncd(incd);
    units.setInca(inca);
    units.setIncab(incab);
    units.setIncv(incv);
    units.setIncvb(incvb);
    units.setTypeabCorrected(true);
    units.setTypevbCorrected(true);
    units.setDecl(decl);

    cwWallsImporter::importCalibrations(units, trip);

    CHECK( trip.calibrations()->hasCorrectedCompassBacksight() );
    CHECK( trip.calibrations()->hasCorrectedClinoBacksight() );
    CHECK( trip.calibrations()->distanceUnit() == cwUnits::LengthUnit::Feet );
    CHECK( trip.calibrations()->tapeCalibration() == incd.get(dUnit) );
    CHECK( trip.calibrations()->frontCompassCalibration() == inca.get(Angle::Degrees) );
    CHECK( trip.calibrations()->backCompassCalibration() == incab.get(Angle::Degrees) );
    CHECK( trip.calibrations()->frontClinoCalibration() == incv.get(Angle::Degrees) );
    CHECK( trip.calibrations()->backClinoCalibration() == incvb.get(Angle::Degrees) );
    CHECK( trip.calibrations()->declination() == decl.get(Angle::Degrees) );

    incd = ULength(4, Length::Feet);
    dUnit = Length::Meters;
    units.setDUnit(dUnit);
    units.setIncd(incd);
    cwWallsImporter::importCalibrations(units, trip);
    CHECK( trip.calibrations()->distanceUnit() == cwUnits::Meters );
    CHECK( trip.calibrations()->tapeCalibration() == incd.get(dUnit) );

    units.setTypeabCorrected(false);
    units.setTypevbCorrected(true);
    cwWallsImporter::importCalibrations(units, trip);
    CHECK( !trip.calibrations()->hasCorrectedCompassBacksight() );
    CHECK( trip.calibrations()->hasCorrectedClinoBacksight() );

    units.setTypeabCorrected(false);
    units.setTypevbCorrected(false);
    cwWallsImporter::importCalibrations(units, trip);
    CHECK( !trip.calibrations()->hasCorrectedCompassBacksight() );
    CHECK( !trip.calibrations()->hasCorrectedClinoBacksight() );

    units.setTypeabCorrected(true);
    units.setTypevbCorrected(false);
    cwWallsImporter::importCalibrations(units, trip);
    CHECK( trip.calibrations()->hasCorrectedCompassBacksight() );
    CHECK( !trip.calibrations()->hasCorrectedClinoBacksight() );
}

namespace {
    QMutex g_wallsWarningsMutex;
    QStringList g_wallsWarnings;

    void captureWarningsHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
        if(type == QtWarningMsg) {
            QMutexLocker lock(&g_wallsWarningsMutex);
            g_wallsWarnings << msg;
        }
    }

    //RAII so a failed assertion can't leak our handler into later test cases.
    struct WarningCapture {
        QtMessageHandler previous;
        WarningCapture() {
            QMutexLocker lock(&g_wallsWarningsMutex);
            g_wallsWarnings.clear();
            previous = qInstallMessageHandler(captureWarningsHandler);
        }
        ~WarningCapture() {
            qInstallMessageHandler(previous);
        }
        QStringList captured() const {
            QMutexLocker lock(&g_wallsWarningsMutex);
            return g_wallsWarnings;
        }
    };
}

TEST_CASE("Import a Walls .wpj project end-to-end", "[cwWallsImporter]") {
    WarningCapture warnings;

    std::unique_ptr<cwWallsImporter> importer(new cwWallsImporter());
    importer->setInputFiles(QStringList() << testcasesDatasetSourcePath("walls/test_cave.wpj"));
    importer->start();
    importer->waitToFinish();

    //The importer should not emit any Qt warnings; the cross-thread setParent
    //regression appeared as such a warning and silently dropped imported data.
    const QStringList captured = warnings.captured();
    for(const QString& w : captured) {
        INFO("Unexpected Qt warning: " << w.toStdString());
    }
    CHECK(captured.isEmpty());

    REQUIRE_FALSE(importer->hasParseErrors());
    REQUIRE(importer->parseErrors().isEmpty());

    cwTreeImportData* data = importer->data();
    REQUIRE(data->nodes().size() == 1);

    cwTreeImportDataNode* root = data->nodes().first();
    root->setImportType(cwTreeImportDataNode::Cave);
    REQUIRE(root->childNodeCount() == 1);
    root->childNode(0)->setImportType(cwTreeImportDataNode::Trip);

    QList<cwCave*> caves = data->caves();
    REQUIRE(caves.size() == 1);
    REQUIRE(caves.first()->trips().size() == 1);

    cwTrip* trip = caves.first()->trips().first();
    REQUIRE(trip->chunks().size() == 1);

    cwSurveyChunk* chunk = trip->chunk(0);
    REQUIRE(chunk->stationCount() == 4);
    CHECK(chunk->station(0).name() == "A1");
    CHECK(chunk->station(1).name() == "A2");
    CHECK(chunk->station(2).name() == "A3");
    CHECK(chunk->station(3).name() == "A4");
}

namespace {
    std::unique_ptr<cwWallsImporter> importWalls(const QString& dataset)
    {
        std::unique_ptr<cwWallsImporter> importer(new cwWallsImporter());
        importer->setInputFiles(QStringList() << testcasesDatasetSourcePath(dataset));
        importer->start();
        importer->waitToFinish();

        REQUIRE_FALSE(importer->hasParseErrors());

        return importer;
    }

    QList<cwFixStation> importedFixStations(const QString& dataset)
    {
        return importWalls(dataset)->capturedFixStations();
    }

    //A3 is written as an angle pair, so it lands in the same place whichever
    //datum it's read in — only the system it declares differs.
    void checkGeoFixAt(const cwFixStation& fix, const QString& expectedCS)
    {
        CHECK(fix.stationName() == "A3");
        CHECK(fix.inputCS() == expectedCS);
        CHECK(fix.state() == cwFixStation::Valid);
        CHECK(fix.easting() == Catch::Approx(-86.0914594).margin(0.000001));
        CHECK(fix.northing() == Catch::Approx(37.1675542).margin(0.000001));
    }
}

TEST_CASE("Walls #FIX stations take their coordinate system from the project's .REF",
          "[cwWallsImporter]") {
    const QList<cwFixStation> fixes = importedFixStations("walls/georef_cave.wpj");
    REQUIRE(fixes.size() == 2);

    //Grid coordinates are read in the .REF's datum and UTM zone: NAD83, zone
    //16, mapped through img.c's table.
    const cwFixStation rectFix = fixes.at(0);
    CHECK(rectFix.stationName() == "A1");
    CHECK(rectFix.inputCS() == QStringLiteral("EPSG:26916"));
    CHECK(rectFix.state() == cwFixStation::Valid);
    CHECK(rectFix.easting() == Catch::Approx(580661.570));
    CHECK(rectFix.northing() == Catch::Approx(4113846.340));
    CHECK(rectFix.elevation() == Catch::Approx(219.0));

    //Latitude and longitude get the geodetic CRS for NAD83 rather than the UTM
    //zone, which doesn't enter into an angle pair.
    checkGeoFixAt(fixes.at(1), QStringLiteral("EPSG:4269"));
}

TEST_CASE("A Walls survey imported without its project file has no .REF to read fixes in",
          "[cwWallsImporter]") {
    const QList<cwFixStation> fixes = importedFixStations("walls/GEOREF.SRV");
    REQUIRE(fixes.size() == 2);

    //Nothing in a .srv says which zone its grid coordinates are in, and a guess
    //would place the cave hundreds of kilometers off, so the fix says it has no
    //system and the user picks one.
    const cwFixStation rectFix = fixes.at(0);
    CHECK(rectFix.inputCS().isEmpty());
    CHECK(rectFix.state() == cwFixStation::NoSystem);
    //The numbers survive as text meanwhile, to be read once there's a system to
    //read them under.
    CHECK(rectFix.coordinate().contains(QStringLiteral("580661.57")));
    CHECK(rectFix.coordinate().contains(QStringLiteral("4113846.34")));

    //An angle pair still places the station whatever the datum, so WGS84 leaves
    //a usable coordinate rather than none.
    checkGeoFixAt(fixes.at(1), cwCoordinateTransform::Wgs84);
}

TEST_CASE("A Walls .REF naming a datum CaveWhere can't map says which datum it was",
          "[cwWallsImporter]") {
    const std::unique_ptr<cwWallsImporter> importer = importWalls("walls/unmapped_datum_cave.wpj");
    const QList<cwFixStation> fixes = importer->capturedFixStations();
    REQUIRE(fixes.size() == 2);

    //The .REF gives a zone, so the reason these coordinates are unplaceable is
    //the datum — and the warning has to name it, or the user goes looking in
    //the wrong part of the file.
    CHECK(fixes.at(0).inputCS().isEmpty());
    CHECK(fixes.at(0).state() == cwFixStation::NoSystem);
    CHECK(fixes.at(1).inputCS() == cwCoordinateTransform::Wgs84);

    const QStringList errors = importer->importErrors();
    const auto namesTheDatum = [&errors](const QString& station) {
        return std::any_of(errors.begin(), errors.end(), [&](const QString& error) {
            return error.contains(station) && error.contains(QStringLiteral("Ordnance Survey 1936"));
        });
    };
    CHECK(namesTheDatum(QStringLiteral("A1")));
    CHECK(namesTheDatum(QStringLiteral("A3")));
}
