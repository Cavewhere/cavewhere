// Catch includes
#include <catch2/catch_test_macros.hpp>
using namespace Catch;

// Our includes
#include "cwTeamMember.h"
#include "cwTrip.h"
#include "cwTripMergeApplier.h"
#include "cwTripMergePlanBuilder.h"
#include "cwSurveyChunk.h"
#include "cwSurveyChunkData.h"

namespace {

cwTripData makeTripDataWithIdentity(const cwTrip& trip)
{
    return trip.data();
}

cwSurveyChunkData makeChunkData(const QUuid& chunkId,
                                const QUuid& firstStationId,
                                const QUuid& secondStationId,
                                const QUuid& shotId,
                                const QString& firstStationName,
                                const QString& secondStationName,
                                const QString& shotDistance)
{
    cwSurveyChunkData chunkData;
    chunkData.id = chunkId;

    cwStation firstStation;
    firstStation.setId(firstStationId);
    firstStation.setName(firstStationName);

    cwStation secondStation;
    secondStation.setId(secondStationId);
    secondStation.setName(secondStationName);

    cwShot shot;
    shot.setId(shotId);
    shot.setDistance(shotDistance);
    shot.setCompass(QStringLiteral("0"));
    shot.setBackCompass(QStringLiteral("180"));
    shot.setClino(QStringLiteral("0"));
    shot.setBackClino(QStringLiteral("0"));
    shot.setDistanceIncluded(true);

    chunkData.stations = {firstStation, secondStation};
    chunkData.shots = {shot};
    return chunkData;
}

void addChunkWithData(cwTrip& trip, const cwSurveyChunkData& chunkData)
{
    auto* chunk = new cwSurveyChunk();
    chunk->setData(chunkData);
    trip.addChunk(chunk);
}

} // namespace

TEST_CASE("cwTrip merge plan builder maps loaded trips by stable id", "[cwTripMerge][sync]")
{
    cwTrip firstTrip;
    cwTrip secondTrip;

    const cwTripData firstLoaded = makeTripDataWithIdentity(firstTrip);
    const cwTripData secondLoaded = makeTripDataWithIdentity(secondTrip);

    const auto preparation = cwTripMergePlanBuilder::build({&firstTrip, &secondTrip},
                                                            {&secondLoaded, &firstLoaded},
                                                            {});
    REQUIRE_FALSE(preparation.hasError());
    REQUIRE(preparation.value().plans.size() == 2);
    CHECK(preparation.value().plans[0].currentTrip == &secondTrip);
    CHECK(preparation.value().plans[0].loadedTripData.id == secondLoaded.id);
    CHECK(preparation.value().plans[1].currentTrip == &firstTrip);
    CHECK(preparation.value().plans[1].loadedTripData.id == firstLoaded.id);
}

TEST_CASE("cwTrip merge plan builder rejects ambiguous loaded trip ids", "[cwTripMerge][sync]")
{
    cwTrip firstTrip;
    cwTrip secondTrip;

    cwTripData firstLoaded = makeTripDataWithIdentity(firstTrip);
    cwTripData secondLoaded = makeTripDataWithIdentity(secondTrip);
    secondLoaded.id = firstLoaded.id;

    const auto preparation = cwTripMergePlanBuilder::build({&firstTrip, &secondTrip},
                                                            {&firstLoaded, &secondLoaded},
                                                            {});
    CHECK(preparation.hasError());
    CHECK(preparation.errorMessage() == QStringLiteral("Ambiguous loaded trip ids."));
}

TEST_CASE("cwTrip merge applier merges name and date bundles independently", "[cwTripMerge][sync]")
{
    cwTrip currentTrip;
    cwTripData baseTripData = currentTrip.data();
    baseTripData.name = QStringLiteral("base-name");
    baseTripData.date = QDateTime(QDate(2020, 1, 1), QTime());
    currentTrip.setName(baseTripData.name);
    currentTrip.setDate(baseTripData.date);

    cwTripData loadedTripData = currentTrip.data();
    loadedTripData.name = baseTripData.name;
    loadedTripData.date = QDateTime(QDate(2024, 5, 10), QTime());

    currentTrip.setName(QStringLiteral("ours-name"));

    cwTripMergePlan plan;
    plan.currentTrip = &currentTrip;
    plan.loadedTripData = loadedTripData;
    plan.baseTripData = baseTripData;

    const auto applyResult = cwTripMergeApplier::applyTripMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());
    CHECK(currentTrip.name() == QStringLiteral("ours-name"));
    CHECK(currentTrip.date() == QDateTime(QDate(2024, 5, 10), QTime()));
}

TEST_CASE("cwTrip merge applier merges calibration fields independently", "[cwTripMerge][sync]")
{
    cwTrip currentTrip;

    cwTripData baseTripData = currentTrip.data();
    baseTripData.calibrations.setTapeCalibration(1.0);
    baseTripData.calibrations.setDeclination(2.0);
    baseTripData.calibrations.setFrontSights(true);
    currentTrip.calibrations()->setData(baseTripData.calibrations);

    cwTripData loadedTripData = currentTrip.data();
    loadedTripData.calibrations.setTapeCalibration(3.0); // Remote-only change.
    loadedTripData.calibrations.setDeclination(2.0); // Unchanged from base.
    loadedTripData.calibrations.setFrontSights(false); // Remote-only change.

    cwTripCalibrationData oursCalibration = currentTrip.calibrations()->data();
    oursCalibration.setDeclination(4.0); // Local-only change.
    currentTrip.calibrations()->setData(oursCalibration);

    cwTripMergePlan plan;
    plan.currentTrip = &currentTrip;
    plan.loadedTripData = loadedTripData;
    plan.baseTripData = baseTripData;

    const auto applyResult = cwTripMergeApplier::applyTripMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());

    const cwTripCalibrationData merged = currentTrip.calibrations()->data();
    CHECK(merged.tapeCalibration() == 3.0);
    CHECK(merged.declination() == 4.0);
    CHECK(merged.hasFrontSights() == false);
}

TEST_CASE("cwTrip merge applier returns error when trip chunk ids are ambiguous", "[cwTripMerge][sync]")
{
    cwTrip currentTrip;
    cwTripData loadedTripData = currentTrip.data();

    cwSurveyChunkData duplicateChunkA;
    duplicateChunkA.id = QUuid::createUuid();
    cwSurveyChunkData duplicateChunkB;
    duplicateChunkB.id = duplicateChunkA.id;
    loadedTripData.chunks = {duplicateChunkA, duplicateChunkB};

    cwTripMergePlan plan;
    plan.currentTrip = &currentTrip;
    plan.loadedTripData = loadedTripData;

    const auto applyResult = cwTripMergeApplier::applyTripMergePlan(plan);
    CHECK(applyResult.hasError());
    CHECK(applyResult.errorMessage() == QStringLiteral("Ambiguous ids in trip chunk list."));
}

TEST_CASE("cwTrip merge applier structurally merges chunk list by stable id with deterministic order", "[cwTripMerge][sync]")
{
    cwTrip currentTrip;

    const QUuid chunkAId = QUuid::createUuid();
    const QUuid chunkBId = QUuid::createUuid();
    const QUuid chunkCId = QUuid::createUuid();
    const QUuid chunkDId = QUuid::createUuid();
    const QUuid chunkEId = QUuid::createUuid();

    const QUuid stationA0Id = QUuid::createUuid();
    const QUuid stationA1Id = QUuid::createUuid();
    const QUuid shotAId = QUuid::createUuid();
    const QUuid stationB0Id = QUuid::createUuid();
    const QUuid stationB1Id = QUuid::createUuid();
    const QUuid shotBId = QUuid::createUuid();

    const cwSurveyChunkData chunkABase = makeChunkData(
        chunkAId,
        stationA0Id,
        stationA1Id,
        shotAId,
        QStringLiteral("A0"),
        QStringLiteral("A1"),
        QStringLiteral("10.0"));
    const cwSurveyChunkData chunkBBase = makeChunkData(
        chunkBId,
        stationB0Id,
        stationB1Id,
        shotBId,
        QStringLiteral("B0"),
        QStringLiteral("B1"),
        QStringLiteral("20.0"));
    const cwSurveyChunkData chunkEBase = makeChunkData(
        chunkEId,
        QUuid::createUuid(),
        QUuid::createUuid(),
        QUuid::createUuid(),
        QStringLiteral("E0"),
        QStringLiteral("E1"),
        QStringLiteral("50.0"));

    cwSurveyChunkData chunkACurrent = chunkABase;
    chunkACurrent.stations[0].setName(QStringLiteral("A-local"));
    const cwSurveyChunkData chunkBCurrent = chunkBBase;
    const cwSurveyChunkData chunkCCurrent = makeChunkData(
        chunkCId,
        QUuid::createUuid(),
        QUuid::createUuid(),
        QUuid::createUuid(),
        QStringLiteral("C0"),
        QStringLiteral("C1"),
        QStringLiteral("30.0"));

    addChunkWithData(currentTrip, chunkBCurrent);
    addChunkWithData(currentTrip, chunkACurrent);
    addChunkWithData(currentTrip, chunkCCurrent);

    cwSurveyChunkData chunkALoaded = chunkABase;
    chunkALoaded.shots[0].setDistance(QStringLiteral("11.1"));
    cwSurveyChunkData chunkBLoaded = chunkBBase;
    chunkBLoaded.stations[1].setName(QStringLiteral("B1-remote"));
    const cwSurveyChunkData chunkDLoaded = makeChunkData(
        chunkDId,
        QUuid::createUuid(),
        QUuid::createUuid(),
        QUuid::createUuid(),
        QStringLiteral("D0"),
        QStringLiteral("D1"),
        QStringLiteral("40.0"));
    cwSurveyChunkData chunkELoaded = chunkEBase;
    chunkELoaded.stations[0].setName(QStringLiteral("E0-remote"));

    cwTripData loadedTripData = currentTrip.data();
    loadedTripData.chunks = {chunkALoaded, chunkDLoaded, chunkBLoaded, chunkELoaded};

    cwTripData baseTripData = currentTrip.data();
    baseTripData.chunks = {chunkABase, chunkBBase, chunkEBase};

    cwTripMergePlan plan;
    plan.currentTrip = &currentTrip;
    plan.loadedTripData = loadedTripData;
    plan.baseTripData = baseTripData;

    const auto applyResult = cwTripMergeApplier::applyTripMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());

    const QList<cwSurveyChunk*> mergedChunks = currentTrip.chunks();
    REQUIRE(mergedChunks.size() == 4);
    CHECK(mergedChunks[0]->id() == chunkBId);
    CHECK(mergedChunks[1]->id() == chunkAId);
    CHECK(mergedChunks[2]->id() == chunkCId);
    CHECK(mergedChunks[3]->id() == chunkDId);

    CHECK(mergedChunks[1]->data().stations[0].name() == QStringLiteral("A-local"));
    CHECK(mergedChunks[1]->data().shots[0].distance().value() == QStringLiteral("11.1"));
    CHECK(mergedChunks[0]->data().stations[1].name() == QStringLiteral("B1-remote"));
}

TEST_CASE("cwTrip merge applier merges survey chunk payloads by stable id", "[cwTripMerge][sync]")
{
    cwTrip currentTrip;
    auto* chunk = new cwSurveyChunk();
    chunk->appendNewShot();
    currentTrip.addChunk(chunk);

    const QUuid stationAId = QUuid::createUuid();
    const QUuid stationBId = QUuid::createUuid();
    const QUuid shotAId = QUuid::createUuid();

    cwSurveyChunkData baseChunkData = chunk->data();
    baseChunkData.stations[0].setId(stationAId);
    baseChunkData.stations[1].setId(stationBId);
    baseChunkData.stations[0].setName(QStringLiteral("A0"));
    baseChunkData.stations[1].setName(QStringLiteral("B0"));
    baseChunkData.shots[0].setId(shotAId);
    baseChunkData.shots[0].setDistance(QStringLiteral("10.0"));
    chunk->setData(baseChunkData);

    cwTripData baseTripData = currentTrip.data();
    baseTripData.chunks[0] = baseChunkData;

    cwSurveyChunkData currentChunkData = chunk->data();
    currentChunkData.stations[0].setName(QStringLiteral("A-local"));
    chunk->setData(currentChunkData);

    cwTripData loadedTripData = currentTrip.data();
    loadedTripData.chunks[0] = baseChunkData;
    loadedTripData.chunks[0].stations[1].setName(QStringLiteral("B-remote"));
    loadedTripData.chunks[0].shots[0].setDistance(QStringLiteral("12.5"));

    cwTripMergePlan plan;
    plan.currentTrip = &currentTrip;
    plan.loadedTripData = loadedTripData;
    plan.baseTripData = baseTripData;

    const auto applyResult = cwTripMergeApplier::applyTripMergePlan(plan);
    REQUIRE_FALSE(applyResult.hasError());

    const cwSurveyChunkData mergedChunkData = currentTrip.chunk(0)->data();
    CHECK(mergedChunkData.stations[0].name() == QStringLiteral("A-local"));
    CHECK(mergedChunkData.stations[1].name() == QStringLiteral("B-remote"));
    CHECK(mergedChunkData.shots[0].distance().value() == QStringLiteral("12.5"));
}
