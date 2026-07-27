/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The scope labels the exporter, the line-plot worker and the geometry pass all
// read. Every one of them used to assign its own; these cases pin the answers
// they now share, and the two properties the sharing rests on — that the live
// caches and the snapshot pool assign identically, and that a trip label needs
// no region to be right.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCaveData.h"
#include "cwCavingRegion.h"
#include "cwCavingRegionData.h"
#include "cwScopeLabels.h"
#include "cwTrip.h"
#include "cwTripData.h"

// Qt
#include <QUuid>

namespace {

cwTripData tripData(const QString& name)
{
    cwTripData trip;
    trip.id = QUuid::createUuid();
    trip.name = name;
    return trip;
}

cwCaveData caveData(const QString& name, const QStringList& tripNames = {})
{
    cwCaveData cave;
    cave.id = QUuid::createUuid();
    cave.name = name;
    for (const QString& tripName : tripNames) {
        cave.trips.append(tripData(tripName));
    }
    return cave;
}

cwCavingRegionData regionData(const QList<cwCaveData>& caves)
{
    cwCavingRegionData region;
    region.caves = caves;
    return region;
}

} // namespace

TEST_CASE("cwScopeLabels names every cave and trip in a region", "[cwScopeLabels][scope]")
{
    const cwCavingRegionData region = regionData({
        caveData(QStringLiteral("Fisher Ridge"), {QStringLiteral("Topo 1"),
                                                  QStringLiteral("Topo-1")}),
        caveData(QStringLiteral("Fisher-Ridge"), {QStringLiteral("Topo 1")}),
    });
    const cwCaveData& first = region.caves.at(0);
    const cwCaveData& second = region.caves.at(1);

    const cwScopeLabels labels(region);

    SECTION("cave labels are unique across the region") {
        CHECK(labels.caveLabel(first.id) == QStringLiteral("fisher_ridge"));
        CHECK(labels.caveLabel(second.id) == QStringLiteral("fisher_ridge_2"));
    }

    SECTION("cavePrefix is the label the cave's stations hang under") {
        CHECK(labels.cavePrefix(first.id) == QStringLiteral("fisher_ridge."));
    }

    SECTION("trip labels are unique only within their own cave") {
        // The collision suffix on "Topo-1" is the point: it is second in *this*
        // cave. The other cave's "Topo 1" collides with nothing, so it keeps the
        // bare label even though the same string is already taken next door.
        CHECK(labels.tripLabels(first.id).value(first.trips.at(0).id)
              == QStringLiteral("topo_1"));
        CHECK(labels.tripLabels(first.id).value(first.trips.at(1).id)
              == QStringLiteral("topo_1_2"));
        CHECK(labels.tripLabels(second.id).value(second.trips.at(0).id)
              == QStringLiteral("topo_1"));
    }

    SECTION("caveId inverts caveLabel, which is how a solved name finds its cave") {
        CHECK(labels.caveId(QStringLiteral("fisher_ridge")) == first.id);
        CHECK(labels.caveId(QStringLiteral("fisher_ridge_2")) == second.id);
    }

    SECTION("a scope this pool never saw is answered as absent, not guessed at") {
        const QUuid stranger = QUuid::createUuid();
        CHECK(labels.caveLabel(stranger).isEmpty());
        CHECK(labels.cavePrefix(stranger).isEmpty());
        CHECK(labels.tripLabels(stranger).isEmpty());
        CHECK(labels.caveId(QStringLiteral("bat_cave")).isNull());
    }

    SECTION("a default-constructed pool names nothing") {
        const cwScopeLabels empty;
        CHECK(empty.caveLabel(first.id).isEmpty());
        CHECK(empty.tripLabels(first.id).isEmpty());
    }
}

TEST_CASE("The live caches and the snapshot pool assign the same labels",
          "[cwScopeLabels][scope]")
{
    // The invariant the whole design rests on, and the only case that crosses
    // the two implementations of it. cwSiblingLabelCache assigns from the live
    // QObject children on the main thread; cwScopeLabels assigns from a
    // cwCavingRegionData on the worker. Nothing is carried between them — the
    // export/decode round trip is correct only because both evaluate the same
    // function over the same order. Neither side can drift alone without this
    // failing.
    cwCavingRegion region;

    cwCave* first = new cwCave();
    first->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(first);

    cwCave* second = new cwCave();
    second->setName(QStringLiteral("Fisher-Ridge"));
    region.addCave(second);

    const auto addTrip = [](cwCave* cave, const QString& name) {
        cwTrip* trip = new cwTrip();
        trip->setName(name);
        cave->addTrip(trip);
    };
    addTrip(first, QStringLiteral("Topo 1"));
    addTrip(first, QStringLiteral("Topo-1"));
    addTrip(second, QStringLiteral("Topo 1"));

    const cwScopeLabels snapshot(region.data());

    REQUIRE(region.caveCount() == 2);
    for (cwCave* cave : region.caves()) {
        INFO("cave: " << cave->name().toStdString());
        CHECK(region.caveScopeLabels().value(cave->id()) == snapshot.caveLabel(cave->id()));
        CHECK(cave->tripScopeLabels() == snapshot.tripLabels(cave->id()));
        CHECK_FALSE(cave->tripScopeLabels().isEmpty());
    }
}

TEST_CASE("cwScopeLabels::forCave names a cave standing alone", "[cwScopeLabels][scope]")
{
    // The single-cave exporter has no region, so nothing can say whether this
    // cave's label is unique — it gets none, and the exporter falls back to the
    // cave's own sanitized name. Its trips still get labels: a trip label is
    // unique within its cave, which is true with or without a region.
    const cwCaveData cave = caveData(QStringLiteral("Fisher Ridge"),
                                     {QStringLiteral("Topo 1"), QStringLiteral("Topo-1")});

    const cwScopeLabels labels = cwScopeLabels::forCave(cave);

    CHECK(labels.caveLabel(cave.id).isEmpty());
    CHECK(labels.tripLabels(cave.id).value(cave.trips.at(0).id) == QStringLiteral("topo_1"));
    CHECK(labels.tripLabels(cave.id).value(cave.trips.at(1).id) == QStringLiteral("topo_1_2"));

    SECTION("and gives its trips the same labels a region would have") {
        const cwScopeLabels regionLabels(regionData({cave}));
        CHECK(labels.tripLabels(cave.id) == regionLabels.tripLabels(cave.id));
    }
}
