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
#include "cwLocalProjection.h"
#include "cwLocalProjectionManager.h"
#include "FixStationFixtureHelper.h"
#include "GeoreferenceFixtureHelper.h"

//Qt includes
#include <QSignalSpy>
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

//! A frame centered \a metersNorth of the project's data, in the same units the
//! fixes are written in.
QString frameNorthOfData(double metersNorth)
{
    const QString frame = cwLocalProjection::deriveFrom(
        kUtm12N, cwGeoPoint(kAnchorEasting, kAnchorNorthing + metersNorth, 0.0));
    REQUIRE_FALSE(frame.isEmpty());
    return frame;
}

using cwGeoreferenceFixture::restoreFrozenFrame;

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
    //
    // The stored frame sits 10 km from the data: near enough that it still
    // describes where the project is, and nothing this project's own inputs
    // could have derived, which is what makes keeping it visible.
    const QString stored = frameNorthOfData(10000.0);

    cwCavingRegion source;
    addCaveWithFixes(&source,
        {makeFix(QStringLiteral("A1"), kUtm12N, kAnchorEasting, kAnchorNorthing, kElevation)});
    cwCavingRegionData data = source.data();
    data.geoReference.state = cwGeoReference::Frozen;
    data.geoReference.localCoordinateSystem = stored;
    data.geoReference.anchor = cwGeoReference::Anchor{};

    cwCavingRegion loaded;
    addCaveWithFixes(&loaded,
        {makeFix(QStringLiteral("Z9"), kUtm12N, kAnchorEasting, kDistantNorthing, kElevation)});
    REQUIRE(loaded.geoReference()->state() == cwGeoReference::Anchored);

    loaded.setData(data);

    CHECK(loaded.geoReference()->state() == cwGeoReference::Frozen);
    CHECK(loaded.geoReference()->localCoordinateSystem() == stored);
}

TEST_CASE("A frozen frame the whole project has left behind moves to the middle of it",
          "[cwLocalProjectionManager]")
{
    // A project stored its frame, and everything that could vouch for that frame
    // is gone: what is left sits 200 km away. Nothing is answerable for the
    // origin any more, so the project as a whole is, and the middle of it is
    // where the origin belongs.
    cwCavingRegion region;

    constexpr double kSpacing = 1000.0;
    addCaveWithFixes(&region, {
        makeFix(QStringLiteral("A1"), kUtm12N,
                kAnchorEasting - kSpacing, kAnchorNorthing - kSpacing, kElevation),
        makeFix(QStringLiteral("A2"), kUtm12N,
                kAnchorEasting, kAnchorNorthing, kElevation),
        makeFix(QStringLiteral("A3"), kUtm12N,
                kAnchorEasting + kSpacing, kAnchorNorthing + kSpacing, kElevation)});

    restoreFrozenFrame(&region, frameNorthOfData(200000.0));

    auto* geoReference = region.geoReference();
    CHECK(geoReference->state() == cwGeoReference::Frozen);
    CHECK_FALSE(geoReference->anchor().isValid());
    // The middle fix in both components, which is what the median picks — not
    // the first input, and not the average of the three.
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                    kAnchorEasting, kAnchorNorthing);
}

TEST_CASE("A frame that has been recentered stays put afterward",
          "[cwLocalProjectionManager]")
{
    // Every move of the frame re-decodes every cloud in the project, so the rule
    // has to run to a stop: once the origin is on the data, later evaluations
    // must find nothing left to do.
    cwCavingRegion region;
    cwCave* cave = addCaveWithFixes(&region, {makeFix(QStringLiteral("A1"), kUtm12N,
                                                     kAnchorEasting, kAnchorNorthing, kElevation)});

    QSignalSpy frameSpy(region.geoReference(), &cwGeoReference::localCoordinateSystemChanged);
    restoreFrozenFrame(&region, frameNorthOfData(200000.0));

    // Two moves: the load put the file's frame in place, and the recenter put it
    // back on the data.
    REQUIRE(frameSpy.count() == 2);
    const QString recentered = region.geoReference()->localCoordinateSystem();

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B1"), kUtm12N, kNearbyEasting, kNearbyNorthing, kElevation));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("C1"), kUtm12N, kAnchorEasting, kDistantNorthing, kElevation));

    CHECK(frameSpy.count() == 2);
    CHECK(region.geoReference()->localCoordinateSystem() == recentered);
}

TEST_CASE("Data spread around a frozen frame leaves it where it is",
          "[cwLocalProjectionManager]")
{
    // Two entrances either side of the origin, both past the threshold, with
    // their middle 2.5 km from the origin. Every input says "the frame is not
    // where I am" and the frame is still where the project is, so nothing should
    // move: an origin off by kilometers is not an origin that is wrong, and
    // moving it re-decodes every cloud in the project.
    //
    // This is also why the rule runs to a stop. Landing the origin on the median
    // leaves the next median a hair off the new origin, and a proj string carries
    // lat_0 to ten decimal places — so a rule that moved for any difference at
    // all would keep finding one.
    cwCavingRegion region;

    constexpr double kNorthOfOrigin = 60000.0;
    constexpr double kSouthOfOrigin = 55000.0;
    addCaveWithFixes(&region, {
        makeFix(QStringLiteral("A1"), kUtm12N,
                kAnchorEasting, kAnchorNorthing - kSouthOfOrigin, kElevation),
        makeFix(QStringLiteral("B1"), kUtm12N,
                kAnchorEasting, kAnchorNorthing + kNorthOfOrigin, kElevation)});

    const QString middle = frameNorthOfData(0.0);
    QSignalSpy frameSpy(region.geoReference(), &cwGeoReference::localCoordinateSystemChanged);
    restoreFrozenFrame(&region, middle);

    CHECK(frameSpy.count() == 1);
    CHECK(region.geoReference()->localCoordinateSystem() == middle);
}

TEST_CASE("One input near a frozen frame's origin keeps the frame",
          "[cwLocalProjectionManager]")
{
    // The wrong county's tile again, on the one path where nothing is answerable
    // for the origin: an entrance sitting on the origin says the frame is right,
    // and the input 300 km away is the one that is wrong.
    cwCavingRegion region;

    constexpr double kWrongCounty = 300000.0;
    addCaveWithFixes(&region, {
        makeFix(QStringLiteral("A1"), kUtm12N, kAnchorEasting, kAnchorNorthing, kElevation),
        makeFix(QStringLiteral("B1"), kUtm12N,
                kAnchorEasting, kAnchorNorthing + kWrongCounty, kElevation)});

    const QString middle = frameNorthOfData(0.0);
    restoreFrozenFrame(&region, middle);

    CHECK(region.geoReference()->state() == cwGeoReference::Frozen);
    CHECK(region.geoReference()->localCoordinateSystem() == middle);
}

TEST_CASE("An input the frame can't be related to neither keeps it nor moves it",
          "[cwLocalProjectionManager]")
{
    // A UTM easting typed into a row that says lat/long is nowhere PROJ can
    // place, so it can't say the frame is right and it can't say where a better
    // one would be. Reading it as "close enough" would strand the frame; reading
    // it as "far away" would let a typo vote on where the project is.
    cwCavingRegion region;
    const cwFixStation unplaceable = makeFix(QStringLiteral("A1"),
                                             QStringLiteral("EPSG:4326"),
                                             kAnchorEasting, kAnchorNorthing, kElevation);
    const QString farFrame = frameNorthOfData(200000.0);

    SECTION("on its own it leaves the frame alone") {
        addCaveWithFixes(&region, {unplaceable});
        restoreFrozenFrame(&region, farFrame);

        CHECK(region.geoReference()->localCoordinateSystem() == farFrame);
    }

    SECTION("beside a readable one, the readable one decides") {
        addCaveWithFixes(&region, {unplaceable,
                                   makeFix(QStringLiteral("A2"), kUtm12N,
                                           kAnchorEasting, kAnchorNorthing, kElevation)});
        restoreFrozenFrame(&region, farFrame);

        checkCenteredOn(region.geoReference()->localCoordinateSystem(), kUtm12N,
                        kAnchorEasting, kAnchorNorthing);
    }

    CHECK(region.geoReference()->state() == cwGeoReference::Frozen);
}
