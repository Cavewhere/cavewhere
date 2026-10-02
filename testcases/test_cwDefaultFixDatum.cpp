/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
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

namespace {

const QString kWgs84 = QStringLiteral("EPSG:4326");
const QString kNad83 = QStringLiteral("EPSG:6318");
const QString kEtrs89 = QStringLiteral("EPSG:4258");

//! Systems a tile can declare, each on a datum the table names and each on a
//! different one, so the code the ladder answers with says which rung it came
//! from.
const QString kWgs84Utm16N = QStringLiteral("EPSG:32616");
const QString kNad83Utm16N = QStringLiteral("EPSG:6345");
const QString kEtrs89Utm32N = QStringLiteral("EPSG:25832");

//! A point in the conterminous US, where a derived frame is plate-fixed to
//! NAD83(2011) — so a frame built here answers with a datum that is not the
//! WGS84 fallback.
constexpr double kUsLatitude = 37.0;
constexpr double kUsLongitude = -84.0;

//! Give \a region \a count tiles in a scanned folder under \a tempDir, none of
//! which declares a system of its own. The tag orders the directory listing,
//! and so the model's rows. The points never leave the file: every test here
//! overrides the system its tile is read as, because the datum is what is under
//! test and not the geometry.
void addTiles(cwCavingRegion* region, const QTemporaryDir& tempDir, int count)
{
    const QDir gisLayers(QDir(tempDir.path()).filePath(cwLazLayerModel::folderName()));
    REQUIRE(QDir().mkpath(gisLayers.path()));

    region->lazLayers()->setGisLayersDir(gisLayers);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    for (int tile = 0; tile < count; ++tile) {
        const QString path = gisLayers.filePath(QStringLiteral("tile-%1.laz").arg(tile));
        REQUIRE_FALSE(writeMinimalLaz(path).isEmpty());
    }

    region->lazLayers()->rescan();
    REQUIRE(region->lazLayers()->count() == count);
    REQUIRE(waitForLazLayerModelSettled(region->lazLayers()));
}

//! A frame the project could have derived in the conterminous US, which the
//! region is walked back through the load path to reach.
void freezeFrameOnNad83(cwCavingRegion* region)
{
    const QString frame = cwLocalProjection::derive(kUsLatitude, kUsLongitude, QString());
    REQUIRE_FALSE(frame.isEmpty());
    cwGeoreferenceFixture::restoreFrozenFrame(region, frame);
    REQUIRE_FALSE(region->geoReference()->localCoordinateSystem().isEmpty());
}

} // namespace

TEST_CASE("An enabled lidar layer's datum is the default fix datum",
          "[cwDefaultFixDatum]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwCavingRegion region;
    addTiles(&region, tempDir, 1);

    // A frame on one datum and a tile on another: a fix placed here is being
    // placed against the terrain, so the tile wins.
    freezeFrameOnNad83(&region);
    region.lazLayers()->layerAt(0)->setSourceCSOverride(kEtrs89Utm32N);

    REQUIRE(cwCoordinateTransform::geographicDatumFor(
                region.geoReference()->localCoordinateSystem()) == kNad83);
    CHECK(region.defaultFixDatum() == kEtrs89);
}

TEST_CASE("The frame's datum is the default when no enabled layer names one",
          "[cwDefaultFixDatum]")
{
    cwCavingRegion region;
    freezeFrameOnNad83(&region);

    CHECK(region.lazLayers()->count() == 0);
    CHECK(region.defaultFixDatum() == kNad83);
}

TEST_CASE("A layer whose system names no table datum defers to the frame",
          "[cwDefaultFixDatum]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwCavingRegion region;
    addTiles(&region, tempDir, 1);
    freezeFrameOnNad83(&region);

    // NAD83 (1986) is a real datum the table doesn't name, and a tile carrying
    // one is a tile the ladder has to step past rather than stop on.
    region.lazLayers()->layerAt(0)->setSourceCSOverride(QStringLiteral("EPSG:26916"));

    CHECK(region.defaultFixDatum() == kNad83);
}

TEST_CASE("WGS84 is the default with neither a layer nor a frame",
          "[cwDefaultFixDatum]")
{
    cwCavingRegion region;

    REQUIRE(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);
    CHECK(region.defaultFixDatum() == kWgs84);
}

TEST_CASE("A disabled layer is skipped when defaulting the fix datum",
          "[cwDefaultFixDatum]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwCavingRegion region;
    addTiles(&region, tempDir, 2);

    // Three datums, one per rung, so each answer names where it came from.
    cwGeoreferenceFixture::restoreFrozenFrame(&region, kEtrs89Utm32N);
    cwLazLayer* first = region.lazLayers()->layerAt(0);
    cwLazLayer* second = region.lazLayers()->layerAt(1);
    first->setSourceCSOverride(kNad83Utm16N);
    second->setSourceCSOverride(kWgs84Utm16N);

    // Both rows name a datum, so row order alone decides — until the first one
    // stops drawing, at which point it stops being what a fix is placed against.
    CHECK(region.defaultFixDatum() == kNad83);

    first->setEnabled(false);
    CHECK(region.defaultFixDatum() == kWgs84);

    second->setEnabled(false);
    CHECK(region.defaultFixDatum() == kEtrs89);
}

TEST_CASE("The default fix datum reports when what it reads changes",
          "[cwDefaultFixDatum]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwCavingRegion region;
    QSignalSpy spy(&region, &cwCavingRegion::defaultFixDatumChanged);

    addTiles(&region, tempDir, 1);
    CHECK(spy.count() > 0);

    spy.clear();
    region.lazLayers()->layerAt(0)->setSourceCSOverride(kNad83Utm16N);
    CHECK(spy.count() > 0);

    spy.clear();
    region.lazLayers()->layerAt(0)->setEnabled(false);
    CHECK(spy.count() > 0);

    spy.clear();
    freezeFrameOnNad83(&region);
    CHECK(spy.count() > 0);
}
