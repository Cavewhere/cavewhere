#include <catch2/catch_test_macros.hpp>

#include "cwNoteLiDAR.h"
#include "cwTrip.h"
#include "cwKeywordModel.h"

TEST_CASE("cwNoteLiDAR keywords include type, file name, and trip", "[cwNoteLiDAR]") {
    cwTrip trip;
    trip.setName(QStringLiteral("Trip A"));

    cwNoteLiDAR note;
    note.setParentTrip(&trip);
    note.setFilename(QStringLiteral("scan.glb"));

    auto keywords = note.keywordModel()->keywords();
    CHECK(keywords.contains(cwKeyword(cwKeywordModel::TypeKey, QStringLiteral("LiDAR"))));
    CHECK(keywords.contains(cwKeyword(cwKeywordModel::FileNameKey, QStringLiteral("scan.glb"))));
    CHECK(keywords.contains(cwKeyword(cwKeywordModel::TripNameKey, QStringLiteral("Trip A"))));

    note.setFilename(QStringLiteral("scan2.glb"));
    keywords = note.keywordModel()->keywords();
    CHECK(keywords.contains(cwKeyword(cwKeywordModel::FileNameKey, QStringLiteral("scan2.glb"))));
}
