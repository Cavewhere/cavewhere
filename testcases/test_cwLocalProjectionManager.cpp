/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwExternalCenterline.h"
#include "cwFixStation.h"
#include "cwFixStationDiagnostics.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwLocalProjection.h"
#include "cwLocalProjectionManager.h"
#include "cwRecenterCandidateModel.h"
#include "FixStationFixtureHelper.h"
#include "GeoreferenceFixtureHelper.h"
#include "TestHelper.h"

//Qt includes
#include <QCoreApplication>
#include <QSignalSpy>
#include <QUuid>

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

const QString kUtm12N = QStringLiteral("EPSG:32612");

constexpr double kAnchorEasting = 500000.0;

//! A million meters past the zone's edge: a real coordinate system, and a
//! number nowhere inside it — a transposed digit, or the wrong zone.
constexpr double kOutOfZoneEasting = 1478000.0;
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

//! \a role of \a row, read the way a delegate reads it.
QVariant candidateRole(const cwRecenterCandidateModel* model, int row,
                       cwRecenterCandidateModel::Roles role)
{
    return model->data(model->index(row, 0), role);
}

//! \a region's picker rows, gathered the way opening the picker gathers them.
//! The model exists from first access and holds nothing until refresh() fills
//! it, so every reader here starts by opening the picker.
cwRecenterCandidateModel* openedCandidates(cwCavingRegion* region)
{
    cwRecenterCandidateModel* candidates = region->localProjection()->recenterCandidates();
    candidates->refresh();
    return candidates;
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
    REQUIRE(waitForQueuedEvents());

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
    REQUIRE(waitUntil([&] { return region.geoReference()->localCoordinateSystem() != before; }));

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
    REQUIRE(waitForQueuedEvents());

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
    REQUIRE(waitForQueuedEvents());
    cave->fixStations()->removeAt(0);
    REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Frozen; }));

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
    REQUIRE(waitUntil([&] { return region.geoReference()->anchor().id == right.id(); }));

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
    REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Ungeoreferenced; }));

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
    REQUIRE(waitForQueuedEvents());
    cave->fixStations()->removeAt(0);
    REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Frozen; }));
    REQUIRE(region.geoReference()->state() == cwGeoReference::Frozen);

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("C1"), kUtm12N, kAnchorEasting, kDistantNorthing, kElevation));
    REQUIRE(waitForQueuedEvents());

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
    REQUIRE(waitForQueuedEvents());
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
    REQUIRE(waitForQueuedEvents());

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
    REQUIRE(waitForQueuedEvents());
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("C1"), kUtm12N, kAnchorEasting, kDistantNorthing, kElevation));
    REQUIRE(waitForQueuedEvents());

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

TEST_CASE("Recentering on a station makes it the project's anchor",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // The user picking a station is the same move the state machine makes when
    // it anchors, so it lands in the same state — nothing about the frame
    // records that a person chose it rather than the rule.
    cwCavingRegion region;
    const cwFixStation entrance = makeFix(QStringLiteral("A1"), kUtm12N,
                                          kAnchorEasting, kAnchorNorthing, kElevation);
    const cwFixStation surveyed = makeFix(QStringLiteral("B1"), kUtm12N,
                                          kAnchorEasting + 3000.0, kAnchorNorthing + 4000.0,
                                          kElevation);
    addCaveWithFixes(&region, {entrance, surveyed});

    auto* geoReference = region.geoReference();
    REQUIRE(geoReference->anchor().id == entrance.id());

    QSignalSpy frameSpy(geoReference, &cwGeoReference::localCoordinateSystemChanged);
    CHECK(region.localProjection()->recenterOnStation(surveyed.id()));

    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().id == surveyed.id());
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                    kAnchorEasting + 3000.0, kAnchorNorthing + 4000.0);

    // One move of the frame, so the project's clouds re-decode once.
    CHECK(frameSpy.count() == 1);
}

TEST_CASE("The lifecycle follows a station the user picked",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // What proves the pick joined the normal lifecycle rather than pinning the
    // frame: the new anchor answers for the origin exactly as a derived one
    // would, and the old anchor has stopped answering for anything.
    cwCavingRegion region;
    const cwFixStation entrance = makeFix(QStringLiteral("A1"), kUtm12N,
                                          kAnchorEasting, kAnchorNorthing, kElevation);
    const cwFixStation surveyed = makeFix(QStringLiteral("B1"), kUtm12N,
                                          kAnchorEasting + 3000.0, kAnchorNorthing + 4000.0,
                                          kElevation);
    cwCave* cave = addCaveWithFixes(&region, {entrance, surveyed});
    REQUIRE(region.localProjection()->recenterOnStation(surveyed.id()));

    auto* geoReference = region.geoReference();
    const QString picked = geoReference->localCoordinateSystem();

    cwFixStation refined = entrance;
    refined.setCoordinate(kAnchorEasting + 3.0, kAnchorNorthing + 2.0, kElevation);
    cave->fixStations()->setFixStations({refined, surveyed});
    REQUIRE(waitForQueuedEvents());
    CHECK(geoReference->localCoordinateSystem() == picked);

    cave->fixStations()->removeAt(1);
    REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Frozen; }));

    // The hand-off every deleted anchor gets: the frame is still a good frame,
    // so it keeps it and gives up the anchor.
    CHECK(geoReference->state() == cwGeoReference::Frozen);
    CHECK(geoReference->localCoordinateSystem() == picked);
    CHECK_FALSE(geoReference->anchor().isValid());
}

TEST_CASE("Recentering on the data's middle freezes the frame there",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // Deliberately asymmetric: a mean of these would land 2.3 km east and 4.7 km
    // north of the median, so the assertion can tell the two apart. Symmetric
    // inputs put both statistics in the same place and could not fail.
    cwCavingRegion region;
    addCaveWithFixes(&region, {
        makeFix(QStringLiteral("A1"), kUtm12N,
                kAnchorEasting, kAnchorNorthing, kElevation),
        makeFix(QStringLiteral("A2"), kUtm12N,
                kAnchorEasting + 1000.0, kAnchorNorthing + 2000.0, kElevation),
        makeFix(QStringLiteral("A3"), kUtm12N,
                kAnchorEasting + 9000.0, kAnchorNorthing + 18000.0, kElevation)});

    auto* geoReference = region.geoReference();
    REQUIRE(geoReference->state() == cwGeoReference::Anchored);

    CHECK(region.localProjection()->recenterOnDataCenter());

    CHECK(geoReference->state() == cwGeoReference::Frozen);
    CHECK_FALSE(geoReference->anchor().isValid());
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                    kAnchorEasting + 1000.0, kAnchorNorthing + 2000.0);
}

TEST_CASE("A frame already frozen on the data's middle says so",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // The picker grays out the row the frame already sits on. A station row
    // answers that by identity against the anchor; freezing leaves no anchor
    // behind, so the middle-of-the-data row has to answer it by position.
    cwCavingRegion region;
    cwCave* cave = addCaveWithFixes(&region, {
        makeFix(QStringLiteral("A1"), kUtm12N,
                kAnchorEasting, kAnchorNorthing, kElevation),
        makeFix(QStringLiteral("A2"), kUtm12N,
                kNearbyEasting, kNearbyNorthing, kElevation)});

    // Anchored on a station, even one sitting on the middle of the data:
    // centering would still cut the frame loose from the input it follows.
    REQUIRE(region.geoReference()->state() == cwGeoReference::Anchored);
    CHECK_FALSE(region.localProjection()->isCenteredOnDataCenter());
    CHECK_FALSE(openedCandidates(&region)->dataCenterIsCurrent());

    REQUIRE(region.localProjection()->recenterOnDataCenter());
    REQUIRE(region.geoReference()->state() == cwGeoReference::Frozen);

    CHECK(region.localProjection()->isCenteredOnDataCenter());
    CHECK(openedCandidates(&region)->dataCenterIsCurrent());

    // Data added on one side moves the middle off the origin, so the row is a
    // real move again.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A3"), kUtm12N,
                kAnchorEasting + 4000.0, kAnchorNorthing + 4000.0, kElevation));
    REQUIRE(waitForQueuedEvents());

    CHECK_FALSE(region.localProjection()->isCenteredOnDataCenter());
    CHECK_FALSE(openedCandidates(&region)->dataCenterIsCurrent());
}

TEST_CASE("Asking whether the frame is centered with a center in hand gives the same answer",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // The picker already holds dataCenter() when it asks, and recomputing it
    // walks every input through PROJ again. The overload it uses instead has to
    // answer identically, in both states of the frame.
    cwCavingRegion region;
    addCaveWithFixes(&region, {
        makeFix(QStringLiteral("A1"), kUtm12N,
                kAnchorEasting, kAnchorNorthing, kElevation),
        makeFix(QStringLiteral("A2"), kUtm12N,
                kNearbyEasting, kNearbyNorthing, kElevation)});

    cwLocalProjectionManager* projection = region.localProjection();

    REQUIRE(region.geoReference()->state() == cwGeoReference::Anchored);
    CHECK(projection->isCenteredOnDataCenter(projection->dataCenter())
          == projection->isCenteredOnDataCenter());

    REQUIRE(projection->recenterOnDataCenter());
    REQUIRE(region.geoReference()->state() == cwGeoReference::Frozen);
    CHECK(projection->isCenteredOnDataCenter(projection->dataCenter())
          == projection->isCenteredOnDataCenter());
    CHECK(projection->isCenteredOnDataCenter(projection->dataCenter()));

    // A project the frame can place nothing in has no middle to be centered on.
    CHECK_FALSE(projection->isCenteredOnDataCenter(std::nullopt));
}

TEST_CASE("A station the project's data would sit far from can't be recentered on",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // Centering on the wrong county's entrance would put the whole survey 200 km
    // out in its own frame — every coordinate large, every distortion real. The
    // picker grays the row out; the action refuses it too, because the model can
    // change between the list being drawn and the click.
    cwCavingRegion region;
    const cwFixStation first = makeFix(QStringLiteral("A1"), kUtm12N,
                                       kAnchorEasting, kAnchorNorthing, kElevation);
    const cwFixStation second = makeFix(QStringLiteral("A2"), kUtm12N,
                                        kAnchorEasting + 100.0, kAnchorNorthing + 100.0,
                                        kElevation);
    const cwFixStation third = makeFix(QStringLiteral("A3"), kUtm12N,
                                       kAnchorEasting + 200.0, kAnchorNorthing + 200.0,
                                       kElevation);
    const cwFixStation elsewhere = makeFix(QStringLiteral("Z1"), kUtm12N,
                                           kAnchorEasting, kDistantNorthing, kElevation);
    cwCave* cave = addCaveWithFixes(&region, {first, second, third, elsewhere});
    cave->setName(QStringLiteral("Roppel Cave"));

    const cwRecenterCandidateModel* candidates = openedCandidates(&region);
    REQUIRE(candidates->count() == 4);

    CHECK(candidateRole(candidates, 3, cwRecenterCandidateModel::StationIdRole).toUuid()
          == elsewhere.id());
    CHECK(candidateRole(candidates, 3, cwRecenterCandidateModel::StationNameRole).toString()
          == QStringLiteral("Z1"));
    CHECK(candidateRole(candidates, 3, cwRecenterCandidateModel::CaveNameRole).toString()
          == QStringLiteral("Roppel Cave"));
    CHECK_FALSE(candidateRole(candidates, 3, cwRecenterCandidateModel::EligibleRole).toBool());

    for (int row = 0; row < 3; ++row) {
        CHECK(candidateRole(candidates, row, cwRecenterCandidateModel::EligibleRole).toBool());
    }

    auto* geoReference = region.geoReference();
    const QString before = geoReference->localCoordinateSystem();
    CHECK_FALSE(region.localProjection()->recenterOnStation(elsewhere.id()));
    CHECK(geoReference->localCoordinateSystem() == before);
    CHECK(geoReference->anchor().id == first.id());
}

TEST_CASE("Every candidate says where on Earth it is",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // The picker prints the coordinate under each row, which is what tells two
    // caves' "entrance" stations apart and what catches a fix typed into the
    // wrong zone before the whole project is centered on it. Zone 12N's central
    // meridian is 111 W, and 4194000 m north of the equator is close to 37.9 N.
    cwCavingRegion region;
    addCaveWithFixes(&region, {makeFix(QStringLiteral("A1"), kUtm12N,
                                       kAnchorEasting, kAnchorNorthing, kElevation),
                               makeFix(QStringLiteral("B1"), QString(),
                                       kAnchorEasting, kAnchorNorthing, kElevation)});

    const cwRecenterCandidateModel* candidates = openedCandidates(&region);
    REQUIRE(candidates->count() == 1);

    CHECK(candidateRole(candidates, 0, cwRecenterCandidateModel::HasCoordinateRole).toBool());
    CHECK_THAT(candidateRole(candidates, 0, cwRecenterCandidateModel::LatitudeRole).toDouble(),
               WithinAbs(37.888, 0.01));
    CHECK_THAT(candidateRole(candidates, 0, cwRecenterCandidateModel::LongitudeRole).toDouble(),
               WithinAbs(-111.0, 0.01));

    // The middle of the data is the other thing the picker offers, so it says
    // where it is on the same terms. One station makes it that station.
    CHECK(candidates->hasDataCenter());
    CHECK_THAT(candidates->dataCenterLatitude(), WithinAbs(37.888, 0.01));
    CHECK_THAT(candidates->dataCenterLongitude(), WithinAbs(-111.0, 0.01));
}

TEST_CASE("A project with nothing to center on has no middle either",
          "[cwLocalProjectionManager][cwRecenter]")
{
    cwCavingRegion region;
    addCaveWithFixes(&region, {makeFix(QStringLiteral("A1"), QString(),
                                       kAnchorEasting, kAnchorNorthing, kElevation)});
    REQUIRE(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);

    CHECK_FALSE(openedCandidates(&region)->hasDataCenter());
}

TEST_CASE("The candidate rows are the project as of the last refresh",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // Eligibility reprojects every station in the project, so the rows are built
    // when the picker asks for them rather than kept current against edits
    // nothing is displaying.
    cwCavingRegion region;
    cwCave* cave = addCaveWithFixes(&region, {makeFix(QStringLiteral("A1"), kUtm12N,
                                                      kAnchorEasting, kAnchorNorthing,
                                                      kElevation)});

    cwRecenterCandidateModel* candidates = openedCandidates(&region);
    REQUIRE(candidates->count() == 1);

    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B1"), kUtm12N, kNearbyEasting, kNearbyNorthing, kElevation));
    REQUIRE(waitForQueuedEvents());
    CHECK(candidates->count() == 1);

    candidates->refresh();
    CHECK(candidates->count() == 2);
    CHECK(candidateRole(candidates, 1, cwRecenterCandidateModel::StationNameRole).toString()
          == QStringLiteral("B1"));
}

TEST_CASE("A project with no frame has nothing to recenter",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // Recentering re-derives a frame the project already has. It is not how the
    // first one gets made, so it has nothing to offer a project that has none.
    cwCavingRegion region;
    addCaveWithFixes(&region, {makeFix(QStringLiteral("A1"), QString(),
                                       kAnchorEasting, kAnchorNorthing, kElevation)});
    REQUIRE(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);

    CHECK(openedCandidates(&region)->count() == 0);
    CHECK_FALSE(region.localProjection()->recenterOnDataCenter());
    CHECK_FALSE(region.localProjection()->recenterOnStation(QUuid::createUuid()));
    CHECK(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);
}

TEST_CASE("Recentering on a station that isn't there leaves the frame alone",
          "[cwLocalProjectionManager][cwRecenter]")
{
    cwCavingRegion region;
    const cwFixStation fix = makeFix(QStringLiteral("A1"), kUtm12N,
                                     kAnchorEasting, kAnchorNorthing, kElevation);
    addCaveWithFixes(&region, {fix});
    const QString before = region.geoReference()->localCoordinateSystem();

    CHECK_FALSE(region.localProjection()->recenterOnStation(QUuid::createUuid()));
    CHECK(region.geoReference()->localCoordinateSystem() == before);
    CHECK(region.geoReference()->anchor().id == fix.id());
}

TEST_CASE("The frame takes the datum of the station it is recentered on",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // A pick re-derives on the station's own system, so it adopts that station's
    // datum — the same rule that applies when the state machine anchors. Both
    // stations are within a few kilometers of each other; only the datum they
    // are declared on differs.
    //
    // Both datums are declared ones, so cwLocalProjection's plate-fixed rule
    // leaves them alone and the datum that arrives is the station's alone.
    cwCavingRegion region;
    const cwFixStation onNad27 = makeFix(QStringLiteral("A1"),
                                         QStringLiteral("EPSG:26716"),
                                         kAnchorEasting, kAnchorNorthing, kElevation);
    const cwFixStation onNad83 = makeFix(QStringLiteral("B1"),
                                         QStringLiteral("EPSG:26916"),
                                         kAnchorEasting + 2000.0, kAnchorNorthing,
                                         kElevation);
    addCaveWithFixes(&region, {onNad27, onNad83});

    auto* geoReference = region.geoReference();
    REQUIRE(geoReference->anchor().id == onNad27.id());
    REQUIRE_THAT(geoReference->datumName().toStdString(),
                 ContainsSubstring("North American Datum 1927"));

    REQUIRE(region.localProjection()->recenterOnStation(onNad83.id()));

    CHECK_THAT(geoReference->datumName().toStdString(),
               ContainsSubstring("North American Datum 1983"));
}

TEST_CASE("Recentering on the data's middle keeps the frame's datum",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // The middle of the data is a point in the frame's own coordinates, so it
    // says where to put the origin and nothing about what to measure it on.
    // Inputs on mixed datums vote on position, never on datum.
    cwCavingRegion region;
    addCaveWithFixes(&region, {
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:26916"),
                kAnchorEasting, kAnchorNorthing, kElevation),
        makeFix(QStringLiteral("B1"), QStringLiteral("EPSG:32616"),
                kAnchorEasting + 2000.0, kAnchorNorthing, kElevation)});

    auto* geoReference = region.geoReference();
    REQUIRE_THAT(geoReference->datumName().toStdString(),
                 ContainsSubstring("North American Datum 1983"));

    REQUIRE(region.localProjection()->recenterOnDataCenter());

    CHECK_THAT(geoReference->datumName().toStdString(),
               ContainsSubstring("North American Datum 1983"));
}

TEST_CASE("A stored frame on WGS84 stays on WGS84 when it is recentered",
          "[cwLocalProjectionManager][cwRecenter]")
{
    // A project placed before CaveWhere started choosing plate-fixed datums
    // carries a WGS84 frame. Recentering it is a move, not a migration: the
    // datum a frame is stored with is the one it keeps, whichever path moves it.
    cwCavingRegion region;
    constexpr double kSpacing = 1000.0;
    addCaveWithFixes(&region, {
        makeFix(QStringLiteral("A1"), kUtm12N,
                kAnchorEasting - kSpacing, kAnchorNorthing - kSpacing, kElevation),
        makeFix(QStringLiteral("A2"), kUtm12N,
                kAnchorEasting, kAnchorNorthing, kElevation),
        makeFix(QStringLiteral("A3"), kUtm12N,
                kAnchorEasting + kSpacing, kAnchorNorthing + kSpacing, kElevation)});

    const QString legacyFrame = cwLocalProjection::deriveFrom(
        kUtm12N, cwGeoPoint(kAnchorEasting, kAnchorNorthing + 200000.0, 0.0),
        cwLocalProjection::DatumSource::StoredFrame);
    REQUIRE_THAT(cwLocalProjection::datumName(legacyFrame).toStdString(),
                 ContainsSubstring("World Geodetic System 1984"));

    auto* geoReference = region.geoReference();

    // The automatic path: the frame arrives 200 km off its data and the manager
    // moves it onto the middle of it.
    restoreFrozenFrame(&region, legacyFrame);
    REQUIRE(geoReference->localCoordinateSystem() != legacyFrame);
    CHECK_THAT(geoReference->datumName().toStdString(),
               ContainsSubstring("World Geodetic System 1984"));

    // And the Recenter dialog's path, which asks for the same move outright.
    REQUIRE(region.localProjection()->recenterOnDataCenter());
    CHECK_THAT(geoReference->datumName().toStdString(),
               ContainsSubstring("World Geodetic System 1984"));
}

TEST_CASE("The manager names what the frame is centered on",
          "[cwLocalProjectionManager][cwAnchorDescription]")
{
    cwCavingRegion region;
    auto* localProjection = region.localProjection();
    CHECK(localProjection->anchorDescription().isEmpty());

    const cwFixStation fix = makeFix(QStringLiteral("A42"), kUtm12N,
                                     kAnchorEasting, kAnchorNorthing, kElevation);
    cwCave* cave = addCaveWithFixes(&region, {fix});
    cave->setName(QStringLiteral("Roppel Cave"));

    REQUIRE(region.geoReference()->state() == cwGeoReference::Anchored);
    CHECK(localProjection->anchorDescription() == QStringLiteral("A42 — Roppel Cave"));

    SECTION("renaming the cave moves the description") {
        QSignalSpy spy(localProjection, &cwLocalProjectionManager::anchorDescriptionChanged);

        cave->setName(QStringLiteral("Hidden River Cave"));

        CHECK(spy.count() == 1);
        CHECK(localProjection->anchorDescription() == QStringLiteral("A42 — Hidden River Cave"));
    }

    SECTION("renaming the station moves the description") {
        QSignalSpy spy(localProjection, &cwLocalProjectionManager::anchorDescriptionChanged);

        cwFixStation renamed = fix;
        renamed.setStationName(QStringLiteral("A43"));
        cave->fixStations()->setFixStations({renamed});
        REQUIRE(waitUntil([&] { return spy.count() >= 1; }));

        CHECK(spy.count() == 1);
        REQUIRE(waitForQueuedEvents());
        CHECK(spy.count() == 1);
        CHECK(localProjection->anchorDescription() == QStringLiteral("A43 — Roppel Cave"));
    }

    SECTION("a frame with no anchor left names nothing") {
        QSignalSpy spy(localProjection, &cwLocalProjectionManager::anchorDescriptionChanged);

        // The anchor was the only georeferenced input, so deleting it takes the
        // frame with it — and there is nothing left the description could name.
        cave->fixStations()->setFixStations({});
        REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Ungeoreferenced; }));

        REQUIRE(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);
        CHECK(spy.count() >= 1);
        CHECK(localProjection->anchorDescription().isEmpty());
    }
}

TEST_CASE("Recentering renames what the frame is centered on",
          "[cwLocalProjectionManager][cwRecenter][cwAnchorDescription]")
{
    // The recentering the user asks for moves the frame without running the
    // state machine. A description left to evaluate() would go on naming the
    // station the project was anchored on before the click — and on a project
    // with no GIS layers nothing would ever come along to correct it.
    cwCavingRegion region;
    const cwFixStation entrance = makeFix(QStringLiteral("A1"), kUtm12N,
                                          kAnchorEasting, kAnchorNorthing, kElevation);
    const cwFixStation surveyed = makeFix(QStringLiteral("B1"), kUtm12N,
                                          kAnchorEasting + 3000.0, kAnchorNorthing + 4000.0,
                                          kElevation);
    cwCave* cave = addCaveWithFixes(&region, {entrance, surveyed});
    cave->setName(QStringLiteral("Roppel Cave"));

    auto* geoReference = region.geoReference();
    auto* localProjection = region.localProjection();
    REQUIRE(localProjection->anchorDescription() == QStringLiteral("A1 — Roppel Cave"));

    SECTION("a picked station is the one named") {
        QSignalSpy spy(localProjection, &cwLocalProjectionManager::anchorDescriptionChanged);

        REQUIRE(localProjection->recenterOnStation(surveyed.id()));

        CHECK(spy.count() == 1);
        CHECK(localProjection->anchorDescription() == QStringLiteral("B1 — Roppel Cave"));
    }

    SECTION("the data's middle is no station, so it names nothing") {
        QSignalSpy spy(localProjection, &cwLocalProjectionManager::anchorDescriptionChanged);

        REQUIRE(localProjection->recenterOnDataCenter());

        REQUIRE(geoReference->state() == cwGeoReference::Frozen);
        CHECK(spy.count() == 1);
        CHECK(localProjection->anchorDescription().isEmpty());
    }

    SECTION("a refused recentering leaves the description alone") {
        QSignalSpy spy(localProjection, &cwLocalProjectionManager::anchorDescriptionChanged);

        CHECK_FALSE(localProjection->recenterOnStation(QUuid::createUuid()));

        CHECK(spy.count() == 0);
        CHECK(localProjection->anchorDescription() == QStringLiteral("A1 — Roppel Cave"));
    }
}

TEST_CASE("The region walk reaches past the first cave",
          "[cwLocalProjectionManager][cwRecenter][cwAnchorDescription]")
{
    // Everything that resolves a fix station — the picker's rows, recentering,
    // the description — shares one walk of the region. A walk that stopped at
    // the first cave, or at the first cave holding no fixes, would lose every
    // project surveyed as more than one cave.
    cwCavingRegion region;

    const cwFixStation entrance = makeFix(QStringLiteral("A1"), kUtm12N,
                                          kAnchorEasting, kAnchorNorthing, kElevation);
    cwCave* first = addCaveWithFixes(&region, {entrance});
    first->setName(QStringLiteral("Roppel Cave"));

    // A cave nobody has fixed yet sits between the two, so the walk has to carry
    // on through a cave that contributes nothing.
    addCaveWithFixes(&region, {})->setName(QStringLiteral("Unfixed Cave"));

    const cwFixStation nearby = makeFix(QStringLiteral("B1"), kUtm12N,
                                        kNearbyEasting, kNearbyNorthing, kElevation);
    cwCave* second = addCaveWithFixes(&region, {nearby});
    second->setName(QStringLiteral("Hidden River Cave"));

    auto* geoReference = region.geoReference();
    auto* localProjection = region.localProjection();
    REQUIRE(localProjection->anchorDescription() == QStringLiteral("A1 — Roppel Cave"));

    SECTION("the picker offers the stations of every cave") {
        auto* candidates = openedCandidates(&region);

        REQUIRE(candidates->count() == 2);
        CHECK(candidateRole(candidates, 1, cwRecenterCandidateModel::StationNameRole).toString()
              == QStringLiteral("B1"));
        CHECK(candidateRole(candidates, 1, cwRecenterCandidateModel::CaveNameRole).toString()
              == QStringLiteral("Hidden River Cave"));
        CHECK(candidateRole(candidates, 1, cwRecenterCandidateModel::EligibleRole).toBool());
    }

    SECTION("a station in the last cave is the one recentered on and named") {
        REQUIRE(localProjection->recenterOnStation(nearby.id()));

        CHECK(localProjection->anchorDescription() == QStringLiteral("B1 — Hidden River Cave"));
        checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                        kNearbyEasting, kNearbyNorthing);
    }

    SECTION("both caves' fixes place the project") {
        // gatherInputs() walks the same hierarchy: the middle of the project is
        // between the two entrances, not on the first one.
        const auto center = localProjection->dataCenter();
        REQUIRE(center.has_value());
        constexpr double kHalfway = 50.0;
        // Loose enough for the frame's own scale factor: the two entrances are
        // 100 m apart in UTM, and UTM at this easting runs a few hundred ppm
        // long against the tmerc the frame is.
        constexpr double kToleranceMeters = 0.1;
        CHECK_THAT(center->x, WithinAbs(kHalfway, kToleranceMeters));
        CHECK_THAT(center->y, WithinAbs(kHalfway, kToleranceMeters));
    }
}

TEST_CASE("A domain-invalid fix never anchors the project (#660)",
          "[cwLocalProjectionManager][issue660]")
{
    // The typo parses and its CS is real, but the coordinate is nowhere the
    // zone covers. Anchoring on it centers the frame on the typo and puts every
    // good fix hundreds of kilometers from the origin.
    const cwFixStation typo = makeFix(QStringLiteral("A1"), kUtm12N,
                                      kOutOfZoneEasting, kAnchorNorthing, kElevation);
    REQUIRE_FALSE(cwFixStationDiagnostics::isDomainValid(typo));
    const cwFixStation good = makeFix(QStringLiteral("A2"), kUtm12N,
                                      kAnchorEasting, kAnchorNorthing, kElevation);

    SECTION("the typo entered first hands the anchor to the good fix") {
        cwCavingRegion region;
        addCaveWithFixes(&region, {typo, good});
        auto* geoReference = region.geoReference();
        REQUIRE(geoReference->state() == cwGeoReference::Anchored);
        CHECK(geoReference->anchor()
              == cwGeoReference::Anchor{cwGeoReference::Anchor::FixStation, good.id()});
        checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                        kAnchorEasting, kAnchorNorthing);
    }

    SECTION("a project whose only fix is a typo stays ungeoreferenced") {
        cwCavingRegion region;
        addCaveWithFixes(&region, {typo});
        CHECK(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);
    }
}

TEST_CASE("A stored anchor the domain gate refuses can still be deleted (#660)",
          "[cwLocalProjectionManager][issue660]")
{
    // A project saved before the gate existed can open anchored on a typo. That
    // anchor can never appear among the inputs again, so deleting it — the
    // user's own repair — is the one way out, and reading that as an anchor
    // that hasn't loaded yet would strand the project on the typo forever.
    const cwFixStation typo = makeFix(QStringLiteral("A1"), kUtm12N,
                                      kOutOfZoneEasting, kAnchorNorthing, kElevation);
    const cwFixStation good = makeFix(QStringLiteral("A2"), kUtm12N,
                                      kAnchorEasting, kAnchorNorthing, kElevation);

    cwCavingRegion source;
    addCaveWithFixes(&source, {typo, good});
    cwCavingRegionData data = source.data();
    data.geoReference.state = cwGeoReference::Anchored;
    data.geoReference.localCoordinateSystem = kElsewhereCS;
    data.geoReference.anchor = cwGeoReference::Anchor{cwGeoReference::Anchor::FixStation,
                                                      typo.id()};

    cwCavingRegion region;
    region.setData(data);
    REQUIRE(waitForQueuedEvents());
    auto* geoReference = region.geoReference();
    REQUIRE(geoReference->state() == cwGeoReference::Anchored);
    REQUIRE(geoReference->anchor().id == typo.id());
    REQUIRE(geoReference->localCoordinateSystem() == kElsewhereCS);

    REQUIRE(region.caveCount() == 1);
    region.cave(0)->fixStations()->removeAt(0);
    REQUIRE(waitUntil([&] { return region.geoReference()->anchor() == cwGeoReference::Anchor{cwGeoReference::Anchor::FixStation, good.id()}; }));

    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor()
          == cwGeoReference::Anchor{cwGeoReference::Anchor::FixStation, good.id()});
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                    kAnchorEasting, kAnchorNorthing);
}

TEST_CASE("An anchor edited into a typo stops holding the frame (#660)",
          "[cwLocalProjectionManager][issue660]")
{
    // Typing the wrong easting into the anchor itself reads exactly like
    // deleting it: the fix is still there, but nothing the frame can follow.
    cwCavingRegion region;
    const cwFixStation anchor = makeFix(QStringLiteral("A1"), kUtm12N,
                                        kAnchorEasting, kAnchorNorthing, kElevation);
    cwCave* cave = addCaveWithFixes(&region, {anchor});
    auto* geoReference = region.geoReference();
    const QString before = geoReference->localCoordinateSystem();

    cwFixStation typo = anchor;
    typo.setCoordinate(kOutOfZoneEasting, kAnchorNorthing, kElevation);

    SECTION("the only fix gives the project back its ungeoreferenced state") {
        cave->fixStations()->setFixStations({typo});
        REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Ungeoreferenced; }));

        CHECK(geoReference->state() == cwGeoReference::Ungeoreferenced);

        // Correcting the typo anchors the project again.
        cave->fixStations()->setFixStations({anchor});
        REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Anchored; }));
        CHECK(geoReference->state() == cwGeoReference::Anchored);
        checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                        kAnchorEasting, kAnchorNorthing);
    }

    SECTION("a neighbor still near the origin keeps the frame, frozen") {
        const cwFixStation neighbor = makeFix(QStringLiteral("B1"), kUtm12N,
                                              kNearbyEasting, kNearbyNorthing, kElevation);
        cave->fixStations()->appendFixStation(neighbor);
        REQUIRE(waitForQueuedEvents());
        cave->fixStations()->setFixStations({typo, neighbor});
        REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Frozen; }));

        CHECK(geoReference->state() == cwGeoReference::Frozen);
        CHECK(geoReference->localCoordinateSystem() == before);
    }
}

namespace {

//! An attached file's fix on \a ownerId at (\a easting, \a northing,
//! kElevation), its coordinate written in meters as the scan stores it.
cwAttachedFix makeAttachedFix(const QString& station, const QUuid& ownerId,
                              double easting, double northing)
{
    cwAttachedFix fix;
    fix.station = station;
    fix.coordinate = QStringLiteral("%1 %2 %3").arg(easting, 0, 'f', 3)
                         .arg(northing, 0, 'f', 3).arg(kElevation, 0, 'f', 3);
    fix.ownerId = ownerId;
    fix.fileName = QStringLiteral("attached.svx");
    return fix;
}

//! The fix as a file that names its own system for it lists it.
cwAttachedFix withOwnSystem(cwAttachedFix fix, const QString& coordinateSystem,
                            const QString& placementSystem = QString())
{
    fix.coordinateSystem = coordinateSystem;
    fix.placementSystem = placementSystem;
    return fix;
}

//! A new cave in \a region named \a name, attached to a file, with \a system
//! chosen for the file's bare fixes (none when empty).
cwCave* addAttachedCave(cwCavingRegion* region, const QString& name,
                        const QString& system = QString())
{
    cwCave* cave = addCaveWithFixes(region, {});
    cave->setName(name);
    cwExternalCenterline centerline(QStringLiteral("attached.svx"));
    centerline.setFixCoordinateSystem(system);
    cave->setExternalCenterline(centerline);
    REQUIRE(waitForQueuedEvents());
    return cave;
}

//! The frame anchoring on \a fix derives: the one deriveFrom() builds at the
//! fix's placement in its own system.
QString frameAt(const cwAttachedFix& fix)
{
    const std::optional<cwAttachedFix::Placement> placement = fix.placementIn(fix.solveSystem());
    REQUIRE(placement.has_value());
    const QString frame = cwLocalProjection::deriveFrom(placement->cs, placement->point);
    REQUIRE_FALSE(frame.isEmpty());
    return frame;
}

constexpr double kOwnSystemEasting = 500200.0;
constexpr double kOwnSystemNorthing = 3000000.0;
constexpr double kCompassEasting = 500000.0;
constexpr double kCompassNorthing = 4000000.0;
constexpr double kWallsEasting = 520000.0;
constexpr double kWallsNorthing = 4040000.0;
constexpr double kNorthBareEasting = 500100.0;
constexpr double kNorthBareNorthing = 3000100.0;
constexpr double kNorthOwnEasting = 500000.0;
constexpr double kNorthOwnNorthing = 3000000.0;

//! A second attached fix near enough to the anchor to vote on the data's
//! middle without leaving the 50 km threshold.
constexpr double kSpreadEasting = kAnchorEasting + 2000.0;

} // namespace

TEST_CASE("An attached fix whose file names its own system gives a project with no frame one",
          "[cwLocalProjectionManager][AttachedFrame]")
{
    struct Format
    {
        const char* label;
        QString station;
        QString coordinateSystem;
        QString placementSystem;
        double easting;
        double northing;
    };
    const Format format = GENERATE(
        Format{"Survex *cs", QStringLiteral("scoped.S1"), QStringLiteral("UTM48N"), QString(),
               kOwnSystemEasting, kOwnSystemNorthing},
        Format{"Walls .REF", QStringLiteral("M1"), QStringLiteral("NAD27 CONUS, UTM zone 16N"),
               QStringLiteral("EPSG:26716"), kWallsEasting, kWallsNorthing},
        Format{"Compass .mak", QStringLiteral("alpha.A1"), QStringLiteral("WGS 1984, UTM zone 16N"),
               QStringLiteral("EPSG:32616"), kCompassEasting, kCompassNorthing});
    INFO("format: " << format.label);

    cwCavingRegion region;
    cwCave* cave = addAttachedCave(&region, QStringLiteral("Owner Cave"));
    REQUIRE(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);

    const cwAttachedFix fix = withOwnSystem(
        makeAttachedFix(format.station, cave->id(), format.easting, format.northing),
        format.coordinateSystem, format.placementSystem);
    cave->setAttachedFixes({fix});
    REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Anchored; }));

    cwGeoReference* geoReference = region.geoReference();
    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(geoReference->anchor().id == cave->id());
    CHECK(geoReference->localCoordinateSystem() == frameAt(fix));
    CHECK(region.localProjection()->anchorDescription()
          == QStringLiteral("%1 — Owner Cave").arg(format.station));
}

TEST_CASE("An attachment anchor whose first listed fix is bare is named by its own-system fix",
          "[cwLocalProjectionManager][AttachedFrame][cwAnchorDescription]")
{
    // north_cs_out.svx's shape: sanqiao's e1 names no system and nothing is
    // chosen for it, while houping's h1 names UTM48N.
    cwCavingRegion region;
    cwCave* cave = addAttachedCave(&region, QStringLiteral("North"));
    const cwAttachedFix bare =
        makeAttachedFix(QStringLiteral("sanqiao.e1"), cave->id(), kNorthBareEasting,
                        kNorthBareNorthing);
    const cwAttachedFix own = withOwnSystem(
        makeAttachedFix(QStringLiteral("houping.h1"), cave->id(), kNorthOwnEasting,
                        kNorthOwnNorthing),
        QStringLiteral("UTM48N"));
    cave->setAttachedFixes({bare, own});
    REQUIRE(waitUntil([&] { return region.geoReference()->localCoordinateSystem() == frameAt(own); }));

    CHECK(region.geoReference()->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(region.geoReference()->localCoordinateSystem() == frameAt(own));
    CHECK(region.localProjection()->anchorDescription() == QStringLiteral("houping.h1 — North"));
}

TEST_CASE("An attached fix with its own system leaves a native fix's frame alone",
          "[cwLocalProjectionManager][AttachedFrame]")
{
    cwCavingRegion region;
    cwCave* native = addCaveWithFixes(&region, {
        makeFix(QStringLiteral("A1"), kUtm12N, kAnchorEasting, kAnchorNorthing, kElevation)});
    native->setName(QStringLiteral("Native Cave"));
    cwGeoReference* geoReference = region.geoReference();
    REQUIRE(geoReference->anchor().kind == cwGeoReference::Anchor::FixStation);
    const QString before = geoReference->localCoordinateSystem();

    cwCave* attached = addAttachedCave(&region, QStringLiteral("Owner Cave"));
    attached->setAttachedFixes({withOwnSystem(
        makeAttachedFix(QStringLiteral("S1"), attached->id(), kAnchorEasting, kDistantNorthing),
        kUtm12N)});
    REQUIRE(waitForQueuedEvents());

    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::FixStation);
    CHECK(geoReference->localCoordinateSystem() == before);
    CHECK(region.localProjection()->anchorDescription() == QStringLiteral("A1 — Native Cave"));
}

TEST_CASE("A bare attached fix with a chosen system anchors ahead of an own-system one",
          "[cwLocalProjectionManager][AttachedFrame]")
{
    cwCavingRegion region;
    // The own-system owner comes first in the region, so only the order of
    // the two kinds can put the bare fix ahead of it.
    cwCave* ownSystem = addAttachedCave(&region, QStringLiteral("Own System"));
    cwCave* chosen = addAttachedCave(&region, QStringLiteral("Chosen"), kUtm12N);
    const cwAttachedFix own = withOwnSystem(
        makeAttachedFix(QStringLiteral("o1"), ownSystem->id(), kAnchorEasting, kAnchorNorthing),
        kUtm12N);
    const cwAttachedFix bare =
        makeAttachedFix(QStringLiteral("b1"), chosen->id(), kNearbyEasting, kNearbyNorthing);
    // Both fixes arrive in one evaluation, as from one load: order decides
    // only between inputs that appear together.
    region.localProjection()->setLoading(true);
    ownSystem->setAttachedFixes({own});
    chosen->setAttachedFixes({bare});
    region.localProjection()->setLoading(false);

    cwGeoReference* geoReference = region.geoReference();
    REQUIRE(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(geoReference->anchor().id == chosen->id());
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                    kNearbyEasting, kNearbyNorthing);

    SECTION("unsetting the choice hands the anchor to the nearby own-system fix")
    {
        cwExternalCenterline centerline = chosen->externalCenterline();
        centerline.setFixCoordinateSystem(QString());
        chosen->setExternalCenterline(centerline);
        REQUIRE(waitUntil([&] { return region.geoReference()->anchor().id == ownSystem->id(); }));

        CHECK(geoReference->state() == cwGeoReference::Anchored);
        CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
        CHECK(geoReference->anchor().id == ownSystem->id());
        checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                        kAnchorEasting, kAnchorNorthing);
        CHECK(region.localProjection()->anchorDescription() == QStringLiteral("o1 — Own System"));
    }
}

TEST_CASE("An own-system attached fix outside its system's domain never anchors",
          "[cwLocalProjectionManager][AttachedFrame]")
{
    cwCavingRegion region;
    cwCave* typo = addAttachedCave(&region, QStringLiteral("Typo"));
    cwCave* good = addAttachedCave(&region, QStringLiteral("Good"));
    typo->setAttachedFixes({withOwnSystem(
        makeAttachedFix(QStringLiteral("t1"), typo->id(), kOutOfZoneEasting, kAnchorNorthing),
        kUtm12N)});
    REQUIRE(waitForQueuedEvents());
    good->setAttachedFixes({withOwnSystem(
        makeAttachedFix(QStringLiteral("g1"), good->id(), kAnchorEasting, kAnchorNorthing),
        kUtm12N)});
    REQUIRE(waitUntil([&] { return region.geoReference()->anchor().id == good->id(); }));

    cwGeoReference* geoReference = region.geoReference();
    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(geoReference->anchor().id == good->id());
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                    kAnchorEasting, kAnchorNorthing);
}

TEST_CASE("The data's middle counts an attached fix whose file names its own system",
          "[cwLocalProjectionManager][AttachedFrame][cwRecenter]")
{
    cwCavingRegion region;
    addCaveWithFixes(&region, {
        makeFix(QStringLiteral("A1"), kUtm12N, kAnchorEasting, kAnchorNorthing, kElevation)});
    cwLocalProjectionManager* projection = region.localProjection();
    const auto before = projection->dataCenter();
    REQUIRE(before.has_value());

    cwCave* attached = addAttachedCave(&region, QStringLiteral("Owner Cave"));
    attached->setAttachedFixes({withOwnSystem(
        makeAttachedFix(QStringLiteral("S1"), attached->id(), kSpreadEasting, kAnchorNorthing),
        kUtm12N)});

    const auto after = projection->dataCenter();
    REQUIRE(after.has_value());
    constexpr double kMarginMeters = 1.0;
    CHECK_THAT(after->x - before->x,
               WithinAbs((kSpreadEasting - kAnchorEasting) / 2.0, kMarginMeters));
    CHECK_THAT(after->y - before->y, WithinAbs(0.0, kMarginMeters));
}

TEST_CASE("Recentering an attachment-only project on the data's middle stays recentered",
          "[cwLocalProjectionManager][AttachedFrame][cwRecenter]")
{
    cwCavingRegion region;
    cwCave* cave = addAttachedCave(&region, QStringLiteral("Owner Cave"), kUtm12N);
    const cwAttachedFix first =
        makeAttachedFix(QStringLiteral("b1"), cave->id(), kAnchorEasting, kAnchorNorthing);
    const cwAttachedFix second =
        makeAttachedFix(QStringLiteral("b2"), cave->id(), kSpreadEasting, kAnchorNorthing);
    cave->setAttachedFixes({first, second});
    REQUIRE(waitUntil([&] { return region.geoReference()->anchor().kind == cwGeoReference::Anchor::Attachment; }));

    cwGeoReference* geoReference = region.geoReference();
    REQUIRE(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    REQUIRE(region.localProjection()->recenterOnDataCenter());
    REQUIRE(geoReference->state() == cwGeoReference::Frozen);
    const QString recentered = geoReference->localCoordinateSystem();

    // The next scan lists the same fixes again, one of them from a moved line,
    // which re-evaluates the frame.
    cwAttachedFix rescanned = second;
    rescanned.line = second.line + 1;
    cave->setAttachedFixes({first, rescanned});
    REQUIRE(waitForQueuedEvents());

    CHECK(geoReference->state() == cwGeoReference::Frozen);
    CHECK(geoReference->localCoordinateSystem() == recentered);

    SECTION("a native fix turns the attached fixes out of the frozen frame again")
    {
        // Far past the threshold: with the attached fixes still counted, the
        // ones at the origin would keep the frame where it is.
        addCaveWithFixes(&region, {
            makeFix(QStringLiteral("N1"), kUtm12N, kAnchorEasting, kDistantNorthing, kElevation)});

        CHECK(geoReference->state() == cwGeoReference::Frozen);
        checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                        kAnchorEasting, kDistantNorthing);
    }
}

TEST_CASE("A bare attached fix with no system chosen puts a project with no frame on the local grid",
          "[cwLocalProjectionManager][AttachedFrame][LocalGrid]")
{
    cwCavingRegion region;
    cwCave* attached = addAttachedCave(&region, QStringLiteral("Grid Cave"));
    attached->setAttachedFixes({makeAttachedFix(QStringLiteral("g1"), attached->id(),
                                                kAnchorEasting, kAnchorNorthing)});
    REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::LocalGrid; }));

    cwGeoReference* geoReference = region.geoReference();
    CHECK(geoReference->state() == cwGeoReference::LocalGrid);
    CHECK_FALSE(geoReference->hasCoordinateSystem());
    CHECK(geoReference->localCoordinateSystem().isEmpty());
    CHECK_FALSE(geoReference->anchor().isValid());
    CHECK(region.localProjection()->anchorDescription().isEmpty());

    SECTION("a native fix takes the project over, and losing it returns the local grid")
    {
        cwCave* native = addCaveWithFixes(&region, {
            makeFix(QStringLiteral("A1"), kUtm12N, kAnchorEasting, kAnchorNorthing, kElevation)});
        CHECK(geoReference->state() == cwGeoReference::Anchored);
        CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::FixStation);
        checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                        kAnchorEasting, kAnchorNorthing);

        native->fixStations()->removeAt(0);
        REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::LocalGrid; }));
        CHECK(geoReference->state() == cwGeoReference::LocalGrid);
        CHECK(geoReference->localCoordinateSystem().isEmpty());
    }

    SECTION("choosing a system anchors the project on the fix, and unsetting it returns the local grid")
    {
        cwExternalCenterline centerline = attached->externalCenterline();
        centerline.setFixCoordinateSystem(kUtm12N);
        attached->setExternalCenterline(centerline);
        REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Anchored; }));
        CHECK(geoReference->state() == cwGeoReference::Anchored);
        CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);

        centerline.setFixCoordinateSystem(QString());
        attached->setExternalCenterline(centerline);
        REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::LocalGrid; }));
        CHECK(geoReference->state() == cwGeoReference::LocalGrid);
        CHECK(geoReference->localCoordinateSystem().isEmpty());
    }

    SECTION("losing the last bare fix returns the project to ungeoreferenced")
    {
        attached->setAttachedFixes({});
        REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Ungeoreferenced; }));
        CHECK(geoReference->state() == cwGeoReference::Ungeoreferenced);
    }
}

TEST_CASE("A restored local grid is kept while the scan has listed nothing yet",
          "[cwLocalProjectionManager][AttachedFrame][LocalGrid]")
{
    cwCavingRegion region;
    cwLocalProjectionManager* projection = region.localProjection();
    projection->setLoading(true);
    region.geoReference()->restore(cwGeoReference::LocalGrid, QString(),
                                   cwGeoReference::Anchor{}, QString());
    addAttachedCave(&region, QStringLiteral("Grid Cave"));
    projection->setLoading(false);

    CHECK(region.geoReference()->state() == cwGeoReference::LocalGrid);
    CHECK(region.geoReference()->localCoordinateSystem().isEmpty());
}

TEST_CASE("A restored frozen frame is kept when a bare attached fix lists before its inputs",
          "[cwLocalProjectionManager][AttachedFrame][LocalGrid]")
{
    cwCavingRegion region;
    cwLocalProjectionManager* projection = region.localProjection();
    cwCave* cave = addAttachedCave(&region, QStringLiteral("Grid Cave"));
    const cwAttachedFix bare =
        makeAttachedFix(QStringLiteral("g1"), cave->id(), kAnchorEasting, kAnchorNorthing);
    cwAttachedFix located = bare;
    located.readInSystem = kUtm12N;
    const QString stored = frameAt(located);

    projection->setLoading(true);
    region.geoReference()->restore(cwGeoReference::Frozen, stored,
                                   cwGeoReference::Anchor{}, QString());
    projection->setLoading(false);
    cave->setAttachedFixes({bare});
    REQUIRE(waitForQueuedEvents());

    CHECK(region.geoReference()->state() == cwGeoReference::Frozen);
    CHECK(region.geoReference()->localCoordinateSystem() == stored);
}

TEST_CASE("A bare attached fix read in its file's own *cs out gives a project with no frame one",
          "[cwLocalProjectionManager][AttachedFrame][LocalGrid]")
{
    cwCavingRegion region;
    cwCave* cave = addAttachedCave(&region, QStringLiteral("Read In Cave"));
    cwAttachedFix readIn = makeAttachedFix(QStringLiteral("sanqiao.e1"), cave->id(),
                                           kAnchorEasting, kAnchorNorthing);
    readIn.readInSystem = kUtm12N;
    readIn.readInFileOutput = true;
    cave->setAttachedFixes({readIn});
    REQUIRE(waitUntil([&] { return region.geoReference()->state() == cwGeoReference::Anchored; }));

    cwGeoReference* geoReference = region.geoReference();
    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(geoReference->anchor().id == cave->id());
    CHECK(geoReference->localCoordinateSystem() == frameAt(readIn));
    CHECK(region.localProjection()->anchorDescription()
          == QStringLiteral("sanqiao.e1 — Read In Cave"));
}

TEST_CASE("Attached fixes listed on two caves in one turn settle the frame once",
          "[cwLocalProjectionManager][AttachedFrame][CoalescedPass]")
{
    // A scan apply hands each derived record its fixes one after another in
    // one turn: a bare fix that alone would put the project on the local
    // grid, then an own-system fix that anchors it.
    cwCavingRegion region;
    cwCave* bareCave = addAttachedCave(&region, QStringLiteral("Bare Cave"));
    cwCave* ownCave = addAttachedCave(&region, QStringLiteral("Own Cave"));
    REQUIRE(waitForQueuedEvents());
    cwGeoReference* geoReference = region.geoReference();
    REQUIRE(geoReference->state() == cwGeoReference::Ungeoreferenced);

    const cwAttachedFix own = withOwnSystem(
        makeAttachedFix(QStringLiteral("o1"), ownCave->id(), kAnchorEasting, kAnchorNorthing),
        kUtm12N);
    QSignalSpy frameSpy(geoReference, &cwGeoReference::localProjectionChanged);
    bareCave->setAttachedFixes({makeAttachedFix(QStringLiteral("b1"), bareCave->id(),
                                                kNearbyEasting, kNearbyNorthing)});
    ownCave->setAttachedFixes({own});
    REQUIRE(waitUntil([&] { return frameSpy.count() >= 1; }));

    CHECK(frameSpy.count() == 1);
    REQUIRE(waitForQueuedEvents());
    CHECK(frameSpy.count() == 1);
    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::Attachment);
    CHECK(geoReference->anchor().id == ownCave->id());
    CHECK(geoReference->localCoordinateSystem() == frameAt(own));
    CHECK(region.localProjection()->anchorDescription() == QStringLiteral("o1 — Own Cave"));
}

TEST_CASE("A fix typed and corrected in one turn settles the frame once",
          "[cwLocalProjectionManager][CoalescedPass]")
{
    cwCavingRegion region;
    cwCave* cave = addCaveWithFixes(&region, {});
    REQUIRE(waitForQueuedEvents());
    cwGeoReference* geoReference = region.geoReference();
    REQUIRE(geoReference->state() == cwGeoReference::Ungeoreferenced);

    const cwFixStation wrong = makeFix(QStringLiteral("A1"), kUtm12N,
                                       kAnchorEasting, kAnchorNorthing, kElevation);
    cwFixStation corrected = wrong;
    corrected.setCoordinate(kAnchorEasting, kDistantNorthing, kElevation);

    QSignalSpy frameSpy(geoReference, &cwGeoReference::localProjectionChanged);
    cave->fixStations()->setFixStations({wrong});
    cave->fixStations()->setFixStations({corrected});
    REQUIRE(waitUntil([&] { return frameSpy.count() >= 1; }));

    CHECK(frameSpy.count() == 1);
    REQUIRE(waitForQueuedEvents());
    CHECK(frameSpy.count() == 1);
    CHECK(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().id == wrong.id());
    checkCenteredOn(geoReference->localCoordinateSystem(), kUtm12N,
                    kAnchorEasting, kDistantNorthing);
}

