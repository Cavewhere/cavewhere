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
#include "cwShot.h"
#include "cwDistanceReading.h"

//AsyncFuture includes
#include "asyncfuture.h"

//Qt includes
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QFile>
#include <QFuture>

namespace {

//Writes a Compass .dat that reproduces cavewhere issue #569.
//
//The FORMAT string DDDDLRUDLAaDdNF describes a 15-character Compass layout.
//The 14th character ('N') means NO redundant backsights, so the data lines
//carry no AZM2/INC2 columns - only FROM TO LEN BEAR INC L U D R then the
//shot flags. The lowercase 'a'/'d' in the shot-order section (positions 11
//and 13) are part of every 15-char format's measurement-order spec and do
//NOT imply the shot recorded backsights.
//
//Compass writes shot flags between a "#|" marker and a trailing "#". When the
//flag set is empty Compass emits "#| #" - a space between the markers. The
//importer splits the data line on whitespace, so "#| #" becomes two tokens
//("#|" and "#") that land exactly where AZM2/INC2 would sit. The importer,
//having wrongly decided this survey has backsights, then tries to parse "#|"
//as a backsight azimuth, fails, and discards the entire cave.
QString writeCompassFile(const QTemporaryDir& dir)
{
    const QString path = dir.filePath("flagsImport.dat");

    const QString contents = QStringLiteral(
        "Test Cave\n"
        "SURVEY NAME: A\n"
        "SURVEY DATE: 1 1 2024\n"
        "SURVEY TEAM:\n"
        "Tester\n"
        "DECLINATION: 0.00  FORMAT: DDDDLRUDLAaDdNF  CORRECTIONS: 0.00 0.00 0.00\n"
        "\n"
        "FROM TO LENGTH BEARING INC LEFT UP DOWN RIGHT FLAGS COMMENTS\n"
        "\n"
        "A1 A2 16.70 323.90 30.40 -9.90 -9.90 -9.90 -9.90 #| #\n"
        "A2 A3 19.60 56.40 1.60 5.00 2.00 5.50 5.50 #|L#\n");

    QFile file(path);
    REQUIRE(file.open(QFile::WriteOnly));
    file.write(contents.toLocal8Bit());
    file.close();

    return path;
}

} // namespace

TEST_CASE("Compass import handles shot flags in a non-backsight format", "[Compass]") {

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString datFile = writeCompassFile(dir);

    auto future = cwCompassImporter::run(QStringList() << datFile);

    //A bounded wait rather than the plain waitForFinished(): the buggy importer
    //spins forever (a shot fails to parse, then parseSurvey early-returns
    //without advancing the file, so parseFile's loop never terminates).
    constexpr int kTimeoutMs = 15000;
    if(!AsyncFuture::waitForFinished(future, kTimeoutMs)) {
        //Break the runaway loop via the isCanceled() check so the worker thread
        //doesn't leak past the test, then fail cleanly instead of reading
        //result() off a canceled future.
        future.cancel();
        future.waitForFinished();
        FAIL("Compass import did not finish within the timeout");
    }

    const auto result = future.result();

    //The whole cave used to be discarded because the "#|" flag marker was
    //misread as a backsight azimuth.
    QList<cwCaveData> caveData = result.caves;
    REQUIRE(caveData.size() == 1);

    auto cave = std::make_unique<cwCave>();
    cave->setData(caveData.first());

    REQUIRE(cave->trips().size() == 1);
    cwTrip* trip = cave->trips().first();

    //FORMAT ...NF has no redundant backsights (see writeCompassFile).
    CHECK_FALSE(trip->calibrations()->hasBackSights());

    REQUIRE(trip->chunks().size() == 1);
    cwSurveyChunk* chunk = trip->chunks().first();

    //Two shots (A1->A2->A3) must survive the import.
    REQUIRE(chunk->shotCount() == 2);

    const cwShot firstShot = chunk->shots().at(0);
    CHECK(firstShot.distance().toDouble() == Catch::Approx(16.70));
    CHECK(firstShot.compass().toDouble() == Catch::Approx(323.90));
    CHECK(firstShot.clino().toDouble() == Catch::Approx(30.40));
    //Empty "#| #" flags must not exclude the length.
    CHECK(firstShot.isDistanceIncluded());

    const cwShot secondShot = chunk->shots().at(1);
    CHECK(secondShot.distance().toDouble() == Catch::Approx(19.60));
    //The "L" flag excludes the length from the calculation.
    CHECK_FALSE(secondShot.isDistanceIncluded());
}
