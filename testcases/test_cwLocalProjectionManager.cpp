/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwLocalProjectionManager.h"

//Qt includes
#include <QUuid>

using Catch::Matchers::WithinAbs;

namespace {

const QString kUtm12N = QStringLiteral("EPSG:32612");

constexpr double kAnchorEasting = 500000.0;
constexpr double kAnchorNorthing = 4194000.0;
constexpr double kElevation = 2700.0;

//! Far enough from the anchor to be past the manager's 50 km threshold — the
//! wrong entrance, not a refinement of the right one.
constexpr double kDistantNorthing = 4394000.0;

//! Inside the threshold: a second entrance of the same cave.
constexpr double kNearbyEasting = 500100.0;
constexpr double kNearbyNorthing = 4194100.0;

// A frame from somewhere else entirely, used as a stand-in for "what the file
// says" — no derivation from this project's data could produce it.
const QString kElsewhereCS = QStringLiteral(
    "+proj=tmerc +lat_0=-33.9 +lon_0=18.4 +k=1 +x_0=0 +y_0=0 "
    "+datum=WGS84 +units=m +no_defs +type=crs");

cwFixStation makeFix(const QString& name, const QString& cs,
                     double easting, double northing, double elevation)
{
    cwFixStation fix;
    fix.setStationName(name);
    fix.setInputCS(cs);
    fix.setCoordinate(easting, northing, elevation);
    return fix;
}

cwCave* addCaveWithFixes(cwCavingRegion* region, const QList<cwFixStation>& fixes)
{
    region->addCave();
    cwCave* cave = region->cave(region->caveCount() - 1);
    REQUIRE(cave != nullptr);
    cave->fixStations()->setFixStations(fixes);
    return cave;
}

//! That \a localCS is a frame centered on the given point — asserted by using
//! the stored string rather than by reading it. The frame puts its origin at
//! x_0 = y_0 = 0, so the point it is centered on is the one that transforms into
//! it at (0, 0). This also exercises that the stored string is usable at all,
//! which parsing +lat_0/+lon_0 back out of it would not.
void checkCenteredOn(const QString& localCS, const QString& cs,
                     double easting, double northing)
{
    REQUIRE_FALSE(localCS.isEmpty());
    const cwCoordinateTransform transform(cs, localCS);
    REQUIRE(transform.isValid());

    const cwGeoPoint origin = transform.transform(cwGeoPoint(easting, northing, 0.0));
    constexpr double kToleranceMeters = 0.001;
    CHECK_THAT(origin.x, WithinAbs(0.0, kToleranceMeters));
    CHECK_THAT(origin.y, WithinAbs(0.0, kToleranceMeters));
}

} // namespace

TEST_CASE("The first fix station with a coordinate anchors the project",
          "[cwLocalProjectionManager]")
{
    cwCavingRegion region;
    REQUIRE(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);

    const cwFixStation fix = makeFix(QStringLiteral("A1"), kUtm12N,
                                     kAnchorEasting, kAnchorNorthing, kElevation);
    addCaveWithFixes(&region, {fix});

    auto* geoReference = region.geoReference();
    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor()
          == cwGeoReference::Anchor{cwGeoReference::Anchor::FixStation, fix.id()});
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N, kAnchorEasting, kAnchorNorthing);
}

TEST_CASE("A fix with nothing readable in it never anchors",
          "[cwLocalProjectionManager]")
{
    // The origin is the one thing in the project that can't be undone by fixing
    // the row that placed it, so only a coordinate that reads back may place it.
    cwCavingRegion region;

    SECTION("a row marked fixed but not filled in") {
        cwFixStation fix;
        fix.setStationName(QStringLiteral("A1"));
        fix.setInputCS(kUtm12N);
        addCaveWithFixes(&region, {fix});
    }

    SECTION("numbers with no coordinate system to read them under") {
        addCaveWithFixes(&region, {makeFix(QStringLiteral("A1"), QString(),
                                           kAnchorEasting, kAnchorNorthing, kElevation)});
    }

    CHECK(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);
    CHECK(region.geoReference()->localCoordinateSystem().isEmpty());
}

TEST_CASE("A first fix that can't be placed hands the anchor to the next one",
          "[cwLocalProjectionManager]")
{
    // A UTM easting typed into a row that says lat/long is nowhere on Earth, so
    // no projection can be centered on it. Stopping at that row would leave the
    // whole project unplaced with a perfectly good fix sitting behind it.
    cwCavingRegion region;

    const cwFixStation unplaceable = makeFix(QStringLiteral("A1"),
                                             QStringLiteral("EPSG:4326"),
                                             kAnchorEasting, kAnchorNorthing, kElevation);
    const cwFixStation good = makeFix(QStringLiteral("A2"), kUtm12N,
                                      kAnchorEasting, kAnchorNorthing, kElevation);
    addCaveWithFixes(&region, {unplaceable, good});

    auto* geoReference = region.geoReference();
    REQUIRE(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor()
          == cwGeoReference::Anchor{cwGeoReference::Anchor::FixStation, good.id()});
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                    kAnchorEasting, kAnchorNorthing);
}

TEST_CASE("Refining the anchor leaves the frame where it is",
          "[cwLocalProjectionManager]")
{
    // A GPS reading replaced by a better one moves the entrance by meters. If
    // that re-derived the frame, every cached coordinate in the project would be
    // invalidated for a change nothing can see.
    cwCavingRegion region;
    const cwFixStation fix = makeFix(QStringLiteral("A1"), kUtm12N,
                                     kAnchorEasting, kAnchorNorthing, kElevation);
    cwCave* cave = addCaveWithFixes(&region, {fix});
    const QString before = region.geoReference()->localCoordinateSystem();
    REQUIRE_FALSE(before.isEmpty());

    cwFixStation refined = fix;
    refined.setCoordinate(kAnchorEasting + 3.0, kAnchorNorthing + 2.0, kElevation);
    cave->fixStations()->setFixStations({refined});

    CHECK(region.geoReference()->localCoordinateSystem() == before);
    CHECK(region.geoReference()->state() == cwGeoReference::Anchored);
}

TEST_CASE("An anchor corrected by more than the threshold re-derives the frame",
          "[cwLocalProjectionManager]")
{
    // The wrong entrance typed first, then corrected: the only automatic move of
    // an origin that already exists.
    cwCavingRegion region;
    const cwFixStation fix = makeFix(QStringLiteral("A1"), kUtm12N,
                                     kAnchorEasting, kAnchorNorthing, kElevation);
    cwCave* cave = addCaveWithFixes(&region, {fix});
    const QString before = region.geoReference()->localCoordinateSystem();

    cwFixStation corrected = fix;
    corrected.setCoordinate(kAnchorEasting, kDistantNorthing, kElevation);  // 200 km north
    cave->fixStations()->setFixStations({corrected});

    auto* geoReference = region.geoReference();
    CHECK(geoReference->localCoordinateSystem() != before);
    CHECK(geoReference->anchor().id == fix.id());
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N, kAnchorEasting, kDistantNorthing);
}

TEST_CASE("A fix that isn't the anchor never moves the origin",
          "[cwLocalProjectionManager]")
{
    // The wrong county's tile, or a second entrance with a typo. It gets flagged
    // as an outlier; what it must not do is drag the whole project to itself.
    cwCavingRegion region;
    const cwFixStation anchor = makeFix(QStringLiteral("A1"), kUtm12N,
                                        kAnchorEasting, kAnchorNorthing, kElevation);
    cwCave* cave = addCaveWithFixes(&region, {anchor});
    const QString before = region.geoReference()->localCoordinateSystem();

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B1"), kUtm12N, kAnchorEasting, kDistantNorthing, kElevation));

    CHECK(region.geoReference()->localCoordinateSystem() == before);
    CHECK(region.geoReference()->anchor().id == anchor.id());
}

TEST_CASE("Deleting the anchor with data still nearby freezes the frame",
          "[cwLocalProjectionManager]")
{
    // The frame is still a good frame; it has just lost the input answerable for
    // it. Handing the role to a neighbor would move the origin for no reason.
    cwCavingRegion region;
    const cwFixStation anchor = makeFix(QStringLiteral("A1"), kUtm12N,
                                        kAnchorEasting, kAnchorNorthing, kElevation);
    cwCave* cave = addCaveWithFixes(&region, {anchor});
    const QString before = region.geoReference()->localCoordinateSystem();

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B1"), kUtm12N, kNearbyEasting, kNearbyNorthing, kElevation));
    cave->fixStations()->removeAt(0);

    auto* geoReference = region.geoReference();
    CHECK(geoReference->state() == cwGeoReference::Frozen);
    CHECK(geoReference->localCoordinateSystem() == before);
    CHECK_FALSE(geoReference->anchor().isValid());
}

TEST_CASE("Deleting the anchor with only distant data left re-derives from what remains",
          "[cwLocalProjectionManager]")
{
    // The wrong tile anchored the project and the right entrance got flagged for
    // it. Deleting the tile has to move the origin to the data that is left,
    // which also clears the flag.
    cwCavingRegion region;
    const cwFixStation wrong = makeFix(QStringLiteral("A1"), kUtm12N,
                                       kAnchorEasting, kAnchorNorthing, kElevation);
    const cwFixStation right = makeFix(QStringLiteral("B1"), kUtm12N,
                                       kAnchorEasting, kDistantNorthing, kElevation);
    cwCave* cave = addCaveWithFixes(&region, {wrong, right});
    REQUIRE(region.geoReference()->anchor().id == wrong.id());

    cave->fixStations()->removeAt(0);

    auto* geoReference = region.geoReference();
    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().id == right.id());
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N, kAnchorEasting, kDistantNorthing);
}

TEST_CASE("Deleting the last georeferenced input un-georeferences the project",
          "[cwLocalProjectionManager]")
{
    cwCavingRegion region;
    cwCave* cave = addCaveWithFixes(&region,
        {makeFix(QStringLiteral("A1"), kUtm12N, kAnchorEasting, kAnchorNorthing, kElevation)});
    REQUIRE(region.geoReference()->state() == cwGeoReference::Anchored);

    cave->fixStations()->removeAt(0);

    CHECK(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);
    CHECK(region.geoReference()->localCoordinateSystem().isEmpty());
}

TEST_CASE("A frozen frame stays put when new data arrives",
          "[cwLocalProjectionManager]")
{
    cwCavingRegion region;
    const cwFixStation anchor = makeFix(QStringLiteral("A1"), kUtm12N,
                                        kAnchorEasting, kAnchorNorthing, kElevation);
    cwCave* cave = addCaveWithFixes(&region, {anchor});
    const QString before = region.geoReference()->localCoordinateSystem();

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B1"), kUtm12N, kNearbyEasting, kNearbyNorthing, kElevation));
    cave->fixStations()->removeAt(0);
    REQUIRE(region.geoReference()->state() == cwGeoReference::Frozen);

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("C1"), kUtm12N, kAnchorEasting, kDistantNorthing, kElevation));

    CHECK(region.geoReference()->state() == cwGeoReference::Frozen);
    CHECK(region.geoReference()->localCoordinateSystem() == before);
}

TEST_CASE("An anchor that hasn't loaded yet is not an anchor that was deleted",
          "[cwLocalProjectionManager]")
{
    // Lidar layers are rescanned off disk after the caves are in place, so a
    // project anchored on one opens with its anchor briefly missing. Reading
    // that as a deletion would move a frame that is stored precisely so that
    // opening a project can't move it.
    cwCavingRegion region;
    cwCavingRegionData data;
    data.geoReference.state = cwGeoReference::Anchored;
    data.geoReference.localCoordinateSystem = kElsewhereCS;
    data.geoReference.anchor = cwGeoReference::Anchor{cwGeoReference::Anchor::LazLayer,
                                                      QUuid::createUuid()};
    region.setData(data);
    REQUIRE(region.geoReference()->state() == cwGeoReference::Anchored);

    // A fix station on the other side of the world — far enough that a frame
    // that did follow it would be unmistakably different.
    addCaveWithFixes(&region,
        {makeFix(QStringLiteral("A1"), kUtm12N, kAnchorEasting, kAnchorNorthing, kElevation)});

    CHECK(region.geoReference()->state() == cwGeoReference::Anchored);
    CHECK(region.geoReference()->localCoordinateSystem() == kElsewhereCS);
}

TEST_CASE("Loading a project keeps the frame the file carries",
          "[cwLocalProjectionManager]")
{
    // The stored frame is the authority — two machines with different PROJ data
    // must not disagree about where a project is. Loading into a region that is
    // already anchored is where that is easiest to get wrong: the caves arriving
    // is an ordinary event, and the frame it would derive is not the one on disk.
    cwCavingRegion source;
    addCaveWithFixes(&source,
        {makeFix(QStringLiteral("A1"), kUtm12N, kAnchorEasting, kAnchorNorthing, kElevation)});
    cwCavingRegionData data = source.data();
    data.geoReference.state = cwGeoReference::Frozen;
    data.geoReference.localCoordinateSystem = kElsewhereCS;
    data.geoReference.anchor = cwGeoReference::Anchor{};

    cwCavingRegion loaded;
    addCaveWithFixes(&loaded,
        {makeFix(QStringLiteral("Z9"), kUtm12N, kAnchorEasting, kDistantNorthing, kElevation)});
    REQUIRE(loaded.geoReference()->state() == cwGeoReference::Anchored);

    loaded.setData(data);

    CHECK(loaded.geoReference()->state() == cwGeoReference::Frozen);
    CHECK(loaded.geoReference()->localCoordinateSystem() == kElsewhereCS);
}
