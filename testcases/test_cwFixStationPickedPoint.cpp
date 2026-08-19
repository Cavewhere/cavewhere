/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLocalProjection.h"

#include "GeoreferenceFixtureHelper.h"
#include "LazFixtureHelper.h"

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVector3D>

using Catch::Approx;

namespace {

const QString kWgs84 = QStringLiteral("EPSG:4326");
const QString kNad83 = QStringLiteral("EPSG:6318");

//! What a lidar tile from a US state survey declares: NAD83(2011) / UTM 16N
//! with NAVD88 heights, spelled as PROJ's horizontal+vertical compound form.
const QString kNad83Utm16NWithNavd88 = QStringLiteral("EPSG:6345+5703");

//! Where the frame under test is centered — the conterminous US, so the derived
//! frame is plate-fixed to NAD83(2011) and the default fix datum is not the
//! WGS84 fallback.
constexpr double kFrameLatitude = 37.0;
constexpr double kFrameLongitude = -84.0;

//! The elevation every pick here is taken at, in meters.
constexpr float kPickElevation = 300.0f;

//! How close a coordinate written at 8 decimals has to land to the degree it
//! was picked at — about a millimeter of latitude.
constexpr double kDegreeTolerance = 1e-7;

//! A region with a frozen frame at (kFrameLatitude, kFrameLongitude) and one
//! cave holding one blank fix row. The scene origin is the frame origin, so a
//! pick at (0, 0, z) lands on exactly those degrees.
struct PickFixture {
    cwCavingRegion region;
    cwCave* cave = nullptr;
    cwFixStationModel* fixes = nullptr;
    QString fixId;

    PickFixture()
    {
        const QString frame = cwLocalProjection::derive(kFrameLatitude, kFrameLongitude, QString());
        REQUIRE_FALSE(frame.isEmpty());
        cwGeoreferenceFixture::restoreFrozenFrame(&region, frame);
        REQUIRE_FALSE(region.geoReference()->localCoordinateSystem().isEmpty());

        region.addCave();
        cave = region.cave(0);
        REQUIRE(cave != nullptr);

        fixes = cave->fixStations();
        REQUIRE(fixes->addFixStation(QStringLiteral("A1")) == 0);
        fixId = fixes->fixStationAt(0).id().toString();
    }

    //! The pick the 3D view makes: the same two systems FixStationPickTool
    //! reads off the region, so a test can't answer a question production
    //! doesn't ask.
    bool pick(const QVector3D& scenePoint) const
    {
        return pickFor(fixId, scenePoint);
    }

    //! The same pick aimed at \a id, for the fixes this fixture doesn't hold.
    bool pickFor(const QString& id, const QVector3D& scenePoint) const
    {
        return fixes->setPickedPoint(id,
                                     scenePoint,
                                     region.geoReference()->localCoordinateSystem(),
                                     region.defaultFixDatum());
    }
};

//! Give \a region one scanned tile whose system is read as \a sourceCS.
void addTile(cwCavingRegion* region, const QTemporaryDir& tempDir, const QString& sourceCS)
{
    const QDir gisLayers(QDir(tempDir.path()).filePath(cwLazLayerModel::folderName()));
    REQUIRE(QDir().mkpath(gisLayers.path()));

    region->lazLayers()->setGisLayersDir(gisLayers);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    REQUIRE_FALSE(writeMinimalLaz(gisLayers.filePath(QStringLiteral("tile.laz"))).isEmpty());

    region->lazLayers()->rescan();
    REQUIRE(region->lazLayers()->count() == 1);
    REQUIRE(waitForLazLayerModelSettled(region->lazLayers()));
    region->lazLayers()->layerAt(0)->setSourceCSOverride(sourceCS);
}

} // namespace

TEST_CASE("A pick writes the coordinate and the system it is written on",
          "[cwFixStationPickedPoint]")
{
    PickFixture fixture;
    QSignalSpy dataSpy(fixture.fixes, &QAbstractItemModel::dataChanged);

    REQUIRE(fixture.pick(QVector3D(0.0f, 0.0f, kPickElevation)));

    const cwFixStation fix = fixture.fixes->fixStationAt(0);
    //Nothing but the frame says where this project is, so the pick lands on the
    //frame's own datum rather than on the WGS84 fallback.
    CHECK(fix.inputCS() == kNad83);
    CHECK(fix.state() == cwFixStation::Valid);
    //A geographic system writes latitude first, so the easting the row reads
    //back is the longitude.
    CHECK(fix.northing() == Approx(kFrameLatitude).margin(kDegreeTolerance));
    CHECK(fix.easting() == Approx(kFrameLongitude).margin(kDegreeTolerance));
    CHECK(fix.elevation() == Approx(kPickElevation).margin(1e-6));

    //One edit, not three: the line plot re-solves once for a pick.
    CHECK(dataSpy.size() == 1);
}

TEST_CASE("A pick over a scanned tile lands on the tile's datum",
          "[cwFixStationPickedPoint]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    PickFixture fixture;
    addTile(&fixture.region, tempDir, kNad83Utm16NWithNavd88);
    REQUIRE(fixture.region.lazLayers()->layerAt(0)->enabled());

    REQUIRE(fixture.pick(QVector3D(0.0f, 0.0f, kPickElevation)));

    const cwFixStation fix = fixture.fixes->fixStationAt(0);
    //The tile's datum — a compound system names its horizontal half's, which is
    //what the fix is being placed against. Where the pick lands is a question
    //for the fixture above: a scanned tile moves the frame onto its own data,
    //so the scene origin is no longer the degrees the fixture froze.
    CHECK(fix.inputCS() == kNad83);
    CHECK(fix.state() == cwFixStation::Valid);
}

TEST_CASE("A pick lands on the row the fix's id names, not on its old index",
          "[cwFixStationPickedPoint]")
{
    PickFixture fixture;

    //The user goes back to the page and adds a row above the one they asked to
    //place — which a stored index would silently follow.
    REQUIRE(fixture.fixes->addFixStation(QStringLiteral("B2")) == 1);
    fixture.fixes->setFixStations({fixture.fixes->fixStationAt(1),
                                   fixture.fixes->fixStationAt(0)});

    REQUIRE(fixture.pick(QVector3D(0.0f, 0.0f, kPickElevation)));

    CHECK(fixture.fixes->fixStationAt(0).state() == cwFixStation::Empty);
    CHECK(fixture.fixes->fixStationAt(1).stationName() == QStringLiteral("A1"));
    CHECK(fixture.fixes->fixStationAt(1).state() == cwFixStation::Valid);
}

TEST_CASE("A pick for a fix that isn't there writes nothing",
          "[cwFixStationPickedPoint]")
{
    PickFixture fixture;
    QSignalSpy dataSpy(fixture.fixes, &QAbstractItemModel::dataChanged);

    CHECK_FALSE(fixture.pickFor(QUuid::createUuid().toString(),
                                QVector3D(0.0f, 0.0f, kPickElevation)));
    CHECK_FALSE(fixture.pickFor(QString(), QVector3D(0.0f, 0.0f, kPickElevation)));

    CHECK(fixture.fixes->fixStationAt(0).state() == cwFixStation::Empty);
    CHECK(dataSpy.size() == 0);
}

TEST_CASE("A pick with no frame writes nothing", "[cwFixStationPickedPoint]")
{
    cwCavingRegion region;
    region.addCave();
    cwCave* cave = region.cave(0);
    REQUIRE(cave->fixStations()->addFixStation(QStringLiteral("A1")) == 0);
    const QString fixId = cave->fixStations()->fixStationAt(0).id().toString();

    REQUIRE(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);
    //Nothing has said where the project is, so the datum is the fallback and
    //there is no system the scene point can be transformed out of.
    CHECK(region.defaultFixDatum() == kWgs84);
    CHECK(region.defaultFixSourceCS().isEmpty());

    CHECK_FALSE(cave->fixStations()->setPickedPoint(fixId,
                                                    QVector3D(0.0f, 0.0f, kPickElevation),
                                                    region.geoReference()->localCoordinateSystem(),
                                                    region.defaultFixDatum()));
    CHECK(cave->fixStations()->fixStationAt(0).state() == cwFixStation::Empty);
}
