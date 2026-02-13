// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwSurveyChunk.h"
#include "cwSurveyChunkData.h"
#include "cwSurveyChunkSyncMergeHandler.h"

namespace {

cwStation makeStation(const QUuid& id,
                      const QString& name,
                      const QString& left = QString(),
                      const QString& right = QString(),
                      const QString& up = QString(),
                      const QString& down = QString())
{
    cwStation station;
    station.setId(id);
    station.setName(name);
    station.setLeft(left);
    station.setRight(right);
    station.setUp(up);
    station.setDown(down);
    return station;
}

cwShot makeShot(const QUuid& id,
                const QString& distance,
                const QString& compass,
                const QString& backCompass,
                const QString& clino,
                const QString& backClino,
                bool includeDistance)
{
    cwShot shot;
    shot.setId(id);
    shot.setDistance(distance);
    shot.setCompass(compass);
    shot.setBackCompass(backCompass);
    shot.setClino(clino);
    shot.setBackClino(backClino);
    shot.setDistanceIncluded(includeDistance);
    return shot;
}

} // namespace

TEST_CASE("cwSurveyChunk merge plan builder validates required objects", "[cwSurveyChunkMerge][sync]")
{
    cwSurveyChunk chunk;
    cwSurveyChunkData loadedData;
    loadedData.id = chunk.id();

    const auto missingCurrent =
        cwSurveyChunkSyncMergeHandler::buildSurveyChunkMergePlan(nullptr, &loadedData, {});
    CHECK(missingCurrent.hasError());

    const auto missingLoaded =
        cwSurveyChunkSyncMergeHandler::buildSurveyChunkMergePlan(&chunk, nullptr, {});
    CHECK(missingLoaded.hasError());
}

TEST_CASE("cwSurveyChunk merge applier merges stations and shots by stable id preserving order", "[cwSurveyChunkMerge][sync]")
{
    cwSurveyChunk currentChunk;

    const QUuid stationAId = QUuid::createUuid();
    const QUuid stationBId = QUuid::createUuid();
    const QUuid stationCId = QUuid::createUuid();

    const QUuid shotOneId = QUuid::createUuid();
    const QUuid shotTwoId = QUuid::createUuid();
    const QUuid shotThreeId = QUuid::createUuid();

    cwSurveyChunkData baseData;
    baseData.id = currentChunk.id();
    baseData.stations = {
        makeStation(stationAId, QStringLiteral("A0")),
        makeStation(stationBId, QStringLiteral("B0"))
    };
    baseData.shots = {
        makeShot(shotOneId, QStringLiteral("10.0"), QStringLiteral("11"), QStringLiteral("12"), QStringLiteral("13"), QStringLiteral("14"), true),
        makeShot(shotTwoId, QStringLiteral("20.0"), QStringLiteral("21"), QStringLiteral("22"), QStringLiteral("23"), QStringLiteral("24"), true)
    };
    currentChunk.setData(baseData);

    cwSurveyChunkData currentData = currentChunk.data();
    currentData.stations[1].setName(QStringLiteral("B-local"));
    currentData.shots[1].setDistance(QStringLiteral("20-local"));
    currentChunk.setData(currentData);

    cwSurveyChunkData loadedData = baseData;
    loadedData.stations = {
        makeStation(stationBId, QStringLiteral("B-remote")),
        makeStation(stationAId, QStringLiteral("A-remote")),
        makeStation(stationCId, QStringLiteral("C-remote"))
    };
    loadedData.shots = {
        makeShot(shotTwoId, QStringLiteral("20-remote"), QStringLiteral("21"), QStringLiteral("22"), QStringLiteral("23"), QStringLiteral("24"), false),
        makeShot(shotOneId, QStringLiteral("10.0"), QStringLiteral("11"), QStringLiteral("12"), QStringLiteral("13"), QStringLiteral("14"), false),
        makeShot(shotThreeId, QStringLiteral("30.0"), QStringLiteral("31"), QStringLiteral("32"), QStringLiteral("33"), QStringLiteral("34"), true)
    };

    const auto planResult =
        cwSurveyChunkSyncMergeHandler::buildSurveyChunkMergePlan(&currentChunk, &loadedData, baseData);
    REQUIRE_FALSE(planResult.hasError());
    REQUIRE_FALSE(cwSurveyChunkSyncMergeHandler::applySurveyChunkMergePlan(planResult.value()).hasError());

    const cwSurveyChunkData mergedData = currentChunk.data();
    REQUIRE(mergedData.stations.size() == 3);
    CHECK(mergedData.stations[0].id() == stationBId);
    CHECK(mergedData.stations[1].id() == stationAId);
    CHECK(mergedData.stations[2].id() == stationCId);
    CHECK(mergedData.stations[0].name() == QStringLiteral("B-local"));
    CHECK(mergedData.stations[1].name() == QStringLiteral("A-remote"));
    CHECK(mergedData.stations[2].name() == QStringLiteral("C-remote"));

    REQUIRE(mergedData.shots.size() == 3);
    CHECK(mergedData.shots[0].id() == shotTwoId);
    CHECK(mergedData.shots[1].id() == shotOneId);
    CHECK(mergedData.shots[2].id() == shotThreeId);
    CHECK(mergedData.shots[0].distance().value() == QStringLiteral("20-local"));
    CHECK(mergedData.shots[1].isDistanceIncluded() == false);
    CHECK(mergedData.shots[2].distance().value() == QStringLiteral("30.0"));
}

TEST_CASE("cwSurveyChunk merge applier rejects ambiguous station ids", "[cwSurveyChunkMerge][sync]")
{
    cwSurveyChunk currentChunk;
    cwSurveyChunkData currentData = currentChunk.data();
    const QUuid sharedId = QUuid::createUuid();
    currentData.stations = {
        makeStation(sharedId, QStringLiteral("A0")),
        makeStation(QUuid::createUuid(), QStringLiteral("B0"))
    };
    currentChunk.setData(currentData);

    cwSurveyChunkData loadedData = currentData;
    loadedData.stations = {
        makeStation(sharedId, QStringLiteral("A-remote")),
        makeStation(sharedId, QStringLiteral("A-duplicate"))
    };

    const auto planResult =
        cwSurveyChunkSyncMergeHandler::buildSurveyChunkMergePlan(&currentChunk, &loadedData, {});
    REQUIRE_FALSE(planResult.hasError());
    const auto applyResult = cwSurveyChunkSyncMergeHandler::applySurveyChunkMergePlan(planResult.value());
    CHECK(applyResult.hasError());
    CHECK(applyResult.errorMessage() == QStringLiteral("Ambiguous ids in chunk payload list."));
}
