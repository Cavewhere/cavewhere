/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwLocalProjection.h"
#include "cwLocalProjectionManager.h"
#include "cwRecenterCandidateModel.h"
#include "FixStationFixtureHelper.h"
#include "GeoreferenceFixtureHelper.h"

//Qt includes
#include <QSignalSpy>
#include <QUuid>

using Catch::Matchers::ContainsSubstring;
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
    CHECK(geoReference->localCoordinateSystem() == picked);

    cave->fixStations()->removeAt(1);

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

    CHECK_FALSE(region.localProjection()->isCenteredOnDataCenter());
    CHECK_FALSE(openedCandidates(&region)->dataCenterIsCurrent());
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
    cwCavingRegion region;
    const cwFixStation onWgs84 = makeFix(QStringLiteral("A1"),
                                         QStringLiteral("EPSG:32616"),
                                         kAnchorEasting, kAnchorNorthing, kElevation);
    const cwFixStation onNad83 = makeFix(QStringLiteral("B1"),
                                         QStringLiteral("EPSG:26916"),
                                         kAnchorEasting + 2000.0, kAnchorNorthing,
                                         kElevation);
    addCaveWithFixes(&region, {onWgs84, onNad83});

    auto* geoReference = region.geoReference();
    REQUIRE(geoReference->anchor().id == onWgs84.id());
    REQUIRE_THAT(geoReference->datumName().toStdString(),
                 ContainsSubstring("World Geodetic System 1984"));

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

TEST_CASE("The manager names what the frame is centered on",
          "[cwLocalProjectionManager][cwAnchorDescription]")
{
    cwCavingRegion region;
    CHECK(region.geoReference()->anchorDescription().isEmpty());

    const cwFixStation fix = makeFix(QStringLiteral("A42"), kUtm12N,
                                     kAnchorEasting, kAnchorNorthing, kElevation);
    cwCave* cave = addCaveWithFixes(&region, {fix});
    cave->setName(QStringLiteral("Roppel Cave"));

    REQUIRE(region.geoReference()->state() == cwGeoReference::Anchored);
    CHECK(region.geoReference()->anchorDescription() == QStringLiteral("A42 — Roppel Cave"));

    SECTION("renaming the cave moves the description") {
        QSignalSpy spy(region.geoReference(), &cwGeoReference::anchorDescriptionChanged);

        cave->setName(QStringLiteral("Hidden River Cave"));

        CHECK(spy.count() == 1);
        CHECK(region.geoReference()->anchorDescription() == QStringLiteral("A42 — Hidden River Cave"));
    }

    SECTION("renaming the station moves the description") {
        QSignalSpy spy(region.geoReference(), &cwGeoReference::anchorDescriptionChanged);

        cwFixStation renamed = fix;
        renamed.setStationName(QStringLiteral("A43"));
        cave->fixStations()->setFixStations({renamed});

        CHECK(spy.count() == 1);
        CHECK(region.geoReference()->anchorDescription() == QStringLiteral("A43 — Roppel Cave"));
    }

    SECTION("a frame with no anchor left names nothing") {
        QSignalSpy spy(region.geoReference(), &cwGeoReference::anchorDescriptionChanged);

        // The anchor was the only georeferenced input, so deleting it takes the
        // frame with it — and there is nothing left the description could name.
        cave->fixStations()->setFixStations({});

        REQUIRE(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);
        CHECK(spy.count() >= 1);
        CHECK(region.geoReference()->anchorDescription().isEmpty());
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
    REQUIRE(geoReference->anchorDescription() == QStringLiteral("A1 — Roppel Cave"));

    SECTION("a picked station is the one named") {
        QSignalSpy spy(geoReference, &cwGeoReference::anchorDescriptionChanged);

        REQUIRE(region.localProjection()->recenterOnStation(surveyed.id()));

        CHECK(spy.count() == 1);
        CHECK(geoReference->anchorDescription() == QStringLiteral("B1 — Roppel Cave"));
    }

    SECTION("the data's middle is no station, so it names nothing") {
        QSignalSpy spy(geoReference, &cwGeoReference::anchorDescriptionChanged);

        REQUIRE(region.localProjection()->recenterOnDataCenter());

        REQUIRE(geoReference->state() == cwGeoReference::Frozen);
        CHECK(spy.count() == 1);
        CHECK(geoReference->anchorDescription().isEmpty());
    }

    SECTION("a refused recentering leaves the description alone") {
        QSignalSpy spy(geoReference, &cwGeoReference::anchorDescriptionChanged);

        CHECK_FALSE(region.localProjection()->recenterOnStation(QUuid::createUuid()));

        CHECK(spy.count() == 0);
        CHECK(geoReference->anchorDescription() == QStringLiteral("A1 — Roppel Cave"));
    }
}
