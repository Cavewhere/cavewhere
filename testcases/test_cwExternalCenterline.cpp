/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwExternalCenterline.h"
#include "cwSaveLoad.h"
#include "cwTrip.h"
#include "cavewhere.pb.h"

// Qt
#include <QSignalSpy>
#include <QString>

// Protobuf
#include <google/protobuf/util/json_util.h>

namespace {

constexpr const char* kEntryFile = "mycave.svx";
constexpr const char* kAlternateEntryFile = "branches/passage.svx";

} // namespace

TEST_CASE("cwExternalCenterline default constructs empty", "[ExternalCenterline]")
{
    const cwExternalCenterline value;
    CHECK(value.entryFile().isEmpty());
    CHECK(value.isEmpty());
}

TEST_CASE("cwExternalCenterline equality is based on entryFile", "[ExternalCenterline]")
{
    cwExternalCenterline a(QString::fromLatin1(kEntryFile));
    cwExternalCenterline b(QString::fromLatin1(kEntryFile));
    cwExternalCenterline c(QString::fromLatin1(kAlternateEntryFile));

    CHECK(a == b);
    CHECK_FALSE(a != b);
    CHECK(a != c);
    CHECK_FALSE(a == c);

    // setEntryFile mutates equality
    a.setEntryFile(QString::fromLatin1(kAlternateEntryFile));
    CHECK(a == c);
    CHECK(a != b);
}

TEST_CASE("cwExternalCenterline qHash matches equality contract", "[ExternalCenterline]")
{
    const cwExternalCenterline a(QString::fromLatin1(kEntryFile));
    const cwExternalCenterline b(QString::fromLatin1(kEntryFile));
    const cwExternalCenterline c(QString::fromLatin1(kAlternateEntryFile));

    CHECK(qHash(a) == qHash(b));
    // Different inputs can collide but the same input must always hash the same.
    CHECK(qHash(a) == qHash(a));
    // Empty value's hash is the hash of an empty QString.
    CHECK(qHash(cwExternalCenterline()) == qHash(QString()));
    Q_UNUSED(c);
}

TEST_CASE("cwExternalCenterline survives proto JSON round-trip on Cave", "[ExternalCenterline]")
{
    auto cave = std::make_unique<cwCave>();
    cave->setName(QStringLiteral("RoundTripCave"));
    cave->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));

    auto proto = cwSaveLoad::toProtoCave(cave.get());
    REQUIRE(proto != nullptr);
    REQUIRE(proto->has_external_centerline());
    CHECK(proto->external_centerline().entry_file() == kEntryFile);

    std::string json;
    google::protobuf::util::JsonPrintOptions printOptions;
    printOptions.add_whitespace = true;
    const auto printStatus =
        google::protobuf::util::MessageToJsonString(*proto, &json, printOptions);
    REQUIRE(printStatus.ok());
    REQUIRE_FALSE(json.empty());

    CavewhereProto::Cave parsed;
    const auto parseStatus =
        google::protobuf::util::JsonStringToMessage(json, &parsed);
    REQUIRE(parseStatus.ok());
    REQUIRE(parsed.has_external_centerline());
    CHECK(parsed.external_centerline().entry_file() == kEntryFile);
}

TEST_CASE("cwExternalCenterline survives proto JSON round-trip on Trip", "[ExternalCenterline]")
{
    auto trip = std::make_unique<cwTrip>();
    trip->setName(QStringLiteral("RoundTripTrip"));
    trip->setExternalCenterline(
        cwExternalCenterline(QString::fromLatin1(kAlternateEntryFile)));

    auto proto = cwSaveLoad::toProtoTrip(trip.get());
    REQUIRE(proto != nullptr);
    REQUIRE(proto->has_external_centerline());
    CHECK(proto->external_centerline().entry_file() == kAlternateEntryFile);

    std::string json;
    google::protobuf::util::JsonPrintOptions printOptions;
    printOptions.add_whitespace = true;
    const auto printStatus =
        google::protobuf::util::MessageToJsonString(*proto, &json, printOptions);
    REQUIRE(printStatus.ok());

    CavewhereProto::Trip parsed;
    const auto parseStatus =
        google::protobuf::util::JsonStringToMessage(json, &parsed);
    REQUIRE(parseStatus.ok());
    REQUIRE(parsed.has_external_centerline());
    CHECK(parsed.external_centerline().entry_file() == kAlternateEntryFile);

    const cwTripData reloaded = cwSaveLoad::tripDataFromProtoTrip(parsed);
    CHECK(reloaded.externalCenterline.entryFile()
          == QString::fromLatin1(kAlternateEntryFile));
}

TEST_CASE("cwExternalCenterline absent on Cave when empty", "[ExternalCenterline]")
{
    auto cave = std::make_unique<cwCave>();
    cave->setName(QStringLiteral("NativeCave"));
    REQUIRE(cave->externalCenterline().isEmpty());

    auto proto = cwSaveLoad::toProtoCave(cave.get());
    REQUIRE(proto != nullptr);
    CHECK_FALSE(proto->has_external_centerline());
}

TEST_CASE("cwExternalCenterline absent on Trip when empty", "[ExternalCenterline]")
{
    auto trip = std::make_unique<cwTrip>();
    trip->setName(QStringLiteral("NativeTrip"));
    REQUIRE(trip->externalCenterline().isEmpty());

    auto proto = cwSaveLoad::toProtoTrip(trip.get());
    REQUIRE(proto != nullptr);
    CHECK_FALSE(proto->has_external_centerline());
}

TEST_CASE("cwCave setExternalCenterline fires changed signal on set and clear",
          "[ExternalCenterline]")
{
    auto cave = std::make_unique<cwCave>();
    QSignalSpy spy(cave.get(), &cwCave::externalCenterlineChanged);
    REQUIRE(spy.isValid());

    cave->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));
    CHECK(spy.size() == 1);

    // Idempotent set is a no-op.
    cave->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));
    CHECK(spy.size() == 1);

    cave->setExternalCenterline(cwExternalCenterline());
    CHECK(spy.size() == 2);
    CHECK(cave->externalCenterline().isEmpty());
}

TEST_CASE("cwTrip setExternalCenterline fires changed signal on set and clear",
          "[ExternalCenterline]")
{
    auto trip = std::make_unique<cwTrip>();
    QSignalSpy spy(trip.get(), &cwTrip::externalCenterlineChanged);
    REQUIRE(spy.isValid());

    trip->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));
    CHECK(spy.size() == 1);

    trip->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));
    CHECK(spy.size() == 1);

    trip->setExternalCenterline(cwExternalCenterline());
    CHECK(spy.size() == 2);
    CHECK(trip->externalCenterline().isEmpty());
}

TEST_CASE("cwCave::data() round-trips externalCenterline through setData",
          "[ExternalCenterline]")
{
    auto cave = std::make_unique<cwCave>();
    cave->setName(QStringLiteral("DataRoundTrip"));
    cave->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));

    const cwCaveData snapshot = cave->data();
    CHECK(snapshot.externalCenterline.entryFile() == QString::fromLatin1(kEntryFile));

    auto destination = std::make_unique<cwCave>();
    destination->setData(snapshot);
    CHECK(destination->externalCenterline().entryFile()
          == QString::fromLatin1(kEntryFile));
}

TEST_CASE("cwTrip::data() round-trips externalCenterline through setData",
          "[ExternalCenterline]")
{
    auto trip = std::make_unique<cwTrip>();
    trip->setName(QStringLiteral("DataRoundTrip"));
    trip->setExternalCenterline(
        cwExternalCenterline(QString::fromLatin1(kAlternateEntryFile)));

    const cwTripData snapshot = trip->data();
    CHECK(snapshot.externalCenterline.entryFile()
          == QString::fromLatin1(kAlternateEntryFile));

    auto destination = std::make_unique<cwTrip>();
    destination->setData(snapshot);
    CHECK(destination->externalCenterline().entryFile()
          == QString::fromLatin1(kAlternateEntryFile));
}
