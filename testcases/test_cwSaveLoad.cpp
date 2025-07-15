//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwShot.h"
#include "cwTrip.h"
#include "cwSaveLoad.h"
#include "cwRootData.h"
#include "cwDiff.h"
#include "cwJobSettings.h"
#include "cwSurveyChunk.h"

//Qt includes
#include <QStandardPaths>

//Test includes
#include "TestHelper.h"

//Protobuf includes
#include <google/protobuf/message.h>
#include <google/protobuf/util/message_differencer.h>
#include "cavewhere.pb.h"

QDir testDir() {
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QDir dir(desktopPath);
    dir.mkdir("trip");
    dir.cd("trip");
    return dir;
}

TEST_CASE("cwSaveLoad should save and load cwTrip - empty", "[cwSaveLoad]") {
    auto dir = testDir();

    cwTrip emptyTrip;

    cwSaveLoad save;
    save.saveTrip(dir, &emptyTrip);

    save.waitForFinished();
}

TEST_CASE("cwSaveLoad should save and load cwTrip - complex", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

    root->project()->loadFile(filename);
    root->project()->waitLoadToFinish();

    REQUIRE(root->project()->cavingRegion()->caveCount() >= 1);
    auto cave = root->project()->cavingRegion()->cave(0);

    REQUIRE(cave->tripCount() >= 1);
    auto trip = cave->trip(0);

    cwSaveLoad save;
    auto dir = testDir();

    save.saveTrip(dir, trip);
    save.waitForFinished();

    //Test 3-way merging with protobuf

}

template<typename T>
void dumpProto(T base, T ours, T theirs, T merged) {
    QDir dir = testDir();
    // dir.removeRecursively();
    dir = testDir();

    dir.mkdir("ours");
    QDir oursDir = dir;
    oursDir.cd("ours");

    dir.mkdir("theirs");
    QDir theirsDir = dir;
    theirsDir.cd("theirs");

    dir.mkdir("merged");
    QDir mergedDir = dir;
    mergedDir.cd("merged");

    cwSaveLoad save;

    save.saveProtoTrip(dir, std::move(base));
    save.saveProtoTrip(oursDir, std::move(ours));
    save.saveProtoTrip(theirsDir, std::move(theirs));
    save.saveProtoTrip(mergedDir, std::move(merged));

    save.waitForFinished();
}

TEST_CASE("cwSaveLoad should 3-way merge cwTrip correctly", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

    //Prevents loop closure from happening
    root->settings()->jobSettings()->setAutomaticUpdate(false);

    root->project()->loadFile(filename);
    root->project()->waitLoadToFinish();

    REQUIRE(root->project()->cavingRegion()->caveCount() >= 1);
    auto cave = root->project()->cavingRegion()->cave(0);

    REQUIRE(cave->tripCount() >= 1);
    auto trip = cave->trip(0);

    auto base = cwSaveLoad::toProtoTrip(trip);

    SECTION("Change the date") {
        SECTION("Ours") {
            trip->setDate(QDateTime(QDate(2025,07,14), QTime()));

            auto change = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = (*change.get());
            const google::protobuf::Message& thiersRef = baseRef;

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseOursOnConflict);
            auto mergedTripPtr = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get());

            REQUIRE(mergedTripPtr != nullptr);

            CHECK(!google::protobuf::util::MessageDifferencer::Equals(base->date(), mergedTripPtr->date()));
            CHECK(google::protobuf::util::MessageDifferencer::Equals(change->date(), mergedTripPtr->date()));
        }

        SECTION("Thiers") {
            trip->setDate(QDateTime(QDate(2025,07,14), QTime()));

            auto change = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = baseRef;
            const google::protobuf::Message& thiersRef = (*change.get());

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseTheirsOnConflict);
            auto mergedTripPtr = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get());

            REQUIRE(mergedTripPtr != nullptr);

            CHECK(!google::protobuf::util::MessageDifferencer::Equals(base->date(), mergedTripPtr->date()));
            CHECK(google::protobuf::util::MessageDifferencer::Equals(change->date(), mergedTripPtr->date()));
        }

        SECTION("Conflict, take ours") {
            trip->setDate(QDateTime(QDate(2025,07,14), QTime()));
            auto ours = cwSaveLoad::toProtoTrip(trip);

            trip->setDate(QDateTime(QDate(2025,07,01), QTime()));
            auto theirs = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = (*ours.get());
            const google::protobuf::Message& thiersRef = (*theirs.get());

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseOursOnConflict);
            auto mergedTripPtr = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get());

            REQUIRE(mergedTripPtr != nullptr);

            CHECK(google::protobuf::util::MessageDifferencer::Equals(ours->date(), mergedTripPtr->date()));
        }

        SECTION("Conflict, take theirs") {
            trip->setDate(QDateTime(QDate(2025,07,14), QTime()));
            auto ours = cwSaveLoad::toProtoTrip(trip);

            trip->setDate(QDateTime(QDate(2025,07,01), QTime()));
            auto theirs = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = (*ours.get());
            const google::protobuf::Message& thiersRef = (*theirs.get());

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseTheirsOnConflict);
            auto mergedTripPtr = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get());

            REQUIRE(mergedTripPtr != nullptr);

            CHECK(google::protobuf::util::MessageDifferencer::Equals(theirs->date(), mergedTripPtr->date()));
        }
    }

    SECTION("Change the update the chunk data") {

        SECTION("Add new chunk") {
            cwStation fromStation;
            fromStation.setName("a1");

            cwStation toStation;
            toStation.setName("test1");

            cwShot shot;
            shot.setDistance(cwDistanceReading("20.1"));
            shot.setClino(cwClinoReading("10.2"));
            shot.setCompass(cwCompassReading("7.3"));

            trip->addShotToLastChunk(fromStation, toStation, shot);

            SECTION("Ours") {
                auto change = cwSaveLoad::toProtoTrip(trip);

                const google::protobuf::Message& baseRef = (*base.get());
                const google::protobuf::Message& oursRef = (*change.get());
                const google::protobuf::Message& thiersRef = (*base.get());

                auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                                   oursRef,
                                                                   thiersRef,
                                                                   cwDiff::MergeStrategy::UseOursOnConflict);
                auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                    if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                        mergedTrip.release();
                        return std::unique_ptr<CavewhereProto::Trip>(raw);
                    }
                    return nullptr;
                }();
                REQUIRE(mergedTripPtr != nullptr);

                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*base, *mergedTripPtr));
                CHECK(google::protobuf::util::MessageDifferencer::Equals(*change, *mergedTripPtr));

                //TODO make sure merge changes are real

                // dumpProto(std::move(base),
                //           std::move(change),
                //           std::unique_ptr<CavewhereProto::Trip>(nullptr),
                //           std::move(mergedTripPtr));

            }

            SECTION("Concat ours / thiers") {
                auto ourChange = cwSaveLoad::toProtoTrip(trip);

                auto lastChunkIndex = trip->chunkCount() - 1;
                trip->removeChunks(lastChunkIndex, lastChunkIndex);

                cwStation fromStation;
                fromStation.setName("a2");

                cwStation toStation;
                toStation.setName("c10");

                cwShot shot;
                shot.setDistance(cwDistanceReading("21.1"));
                shot.setClino(cwClinoReading("11.2"));
                shot.setCompass(cwCompassReading("71.3"));

                trip->addShotToLastChunk(fromStation, toStation, shot);

                auto theirsChange = cwSaveLoad::toProtoTrip(trip);

                const google::protobuf::Message& baseRef = (*base.get());
                const google::protobuf::Message& oursRef = (*ourChange.get());
                const google::protobuf::Message& thiersRef = (*theirsChange.get());

                auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                                   oursRef,
                                                                   thiersRef,
                                                                   cwDiff::MergeStrategy::UseOursOnConflict);
                auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                    if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                        mergedTrip.release();
                        return std::unique_ptr<CavewhereProto::Trip>(raw);
                    }
                    return nullptr;
                }();
                REQUIRE(mergedTripPtr != nullptr);

                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*base, *mergedTripPtr));
                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*ourChange, *mergedTripPtr));
                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*theirsChange, *mergedTripPtr));

                //TODO make sure merge changes are real

                // dumpProto(std::move(base),
                //           std::move(ourChange),
                //           std::move(theirsChange),
                //           std::move(mergedTripPtr));
            }
        }

        SECTION("Change station name and add shot at end") {

            auto ourLastChunk = trip->chunk(trip->chunkCount() - 1);
            auto ourLastStation = ourLastChunk->stationCount() - 1;
            auto baseLastStationName = ourLastChunk->station(ourLastStation).name();
            ourLastChunk->setData(cwSurveyChunk::StationNameRole, ourLastStation, "d1");
            auto ourChange = cwSaveLoad::toProtoTrip(trip);

            //Change it back
            ourLastChunk->setData(cwSurveyChunk::StationNameRole, ourLastStation, baseLastStationName);

            //Thiers
            auto chunk = trip->chunk(trip->chunkCount() - 1);

            auto lastShot = chunk->shotCount() - 1;
            chunk->setData(cwSurveyChunk::ShotDistanceRole, lastShot, "30.2");
            chunk->setData(cwSurveyChunk::ShotCompassRole, lastShot, "123.1");
            chunk->setData(cwSurveyChunk::ShotClinoRole, lastShot, "23.4");

            auto lastStation = chunk->stationCount() - 1;
            chunk->setData(cwSurveyChunk::StationNameRole, lastStation, "c4");

            auto theirsChange = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = (*ourChange.get());
            const google::protobuf::Message& thiersRef = (*theirsChange.get());

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseOursOnConflict);
            auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                    mergedTrip.release();
                    return std::unique_ptr<CavewhereProto::Trip>(raw);
                }
                return nullptr;
            }();
            REQUIRE(mergedTripPtr != nullptr);

            CHECK(!google::protobuf::util::MessageDifferencer::Equals(*base, *mergedTripPtr));
            CHECK(!google::protobuf::util::MessageDifferencer::Equals(*ourChange, *mergedTripPtr));
            CHECK(!google::protobuf::util::MessageDifferencer::Equals(*theirsChange, *mergedTripPtr));

            //TODO make sure merge changes are real

            // dumpProto(std::move(base),
            //           std::move(ourChange),
            //           std::move(theirsChange),
            //           std::move(mergedTripPtr));
        }
    }
}

