/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFarFromOriginNotice.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwLinePlotManager.h"
#include "cwLocalProjectionManager.h"
#include "cwProject.h"
#include "cwStationPositionLookup.h"
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QSignalSpy>
#include <QVector3D>

// Std
#include <cmath>
#include <memory>

namespace {

const QString kBareFixFixture = QStringLiteral("survex_bare_fix/bare_fix.svx");
const QString kBareFixFileName = QStringLiteral("bare_fix.svx");
const QString kUtm48NFrame = QStringLiteral("EPSG:32648");
const QString kUtm48N = QStringLiteral("UTM48N");

// bare_fix.svx: *fix A 500000 3000000 100, then A-B 10 m east and B-C 10 m
// north, so its solved stations span 10 m on each horizontal axis.
constexpr double kFixEasting = 500000.0;
constexpr double kFixNorthing = 3000000.0;
constexpr double kFixElevation = 100.0;
constexpr double kLegMeters = 10.0;
const double kBareFixCenterDistance =
    std::hypot(kFixEasting + kLegMeters / 2.0, kFixNorthing + kLegMeters / 2.0);

// Solved positions are float32: at three million meters one step is a
// quarter meter, so a meter covers the rounding.
constexpr double kDistanceMarginMeters = 1.0;

// A survey whose framed center sits this close to the origin is in plain view.
constexpr double kNearOriginMeters = 100.0;

// A lopsided survey: one station at the origin and three stacked 1000 m east
// and 5000 m down. Its bounding box's horizontal center is 500 m out; the
// stations' mean is 750 m out and the box's 3D center is farther still.
constexpr double kLopsidedEastMeters = 1000.0;
constexpr double kLopsidedDepthMeters = -5000.0;
constexpr double kLopsidedCenterDistance = kLopsidedEastMeters / 2.0;

// A native fix in the project's frame 100 m east of the file's. Alone it is
// the data center; with the file's fix counted, the median of the two sits
// halfway back toward the file's.
const QString kMainFixStation = QStringLiteral("m1");
constexpr double kMainFixEasting = 500100.0;
constexpr double kTwoFixMedianShift = (kFixEasting - kMainFixEasting) / 2.0;
constexpr double kDataCenterMarginMeters = 1.0;

//! bare_fix.svx's three stations where a project with no frame solves them.
cwStationPositionLookup bareFixPositions()
{
    cwStationPositionLookup positions;
    positions.setPosition(QStringLiteral("bare.a"),
                          QVector3D(kFixEasting, kFixNorthing, kFixElevation));
    positions.setPosition(QStringLiteral("bare.b"),
                          QVector3D(kFixEasting + kLegMeters, kFixNorthing, kFixElevation));
    positions.setPosition(QStringLiteral("bare.c"),
                          QVector3D(kFixEasting + kLegMeters, kFixNorthing + kLegMeters,
                                    kFixElevation));
    return positions;
}

cwStationPositionLookup lopsidedPositions()
{
    cwStationPositionLookup positions;
    positions.setPosition(QStringLiteral("a"), QVector3D(0.0, 0.0, 0.0));
    positions.setPosition(QStringLiteral("b"),
                          QVector3D(kLopsidedEastMeters, 0.0, kLopsidedDepthMeters));
    positions.setPosition(QStringLiteral("c"),
                          QVector3D(kLopsidedEastMeters, 0.0, kLopsidedDepthMeters));
    positions.setPosition(QStringLiteral("d"),
                          QVector3D(kLopsidedEastMeters, 0.0, kLopsidedDepthMeters));
    return positions;
}

cwLinePlotManager* linePlotOf(SavedProjectFixture* fixture)
{
    return fixture->rootData->linePlotManager();
}

bool waitForDriverIncludingFixture(SavedProjectFixture* fixture)
{
    const bool included = tryWait(kAttachWaitMs, [fixture]() {
        return linePlotOf(fixture)->driverSource().contains(kBareFixFileName);
    });
    drainPipelines(fixture);
    return included;
}

} // namespace

TEST_CASE("A region with no solved stations sits at the origin and shows no notice",
          "[FarFromOrigin]")
{
    cwCavingRegion region;
    const cwFarFromOriginNotice* notice = region.farFromOriginNotice();

    CHECK(notice->distanceFromOrigin() == Catch::Approx(0.0));
    CHECK_FALSE(notice->isShown());
}

TEST_CASE("The distance from the origin is the horizontal distance of the solved bounding box's center",
          "[FarFromOrigin]")
{
    cwCavingRegion region;
    const cwFarFromOriginNotice* notice = region.farFromOriginNotice();

    region.setStationPositions(lopsidedPositions());
    CHECK(notice->distanceFromOrigin()
          == Catch::Approx(kLopsidedCenterDistance).margin(kDistanceMarginMeters));

    region.setStationPositions(bareFixPositions());
    CHECK(notice->distanceFromOrigin()
          == Catch::Approx(kBareFixCenterDistance).margin(kDistanceMarginMeters));

    region.setStationPositions(cwStationPositionLookup());
    CHECK(notice->distanceFromOrigin() == Catch::Approx(0.0));
}

TEST_CASE("A survey far from the origin in a region with no frame shows the notice until a frame is set",
          "[FarFromOrigin]")
{
    cwCavingRegion region;
    const cwFarFromOriginNotice* notice = region.farFromOriginNotice();
    REQUIRE_FALSE(region.geoReference()->hasCoordinateSystem());

    QSignalSpy shownSpy(notice, &cwFarFromOriginNotice::shownChanged);

    region.setStationPositions(bareFixPositions());
    CHECK(notice->isShown());
    CHECK(shownSpy.count() == 1);

    // The frame alone clears it; the positions have not moved yet.
    region.geoReference()->restore(cwGeoReference::Frozen, kUtm48NFrame, {}, QString());
    CHECK_FALSE(notice->isShown());
    CHECK(shownSpy.count() == 2);

    region.geoReference()->clear();
    CHECK(notice->isShown());
}

TEST_CASE("A survey near the origin in a region with no frame shows no notice",
          "[FarFromOrigin]")
{
    cwCavingRegion region;
    const cwFarFromOriginNotice* notice = region.farFromOriginNotice();

    region.setStationPositions(lopsidedPositions());
    CHECK(notice->distanceFromOrigin() > 0.0);
    CHECK_FALSE(notice->isShown());
}

TEST_CASE("A native survey with no fix sits at the origin and shows no notice",
          "[FarFromOrigin]")
{
    auto fixture = makeSavedProject(QStringLiteral("far-from-origin-native"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    addNativeTripWithShot(fixture->cave, QStringLiteral("Survey"), QStringLiteral("a1"),
                          QStringLiteral("a2"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    REQUIRE(tryWait(kAttachWaitMs, [region]() { return !region->stationPositions().isEmpty(); }));
    drainPipelines(fixture.get());

    const cwFarFromOriginNotice* notice = region->farFromOriginNotice();
    CHECK(notice->distanceFromOrigin() < kNearOriginMeters);
    CHECK_FALSE(notice->isShown());
}

TEST_CASE("An attached bare fix in a project with no frame sits at the origin and shows no notice",
          "[FarFromOrigin][BareFixCS][BareFixOrigin]")
{
    auto fixture = makeSavedProject(QStringLiteral("far-from-origin-bare-fix"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    cwGeoReference* geoReference = region->geoReference();
    REQUIRE_FALSE(geoReference->hasCoordinateSystem());
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    // The file is a local grid placed with its fix at the origin, so the
    // survey sits in plain view though nothing locates it.
    CHECK(tryWait(kAttachWaitMs, [geoReference]() {
        return geoReference->state() == cwGeoReference::LocalGrid;
    }));
    CHECK(tryWait(kAttachWaitMs, [root]() {
        return root->scopedPositions().hasPosition(QStringLiteral("bare.A"))
               && std::abs(root->scopedPositions().position(QStringLiteral("bare.A")).x())
                      < kNearOriginMeters;
    }));
    drainPipelines(fixture.get());

    const cwFarFromOriginNotice* notice = region->farFromOriginNotice();
    CHECK(notice->distanceFromOrigin() < kNearOriginMeters);
    CHECK_FALSE(notice->isShown());
}

TEST_CASE("The data center counts an attached bare fix whose system is chosen in a frozen project",
          "[FarFromOrigin][cwLocalProjectionManager][BareFixCS]")
{
    auto fixture = makeSavedProject(QStringLiteral("far-from-origin-data-center"),
                                    QStringLiteral("Main"), QStringLiteral("NativeTrip"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    region->geoReference()->restore(cwGeoReference::Frozen, kUtm48NFrame, {}, QString());

    cwCave* main = fixture->cave;
    addNativeTripWithShot(main, QStringLiteral("Main survey"), kMainFixStation,
                          QStringLiteral("m2"));
    cwFixStation fix;
    fix.setStationName(kMainFixStation);
    fix.setInputCS(kUtm48NFrame);
    fix.setCoordinate(kMainFixEasting, kFixNorthing, kFixElevation);
    main->fixStations()->appendFixStation(fix);

    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    REQUIRE(waitForDriverIncludingFixture(fixture.get()));

    cwLocalProjectionManager* projection = region->localProjection();
    const auto before = projection->dataCenter();
    REQUIRE(before.has_value());

    chooseFixSystem(root, kUtm48N);
    drainPipelines(fixture.get());
    REQUIRE(region->geoReference()->state() == cwGeoReference::Frozen);
    REQUIRE(subtreeAttachedFixFor(root, QStringLiteral("bare.A")).readInSystem == kUtm48N);

    const auto after = projection->dataCenter();
    REQUIRE(after.has_value());
    CHECK(after->x - before->x
          == Catch::Approx(kTwoFixMedianShift).margin(kDataCenterMarginMeters));
    CHECK(after->y - before->y == Catch::Approx(0.0).margin(kDataCenterMarginMeters));
}
