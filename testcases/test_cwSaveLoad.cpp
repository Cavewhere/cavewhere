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
using namespace google::protobuf;

QDir testDir() {
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QDir dir(desktopPath);
    dir.mkdir("trip");
    dir.cd("trip");
    return dir;
}


// deep‑compare a & b, but skip ANY field whose descriptor
// appears in excludedFields
bool compareMessagesIgnoring(
    const Message& a,
    const Message& b,
    const std::set<const FieldDescriptor*>& excludedFields)
{
    // same object?
    if (&a == &b) {
        return true;
    }

    // byte‑for‑byte equal?
    if (util::MessageDifferencer::Equals(a, b)) {
        return true;
    }

    // must be same type
    const Descriptor* desc = a.GetDescriptor();
    if (desc != b.GetDescriptor()) {
        return false;
    }

    const Reflection* reflA = a.GetReflection();
    const Reflection* reflB = b.GetReflection();

    // iterate every field in the descriptor
    for (int i = 0; i < desc->field_count(); ++i) {
        const FieldDescriptor* field = desc->field(i);
        // qDebug() << "Field:" << i << field->DebugString();

        // skip this field entirely if asked
        if (excludedFields.count(field)) {
            // qDebug() << "\t excluded!";
            continue;
        }

        // handle repeated fields
        if (field->is_repeated()) {
            int sizeA = reflA->FieldSize(a, field);
            int sizeB = reflB->FieldSize(b, field);
            if (sizeA != sizeB) {
                // qDebug() << "\tRepeated is different sizes!";
                return false;
            }

            for (int idx = 0; idx < sizeA; ++idx) {
                if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
                    const Message& subA = reflA->GetRepeatedMessage(a, field, idx);
                    const Message& subB = reflB->GetRepeatedMessage(b, field, idx);
                    // qDebug() << "\tSub message!";
                    if (!compareMessagesIgnoring(subA, subB, excludedFields)) {
                        return false;
                    }
                }
                else {
                    // primitive or enum or string
                    switch (field->cpp_type()) {
                    case FieldDescriptor::CPPTYPE_INT32:
                        if (reflA->GetRepeatedInt32(a, field, idx)
                            != reflB->GetRepeatedInt32(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_INT64:
                        if (reflA->GetRepeatedInt64(a, field, idx)
                            != reflB->GetRepeatedInt64(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_UINT32:
                        if (reflA->GetRepeatedUInt32(a, field, idx)
                            != reflB->GetRepeatedUInt32(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_UINT64:
                        if (reflA->GetRepeatedUInt64(a, field, idx)
                            != reflB->GetRepeatedUInt64(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_DOUBLE:
                        if (reflA->GetRepeatedDouble(a, field, idx)
                            != reflB->GetRepeatedDouble(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_FLOAT:
                        if (reflA->GetRepeatedFloat(a, field, idx)
                            != reflB->GetRepeatedFloat(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_BOOL:
                        if (reflA->GetRepeatedBool(a, field, idx)
                            != reflB->GetRepeatedBool(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_ENUM:
                        if (reflA->GetRepeatedEnumValue(a, field, idx)
                            != reflB->GetRepeatedEnumValue(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_STRING:
                        if (reflA->GetRepeatedString(a, field, idx)
                            != reflB->GetRepeatedString(b, field, idx)) {
                            return false;
                        }
                        break;
                    default:
                        return false;  // unknown type
                    }
                }
            }
        }
        // singular field
        else {
            bool hasA = reflA->HasField(a, field);
            bool hasB = reflB->HasField(b, field);
            if (hasA != hasB) return false;
            if (!hasA) continue;  // both unset → OK

            if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
                const Message& subA = reflA->GetMessage(a, field);
                const Message& subB = reflB->GetMessage(b, field);
                if (!compareMessagesIgnoring(subA, subB, excludedFields)) {
                    return false;
                }
            }
            else {
                switch (field->cpp_type()) {
                case FieldDescriptor::CPPTYPE_INT32:
                    if (reflA->GetInt32(a, field)
                        != reflB->GetInt32(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_INT64:
                    if (reflA->GetInt64(a, field)
                        != reflB->GetInt64(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_UINT32:
                    if (reflA->GetUInt32(a, field)
                        != reflB->GetUInt32(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_UINT64:
                    if (reflA->GetUInt64(a, field)
                        != reflB->GetUInt64(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_DOUBLE:
                    if (reflA->GetDouble(a, field)
                        != reflB->GetDouble(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_FLOAT:
                    if (reflA->GetFloat(a, field)
                        != reflB->GetFloat(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_BOOL:
                    if (reflA->GetBool(a, field)
                        != reflB->GetBool(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_ENUM:
                    if (reflA->GetEnumValue(a, field)
                        != reflB->GetEnumValue(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_STRING:
                    if (reflA->GetString(a, field)
                        != reflB->GetString(b, field)) {
                        return false;
                    }
                    break;
                default:
                    return false;  // unknown type
                }
            }
        }
    }

    // all fields matched
    return true;
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
                REQUIRE(mergedTripPtr->chunks_size() == 5);

                auto newChunk = mergedTripPtr->chunks(4);
                REQUIRE(newChunk.leg_size() == 3);

                int chunksFieldNumber = 5;
                auto chunkField = base->descriptor()->FindFieldByNumber(5);
                CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

                REQUIRE(base->chunks_size() == 4);
                for(int i = 0; i < base->chunks_size(); ++i) {
                    auto baseChunk = base->chunks(i);
                    auto mergedChunk = mergedTripPtr->chunks(i);
                    CHECK(google::protobuf::util::MessageDifferencer::Equals(baseChunk, mergedChunk));
                }

                CHECK(newChunk.leg(0).has_name());
                CHECK(newChunk.leg(0).name() == "a1");

                CHECK(newChunk.leg(1).has_distance());
                CHECK(newChunk.leg(1).distance() == "20.1");
                CHECK(newChunk.leg(1).has_clino());
                CHECK(newChunk.leg(1).clino() == "10.2");
                CHECK(newChunk.leg(1).has_compass());
                CHECK(newChunk.leg(1).compass() == "7.3");

                CHECK(newChunk.leg(2).has_name());
                CHECK(newChunk.leg(2).name() == "test1");

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

                int chunksFieldNumber = 5;
                auto chunkField = base->descriptor()->FindFieldByNumber(5);
                CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

                REQUIRE(mergedTripPtr->chunks_size() == 6);
                REQUIRE(base->chunks_size() == 4);
                for(int i = 0; i < base->chunks_size(); ++i) {
                    auto baseChunk = base->chunks(i);
                    auto mergedChunk = mergedTripPtr->chunks(i);
                    CHECK(google::protobuf::util::MessageDifferencer::Equals(baseChunk, mergedChunk));
                }

                auto newChunkOurs = mergedTripPtr->chunks(4);
                REQUIRE(newChunkOurs.leg_size() == 3);

                CHECK(newChunkOurs.leg(0).has_name());
                CHECK(newChunkOurs.leg(0).name() == "a1");

                CHECK(newChunkOurs.leg(1).has_distance());
                CHECK(newChunkOurs.leg(1).distance() == "20.1");
                CHECK(newChunkOurs.leg(1).has_clino());
                CHECK(newChunkOurs.leg(1).clino() == "10.2");
                CHECK(newChunkOurs.leg(1).has_compass());
                CHECK(newChunkOurs.leg(1).compass() == "7.3");

                CHECK(newChunkOurs.leg(2).has_name());
                CHECK(newChunkOurs.leg(2).name() == "test1");

                auto newChunkTheirs = mergedTripPtr->chunks(5);
                REQUIRE(newChunkTheirs.leg_size() == 3);

                CHECK(newChunkTheirs.leg(0).has_name());
                CHECK(newChunkTheirs.leg(0).name() == "a2");

                CHECK(newChunkTheirs.leg(1).has_distance());
                CHECK(newChunkTheirs.leg(1).distance() == "21.1");
                CHECK(newChunkTheirs.leg(1).has_clino());
                CHECK(newChunkTheirs.leg(1).clino() == "11.2");
                CHECK(newChunkTheirs.leg(1).has_compass());
                CHECK(newChunkTheirs.leg(1).compass() == "71.3");

                CHECK(newChunkTheirs.leg(2).has_name());
                CHECK(newChunkTheirs.leg(2).name() == "c10");


                // dumpProto(std::move(base),
                //           std::move(ourChange),
                //           std::move(theirsChange),
                //           std::move(mergedTripPtr));
            }
        }

        SECTION("Change station name and add shot at end") {

            auto ourLastChunk = trip->chunk(trip->chunkCount() - 1);
            auto ourLastStation = ourLastChunk->stationCount() - 2;
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

            int chunksFieldNumber = 5;
            auto chunkField = base->descriptor()->FindFieldByNumber(5);
            CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

            REQUIRE(mergedTripPtr->chunks_size() == 4);
            REQUIRE(base->chunks_size() == 4);

            //Skip the last chunk, because that's the one we changed
            for(int i = 0; i < 3; ++i) {
                auto baseChunk = base->chunks(i);
                auto mergedChunk = mergedTripPtr->chunks(i);
                CHECK(google::protobuf::util::MessageDifferencer::Equals(baseChunk, mergedChunk));
            }

            //The leg should be the same exepect for the last 3 entries, station and shot
            const auto& mergedLastChunk = mergedTripPtr->chunks(3);
            const auto& baseLastChunk = base->chunks(3);

            CHECK(!google::protobuf::util::MessageDifferencer::Equals(baseLastChunk, mergedLastChunk));

            //Everything should be the same expect for the legField
            auto legField = baseLastChunk.descriptor()->FindFieldByNumber(4);
            CHECK(compareMessagesIgnoring(baseLastChunk, mergedLastChunk, {legField}));

            REQUIRE(mergedLastChunk.leg_size() == 7);
            REQUIRE(baseLastChunk.leg_size() == 7);

            //All the leg data should be the same expect for the last three rows
            for(int i = 0; i < 4; i++) {
                auto baseStationShot = baseLastChunk.leg(i);
                auto mergedStationShot = mergedLastChunk.leg(i);
                CHECK(util::MessageDifferencer::Equals(baseStationShot, mergedStationShot));
            }

            auto stationRenamed = mergedLastChunk.leg(4);
            CHECK(stationRenamed.name() == "d1");

            auto newShot = mergedLastChunk.leg(5);
            CHECK(newShot.distance() == "30.2");
            CHECK(newShot.compass() == "123.1");
            CHECK(newShot.clino() == "23.4");

            auto newStation = mergedLastChunk.leg(6);
            CHECK(newStation.name() == "c4");

            // dumpProto(std::move(base),
            //           std::move(ourChange),
            //           std::move(theirsChange),
            //           std::move(mergedTripPtr));
        }

        SECTION("Modify the same the same data") {
            REQUIRE(trip->chunkCount() > 0);
            auto firstChunk = trip->chunk(0);
            firstChunk->setData(cwSurveyChunk::StationNameRole, 0, "b1"); //from a1 -> b1

            auto ourChange = cwSaveLoad::toProtoTrip(trip);
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
            CHECK(google::protobuf::util::MessageDifferencer::Equals(*ourChange, *mergedTripPtr));
            CHECK(google::protobuf::util::MessageDifferencer::Equals(*theirsChange, *mergedTripPtr));

            int chunksFieldNumber = 5;
            auto chunkField = base->descriptor()->FindFieldByNumber(5);
            CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

            REQUIRE(base->chunks_size() == 4);
            REQUIRE(mergedTripPtr->chunks_size() == 4);

            //Check that everything that the chunk are the same
            const auto& baseFirstChunk = base->chunks(0);
            const auto& mergedFirstChunk = mergedTripPtr->chunks(0);
            CHECK(!util::MessageDifferencer::Equals(baseFirstChunk, mergedFirstChunk));

            REQUIRE(baseFirstChunk.leg_size() == 9);
            REQUIRE(mergedFirstChunk.leg_size() == 9);

            const auto& baseFirstStation = baseFirstChunk.leg(0);
            const auto& mergedFirstStation = mergedFirstChunk.leg(0);

            CHECK(baseFirstStation.name() == "a1");
            CHECK(mergedFirstStation.name() == "b1");

            auto stationNameField = mergedFirstStation.descriptor()->FindFieldByNumber(100);
            CHECK(compareMessagesIgnoring(baseFirstStation, mergedFirstStation, {stationNameField}));

            //Make sure all the other leg data in the first chunk is the same
            for(int i = 1; i < baseFirstChunk.leg_size(); i++) {
                const auto& baseLeg = baseFirstChunk.leg(i);
                const auto& mergedLeg = mergedFirstChunk.leg(i);
                CHECK(util::MessageDifferencer::Equals(baseLeg, mergedLeg));
            }

            //Make sure all the other chunks are the same
            for(int i = 1; i < base->chunks_size(); ++i) {
                auto baseChunk = base->chunks(i);
                auto mergedChunk = mergedTripPtr->chunks(i);
                CHECK(util::MessageDifferencer::Equals(baseChunk, mergedChunk));
            }

            // dumpProto(std::move(base),
            //           std::move(ourChange),
            //           std::move(theirsChange),
            //           std::move(mergedTripPtr));

        }

        SECTION("Modify the same name, but changed lrud") {
            REQUIRE(trip->chunkCount() > 0);
            auto firstChunk = trip->chunk(0);
            firstChunk->setData(cwSurveyChunk::StationNameRole, 0, "b1"); //from a1 -> b1
            auto ourChange = cwSaveLoad::toProtoTrip(trip);

            firstChunk->setData(cwSurveyChunk::StationLeftRole, 0, "1.0"); //from a1 -> b1
            firstChunk->setData(cwSurveyChunk::StationUpRole, 0, "2.1"); //from a1 -> b1

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
            CHECK(!google::protobuf::util::MessageDifferencer::Equals(*ourChange, *mergedTripPtr)); //Not the same because LRUD were changed
            CHECK(google::protobuf::util::MessageDifferencer::Equals(*theirsChange, *mergedTripPtr));

            int chunksFieldNumber = 5;
            auto chunkField = base->descriptor()->FindFieldByNumber(5);
            CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

            REQUIRE(base->chunks_size() == 4);
            REQUIRE(mergedTripPtr->chunks_size() == 4);

            //Check that everything that the chunk are the same
            const auto& baseFirstChunk = base->chunks(0);
            const auto& mergedFirstChunk = mergedTripPtr->chunks(0);
            CHECK(!util::MessageDifferencer::Equals(baseFirstChunk, mergedFirstChunk));

            REQUIRE(baseFirstChunk.leg_size() == 9);
            REQUIRE(mergedFirstChunk.leg_size() == 9);

            const auto& baseFirstStation = baseFirstChunk.leg(0);
            const auto& mergedFirstStation = mergedFirstChunk.leg(0);

            CHECK(baseFirstStation.name() == "a1");
            CHECK(mergedFirstStation.name() == "b1");
            CHECK(mergedFirstStation.left() == "1.0");
            CHECK(mergedFirstStation.up() == "2.1");

            auto stationNameField = mergedFirstStation.descriptor()->FindFieldByNumber(100);
            auto stationLeftField = mergedFirstStation.descriptor()->FindFieldByNumber(101);
            auto stationUpField = mergedFirstStation.descriptor()->FindFieldByNumber(103);
            CHECK(compareMessagesIgnoring(baseFirstStation, mergedFirstStation, {stationNameField,
                                                                                 stationLeftField,
                                                                                 stationUpField}));

            //Make sure all the other leg data in the first chunk is the same
            for(int i = 1; i < baseFirstChunk.leg_size(); i++) {
                const auto& baseLeg = baseFirstChunk.leg(i);
                const auto& mergedLeg = mergedFirstChunk.leg(i);
                CHECK(util::MessageDifferencer::Equals(baseLeg, mergedLeg));
            }

            //Make sure all the other chunks are the same
            for(int i = 1; i < base->chunks_size(); ++i) {
                auto baseChunk = base->chunks(i);
                auto mergedChunk = mergedTripPtr->chunks(i);
                CHECK(util::MessageDifferencer::Equals(baseChunk, mergedChunk));
            }
        }

        SECTION("Conflicts needs to be valid") {

            SECTION("Remove first shot") {
                //Change the first station in ours
                REQUIRE(trip->chunkCount() > 0);
                auto firstChunk = trip->chunk(0);
                firstChunk->setData(cwSurveyChunk::StationNameRole, 0, "b1"); //from a1 -> b1
                auto ourChange = cwSaveLoad::toProtoTrip(trip);

                //Remove the first station and shot in thiers
                //This removes the first station and shot, since merging is tracked on id
                //there's no conflict even through the first station in the chunk has changed.
                //This because the id, hasn't changed and will never change because it's a uuid
                //This gives valid results (for chunk) at this case,
                //but will always take the delete even if the merge
                //stratage is opposite (like this testcase).
                firstChunk->setData(cwSurveyChunk::StationNameRole, 0, "a1"); //from a1 -> b1
                firstChunk->removeStation(0, cwSurveyChunk::Below);

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
                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*ourChange, *mergedTripPtr)); //Not the same because LRUD were changed
                CHECK(google::protobuf::util::MessageDifferencer::Equals(*theirsChange, *mergedTripPtr));

                int chunksFieldNumber = 5;
                auto chunkField = base->descriptor()->FindFieldByNumber(5);
                CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

                REQUIRE(base->chunks_size() == 4);
                REQUIRE(mergedTripPtr->chunks_size() == 4);

                //Check that everything that the chunk are the same
                const auto& baseFirstChunk = base->chunks(0);
                const auto& mergedFirstChunk = mergedTripPtr->chunks(0);
                CHECK(!util::MessageDifferencer::Equals(baseFirstChunk, mergedFirstChunk));

                REQUIRE(baseFirstChunk.leg_size() == 9);
                REQUIRE(mergedFirstChunk.leg_size() == 7); //We delete the first and last station

                const auto& baseFirstStation = baseFirstChunk.leg(0);
                const auto& mergedFirstStation = mergedFirstChunk.leg(0);

                //Make sure all the other leg data in the first chunk is the same
                for(int i = 2; i < baseFirstChunk.leg_size(); i++) {
                    const auto& baseLeg = baseFirstChunk.leg(i);
                    const auto& mergedLeg = mergedFirstChunk.leg(i - 2);
                    CHECK(util::MessageDifferencer::Equals(baseLeg, mergedLeg));
                }

                //Make sure all the other chunks are the same
                for(int i = 1; i < base->chunks_size(); ++i) {
                    auto baseChunk = base->chunks(i);
                    auto mergedChunk = mergedTripPtr->chunks(i);
                    CHECK(util::MessageDifferencer::Equals(baseChunk, mergedChunk));
                }

                // dumpProto(std::move(base),
                //           std::move(ourChange),
                //           std::move(theirsChange),
                //           std::move(mergedTripPtr));
            }
        }
    }
}

