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
#include "cwDistanceReading.h"

//Qt includes
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QFile>
#include "cwSignalSpy.h"

namespace {

//Writes a Compass .dat with a single feet survey whose LRUD values are chosen
//to expose the old importer bug, which reformatted each value with
//QString::number(value, 'g', 2): values >= 100 came out in scientific notation
//(110.00 -> "1.1e+02", which Survex can't parse) and values needing three
//significant figures were truncated (35.70 -> "36"). The data line columns are
//LEFT UP DOWN RIGHT.
QString writeCompassFile(const QTemporaryDir& dir)
{
    const QString path = dir.filePath("lrudImport.dat");

    const QString contents = QStringLiteral(
        "Test Cave\n"
        "SURVEY NAME: A\n"
        "SURVEY DATE: 1 1 2024\n"
        "SURVEY TEAM:\n"
        "Tester\n"
        "DECLINATION: 0.00  FORMAT: DDDDLRUDLADadBT\n"
        "\n"
        "FROM TO LEN BEAR INC LEFT UP DOWN RIGHT AZM2 INC2 FLAGS COMMENTS\n"
        "\n"
        "A1 A2 20.00 90.00 0.00 110.00 35.70 1.10 8.49 270.00 0.00\n");

    QFile file(path);
    REQUIRE(file.open(QFile::WriteOnly));
    file.write(contents.toLocal8Bit());
    file.close();

    return path;
}

} // namespace

TEST_CASE("Compass LRUD import keeps full precision and plain decimals", "[Compass]") {

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

    REQUIRE(cave->trips().size() == 1);
    cwSurveyChunk* chunk = cave->trips().first()->chunks().first();
    REQUIRE(chunk->stationCount() >= 1);

    const cwStation station = chunk->stations().at(0);

    //A value >= 100 used to be stored as scientific notation ("1.1e+02"), which
    //Survex's parser rejects. The raw decimal string must be kept.
    CHECK(station.left().toDouble() == Catch::Approx(110.00));
    CHECK_FALSE(station.left().value().contains(QLatin1Char('e'), Qt::CaseInsensitive));

    //A value needing three significant figures used to be truncated (35.70 ->
    //"36"). Full precision must be preserved.
    CHECK(station.up().toDouble() == Catch::Approx(35.70));

    CHECK(station.down().toDouble() == Catch::Approx(1.10));
    CHECK(station.right().toDouble() == Catch::Approx(8.49));
}
