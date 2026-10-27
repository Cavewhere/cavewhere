/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwSurveyChunkSignaler.h"
#include "cwTrip.h"

// Qt
#include <QString>

namespace {

cwCave* addCave(cwCavingRegion& region, const QString& name)
{
    cwCave* cave = new cwCave();
    cave->setName(name);
    region.addCave(cave);
    return cave;
}

} // namespace

TEST_CASE("cwSurveyChunkSignaler accepts a null region and clears its connections",
          "[cwSurveyChunkSignaler]")
{
    cwCavingRegion region;
    cwCave* cave = addCave(region, QStringLiteral("First"));
    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Trip"));
    cave->addTrip(trip);

    cwSurveyChunkSignaler signaler;
    signaler.setRegion(&region);
    REQUIRE(signaler.region() == &region);

    signaler.setRegion(nullptr);
    CHECK(signaler.region() == nullptr);

    // The cleared signaler must be fully detached: a cave insert on the
    // old region used to reach connectAddedCaves, which dereferences the
    // (now null) region member.
    addCave(region, QStringLiteral("Second"));
    CHECK(signaler.region() == nullptr);
}

TEST_CASE("cwSurveyChunkSignaler switching regions disconnects the old one",
          "[cwSurveyChunkSignaler]")
{
    cwCavingRegion oldRegion;
    addCave(oldRegion, QStringLiteral("Old"));

    cwCavingRegion newRegion;

    cwSurveyChunkSignaler signaler;
    signaler.setRegion(&oldRegion);
    signaler.setRegion(&newRegion);
    REQUIRE(signaler.region() == &newRegion);

    // A cave insert on the old region must not reach the signaler: the
    // stale connection would run connectAddedCaves against the NEW
    // region with the old region's indices — out of bounds here, since
    // the new region is empty.
    addCave(oldRegion, QStringLiteral("OldSecond"));
    CHECK(newRegion.caveCount() == 0);
}
