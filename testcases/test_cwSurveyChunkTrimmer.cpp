/**************************************************************************
**
**    Copyright (C) 2026
**
**************************************************************************/

//Catch includes
#define CATCH_CONFIG_SFINAE
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwSurveyChunk.h"
#include "cwSurveyChunkTrimmer.h"

TEST_CASE("cwSurveyChunkTrimmer trims trailing empty station on chunk change", "[cwSurveyChunkTrimmer]") {
    cwSurveyChunk chunk;
    cwSurveyChunk other;

    chunk.appendNewShot();
    REQUIRE(chunk.stationCount() == 2);
    REQUIRE(chunk.shotCount() == 1);

    chunk.setData(cwSurveyChunk::StationNameRole, 0, "A0");
    chunk.setData(cwSurveyChunk::StationNameRole, 1, "A1");
    chunk.setData(cwSurveyChunk::ShotDistanceRole, 0, 10);

    chunk.insertStation(1, cwSurveyChunk::Below);

    REQUIRE(chunk.stationCount() == 3);
    REQUIRE(chunk.shotCount() == 2);

    cwSurveyChunkTrimmer trimmer;
    trimmer.setChunk(&chunk);
    trimmer.setChunk(&other);

    CHECK(chunk.stationCount() == 2);
    CHECK(chunk.shotCount() == 1);
}
