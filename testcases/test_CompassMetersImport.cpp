/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Cavewhere includes
#include "cwCompassImporter.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwDistanceReading.h"
#include "cwUnits.h"

//Qt includes
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QFile>
#include "cwSignalSpy.h"

namespace {

//Writes a Compass .dat file with a metric survey (DMMD format) and a decimal
//feet survey (DDDD format) and returns the file path. The metric survey's
//length and LRUD values are stored in meters; the feet survey's in decimal
//feet - this mirrors how Compass stores measurements in the format's units.
QString writeCompassFile(const QTemporaryDir& dir)
{
    const QString path = dir.filePath("metersImport.dat");

    const QString contents = QStringLiteral(
        "Test Cave\n"
        "SURVEY NAME: M\n"
        "SURVEY DATE: 1 1 2024\n"
        "SURVEY TEAM:\n"
        "Tester\n"
        "DECLINATION: 0.00  FORMAT: DMMDLRUDLADadBT\n"
        "\n"
        "FROM TO LEN BEAR INC LEFT UP DOWN RIGHT AZM2 INC2 FLAGS COMMENTS\n"
        "\n"
        "M1 M2 15.55 178.00 0.00 4.00 2.00 1.00 3.00 358.00 0.00\n"
        "M2 M3 12.00 180.00 0.00 5.00 2.00 1.00 3.00 0.00 0.00\n"
        "\f\n"
        "Test Cave\n"
        "SURVEY NAME: F\n"
        "SURVEY DATE: 1 1 2024\n"
        "SURVEY TEAM:\n"
        "Tester\n"
        "DECLINATION: 0.00  FORMAT: DDDDLRUDLADadBT\n"
        "\n"
        "FROM TO LEN BEAR INC LEFT UP DOWN RIGHT AZM2 INC2 FLAGS COMMENTS\n"
        "\n"
        "F1 F2 20.00 90.00 0.00 5.00 5.00 5.00 5.00 270.00 0.00\n");

    QFile file(path);
    REQUIRE(file.open(QFile::WriteOnly));
    file.write(contents.toLocal8Bit());
    file.close();

    return path;
}

} // namespace

TEST_CASE("Compass meters import keeps native units - ISSUE meters double-conversion", "[Compass]") {

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString datFile = writeCompassFile(dir);

    auto importer = std::make_unique<cwCompassImporter>();
    cwSignalSpy messageSpy(importer.get(), SIGNAL(statusMessage(QString)));
    importer->setCompassDataFiles(QStringList() << datFile);
    importer->start();
    importer->waitToFinish();

    QList<cwCaveData> caveData = importer->caves();
    REQUIRE(caveData.size() == 1);

    auto cave = std::make_unique<cwCave>();
    cave->setData(caveData.first());

    REQUIRE(cave->trips().size() == 2);

    //--- Metric survey: values are stored in meters and must stay in meters ---
    cwTrip* metricTrip = cave->trips().at(0);
    REQUIRE(metricTrip->name() == "M");
    CHECK(metricTrip->calibrations()->distanceUnit() == cwUnits::Meters);
    REQUIRE(metricTrip->chunks().size() == 1);

    cwSurveyChunk* metricChunk = metricTrip->chunks().first();
    REQUIRE(metricChunk->shotCount() == 2);

    //A 15.55 m shot must import as 15.55 m, not 15.55 * 0.3048 = 4.74 m.
    CHECK(metricChunk->shots().at(0).distance().toDouble() == Catch::Approx(15.55));
    CHECK(metricChunk->shots().at(1).distance().toDouble() == Catch::Approx(12.00));

    //LRUD on the metric survey is likewise already in meters.
    CHECK(metricChunk->stations().at(0).left().toDouble() == Catch::Approx(4.00));
    CHECK(metricChunk->stations().at(0).up().toDouble() == Catch::Approx(2.00));
    CHECK(metricChunk->stations().at(0).down().toDouble() == Catch::Approx(1.00));
    CHECK(metricChunk->stations().at(0).right().toDouble() == Catch::Approx(3.00));

    //--- Feet survey: decimal feet must be unchanged ------------------------
    cwTrip* feetTrip = cave->trips().at(1);
    REQUIRE(feetTrip->name() == "F");
    CHECK(feetTrip->calibrations()->distanceUnit() == cwUnits::Feet);
    REQUIRE(feetTrip->chunks().size() == 1);

    cwSurveyChunk* feetChunk = feetTrip->chunks().first();
    REQUIRE(feetChunk->shotCount() == 1);
    CHECK(feetChunk->shots().at(0).distance().toDouble() == Catch::Approx(20.00));
    CHECK(feetChunk->stations().at(0).left().toDouble() == Catch::Approx(5.00));
}
