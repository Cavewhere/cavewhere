/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Cavewhere includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwLinePlotManager.h"
#include "cwLinePlotTask.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

// Qt includes
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QUuid>

namespace {

// The 32-hex-no-braces form of QUuid::Id128 is the contract; if someone ever
// switches QUuid::toString() defaults under us, this test fails before
// downstream parsing silently breaks. Pulled into a helper so the format
// assertion lives in one place across the test cases below.
bool isThirtyTwoHex(const QString& s)
{
    static const QRegularExpression hex32(QStringLiteral("^[0-9a-fA-F]{32}$"));
    return hex32.match(s).hasMatch();
}

cwCave* addCaveWithSimpleShot(cwCavingRegion& region,
                              const QString& displayName,
                              const QString& fromName,
                              const QString& toName,
                              double distance)
{
    cwCave* cave = new cwCave();
    cave->setName(displayName);
    region.addCave(cave);

    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Trip"));
    cave->addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);

    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(distance)));
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    chunk->appendShot(cwStation(fromName), cwStation(toName), shot);

    return cave;
}

} // namespace

TEST_CASE("cavernCaveNameFor returns cave_<32 hex of the QUuid>",
          "[LinePlot][UuidPrefix]")
{
    const QUuid id = QUuid::createUuid();
    const QString encoded = cwLinePlotTask::cavernCaveNameFor(id);

    REQUIRE(encoded.startsWith(QStringLiteral("cave_")));
    const QString tail = encoded.mid(5);
    CHECK(tail.size() == 32);
    CHECK(isThirtyTwoHex(tail));

    // Encoding two different UUIDs must yield two different prefixes - if
    // the encoder ever collapsed UUIDs (e.g. via a hash) splitLookupByCave
    // would silently fold two caves together.
    const QUuid other = QUuid::createUuid();
    REQUIRE(id != other);
    CHECK(cwLinePlotTask::cavernCaveNameFor(other) != encoded);

    // The encoded prefix must be self-matching with the public regex: this
    // catches drift between the encoder and the parser (the two halves
    // share a literal "cave_" and a hex character class, and any future
    // edit that touches only one side is caught here).
    const QString joined = encoded + QStringLiteral(".a1");
    const QRegularExpressionMatch match = cwLinePlotTask::cavernStationRegex().match(joined);
    REQUIRE(match.hasMatch());
    CHECK(match.captured(2) == QStringLiteral("a1"));

    // QUuid::fromString does not accept the 32-hex-no-hyphens form even
    // when wrapped in braces - it requires the RFC-4122 dashed layout - so
    // we re-insert hyphens at the 8/4/4/4/12 boundaries before parsing.
    // The production parser inside splitLookupByCave does the same thing;
    // the test exercises the bare contract.
    const QString hex32 = match.captured(1);
    const QString dashed = hex32.mid(0, 8) + QStringLiteral("-")
                         + hex32.mid(8, 4) + QStringLiteral("-")
                         + hex32.mid(12, 4) + QStringLiteral("-")
                         + hex32.mid(16, 4) + QStringLiteral("-")
                         + hex32.mid(20, 12);
    const QUuid recovered = QUuid::fromString(dashed);
    CHECK(recovered == id);
}

TEST_CASE("cavernStationRegex matches new prefixed names and rejects the old integer form",
          "[LinePlot][UuidPrefix]")
{
    const QRegularExpression regex = cwLinePlotTask::cavernStationRegex();

    SECTION("matches cave_<32 hex>.<station>") {
        const QUuid id = QUuid::createUuid();
        const QString name = cwLinePlotTask::cavernCaveNameFor(id) + QStringLiteral(".a1");
        const QRegularExpressionMatch match = regex.match(name);
        REQUIRE(match.hasMatch());
        CHECK(match.captured(2) == QStringLiteral("a1"));
    }

    SECTION("matches stations with hyphens and underscores (validCharactersRegex contract)") {
        const QUuid id = QUuid::createUuid();
        const QString name = cwLinePlotTask::cavernCaveNameFor(id) + QStringLiteral(".big-passage_east");
        const QRegularExpressionMatch match = regex.match(name);
        REQUIRE(match.hasMatch());
        CHECK(match.captured(2) == QStringLiteral("big-passage_east"));
    }

    SECTION("rejects the legacy <integer>.<station> form") {
        // Before this commit the worker emitted cave names as the integer
        // index of the cave in the region snapshot ("0.a1", "1.a1", ...).
        // The new regex must NOT match that form - if it did, a stray
        // legacy line would silently fold into an unknown UUID and crash
        // QUuid::fromString downstream. Locked here.
        CHECK_FALSE(regex.match(QStringLiteral("0.a1")).hasMatch());
        CHECK_FALSE(regex.match(QStringLiteral("12.a1")).hasMatch());
    }

    SECTION("rejects a malformed prefix (wrong hex length)") {
        CHECK_FALSE(regex.match(QStringLiteral("cave_deadbeef.a1")).hasMatch());
        // 33 hex chars: one too many.
        CHECK_FALSE(regex.match(QStringLiteral("cave_0123456789abcdef0123456789abcdefa.a1")).hasMatch());
    }

    SECTION("rejects a missing dot separator") {
        const QString prefixOnly = cwLinePlotTask::cavernCaveNameFor(QUuid::createUuid());
        CHECK_FALSE(regex.match(prefixOnly).hasMatch());
        CHECK_FALSE(regex.match(prefixOnly + QStringLiteral("a1")).hasMatch());
    }
}

TEST_CASE("Two-cave pipeline keeps each cave's positions keyed by its own UUID",
          "[LinePlot][UuidPrefix]")
{
    // End-to-end gate: build a region with two caves whose station names
    // collide ("a1" appears in both). Before the UUID switch the encoder
    // wrote integer cave names ("0", "1") and cavern echoed them back via
    // the dotted prefix; that worked but could not survive the
    // *include "<external>.svx" path that commit 8 will introduce, because
    // the included files emit their own names. UUIDs decouple the two.
    //
    // Here we check the simpler invariant: with two caves sharing station
    // names, each cave gets its own position lookup and there is no
    // cross-contamination. The implementation detail being pinned is that
    // splitLookupByCave keys by QUuid (matching cwCave::id()); the public
    // surface this test binds against is cave->stationPositionLookup().
    cwCavingRegion region;
    cwCave* caveA = addCaveWithSimpleShot(region,
                                          QStringLiteral("Alpha"),
                                          QStringLiteral("a1"),
                                          QStringLiteral("a2"),
                                          10.0);
    cwCave* caveB = addCaveWithSimpleShot(region,
                                          QStringLiteral("Beta"),
                                          QStringLiteral("a1"),
                                          QStringLiteral("a2"),
                                          20.0);

    REQUIRE(caveA->id() != caveB->id());

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(&region);
    plotManager->waitToFinish();

    // Each cave's a1 anchors at the origin; the colliding a2 must NOT
    // share a position across caves. The 10m vs 20m shot distances make
    // any accidental cross-write trivially detectable.
    CHECK(caveA->stationPositionLookup().position(QStringLiteral("a1"))
          == QVector3D(0.0f, 0.0f, 0.0f));
    CHECK(caveA->stationPositionLookup().position(QStringLiteral("a2"))
          == QVector3D(0.0f, 10.0f, 0.0f));

    CHECK(caveB->stationPositionLookup().position(QStringLiteral("a1"))
          == QVector3D(0.0f, 0.0f, 0.0f));
    CHECK(caveB->stationPositionLookup().position(QStringLiteral("a2"))
          == QVector3D(0.0f, 20.0f, 0.0f));

    // Per-cave length echoes the shot input; if both caves were merged
    // into one lookup the totals would be wrong (or one cave would have
    // zero length because cavern overwrote its prefix).
    CHECK(caveA->length()->value() == 10.0);
    CHECK(caveB->length()->value() == 20.0);
}
