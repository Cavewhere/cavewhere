#include <catch2/catch_test_macros.hpp>
#include "cwWallsImporter.h"
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
