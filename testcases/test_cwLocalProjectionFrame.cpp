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

#include "FixStationFixtureHelper.h"
#include "LazFixtureHelper.h"

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVector>
#include <QVector3D>

//Std includes
#include <cmath>

namespace {

//! Three UTM zones, so that a frame derived from the wrong one is a frame
//! nothing else could have produced.
const QString kUtm10N = utmZoneWkt(10, -123);
const QString kUtm11N = utmZoneWkt(11, -117);
const QString kUtm12N = utmZoneWkt(12, -111);

//! A cloud of three points centered on \a easting, \a northing — small enough
//! that the frame derived from it is centered on the cloud itself.
QVector<QVector3D> cloudAt(double easting, double northing)
{
    return {
        {float(easting - 10.0), float(northing - 10.0), 100.0f},
        {float(easting),        float(northing),        110.0f},
        {float(easting + 10.0), float(northing + 10.0), 120.0f}
    };
}

QDir makeGisLayersDir(const QTemporaryDir& tempDir)
{
    const QString path = QDir(tempDir.path()).filePath(cwLazLayerModel::folderName());
    QDir().mkpath(path);
    return QDir(path);
}

//! Write a cloud into the scanned folder under \a tag, which is also what the
//! directory listing sorts on — so the tag decides the model's row order.
void writeTile(const QDir& dir, const QString& tag,
               const QVector<QVector3D>& points, const QString& wktCS = QString())
{
    const QString path = dir.filePath(QStringLiteral("%1.laz").arg(tag));
    REQUIRE(writeSyntheticLazFile(path, points, wktCS));
}

//! Spin until every header has been read and every load that the settled frame
//! released has finished.
bool waitForFrame(cwCavingRegion* region)
{
    return waitForLazLayerModelSettled(region->lazLayers());
}

//! Bind the region's layer model to \a dir while the folder is still empty. The
//! rescan setGisLayersDir queues runs here and finds nothing, so the read the
//! test cares about is the one it asks for.
void bindEmptyFolder(cwCavingRegion* region, const QDir& dir)
{
    region->lazLayers()->setGisLayersDir(dir);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    REQUIRE(region->lazLayers()->count() == 0);
}

//! Read the folder without letting the event loop run: on return every header
//! read has been asked for and none of them can have been delivered, which is
//! the middle of an epoch.
void readFolder(cwCavingRegion* region)
{
    region->lazLayers()->rescan();
}

//! How far \a layer's loaded points sit from the frame's origin. A cloud that
//! decoded before the frame existed carries its raw source coordinates, which
//! for a UTM tile is hundreds of kilometers out.
double distanceFromOrigin(const cwLazLayer* layer)
{
    const QVector3D center = (layer->bboxMin() + layer->bboxMax()) * 0.5f;
    return std::hypot(double(center.x()), double(center.y()));
}

constexpr double kNearOriginMeters = 5000.0;

} // namespace

TEST_CASE("The frame is derived from every header, not from the first one back",
          "[cwLocalProjectionFrame]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir gisLayers = makeGisLayersDir(tempDir);

    cwCavingRegion region;
    bindEmptyFolder(&region, gisLayers);
    auto* geoReference = region.geoReference();

    // Three tiles a long way apart, each in its own zone. Whichever one the
    // anchor lands on is written into the frame, so a race between the header
    // reads would be a race between three different stored projections.
    writeTile(gisLayers, QStringLiteral("a-tile"), cloudAt(500000.0, 4194000.0), kUtm10N);
    writeTile(gisLayers, QStringLiteral("b-tile"), cloudAt(400000.0, 3800000.0), kUtm11N);
    writeTile(gisLayers, QStringLiteral("c-tile"), cloudAt(600000.0, 4400000.0), kUtm12N);

    // What the frame was decided on, captured at the moment it was decided. The
    // guarantee is not that the layers eventually all report a header — it is
    // that none of them was still reading one when the anchor was chosen.
    bool everyHeaderRead = false;
    bool anchoredOnce = false;
    QObject::connect(geoReference, &cwGeoReference::localProjectionChanged,
                     &region, [&]() {
        if (anchoredOnce) {
            return;
        }
        anchoredOnce = true;
        cwLazLayerModel* layers = region.lazLayers();
        everyHeaderRead = layers->count() == 3;
        for (int row = 0; row < layers->count(); ++row) {
            everyHeaderRead = everyHeaderRead && layers->layerAt(row)->hasReadHeader();
        }
    });

    readFolder(&region);
    REQUIRE(region.lazLayers()->count() == 3);
    REQUIRE(waitForFrame(&region));

    CHECK(everyHeaderRead);
    REQUIRE(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().id == region.lazLayers()->layerAt(0)->id());
    CHECK(geoReference->localCoordinateSystem()
          == cwLocalProjection::deriveFrom(kUtm10N, cwGeoPoint(500000.0, 4194000.0, 110.0)));
}

TEST_CASE("Reading the same folder twice derives the same frame",
          "[cwLocalProjectionFrame]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir gisLayers = makeGisLayersDir(tempDir);

    writeTile(gisLayers, QStringLiteral("a-tile"), cloudAt(500000.0, 4194000.0), kUtm10N);
    writeTile(gisLayers, QStringLiteral("b-tile"), cloudAt(400000.0, 3800000.0), kUtm11N);
    writeTile(gisLayers, QStringLiteral("c-tile"), cloudAt(600000.0, 4400000.0), kUtm12N);

    // The frame is stored in the project file, so two people importing the same
    // directory have to get the same string out of it.
    QStringList frames;
    for (int run = 0; run < 2; ++run) {
        cwCavingRegion region;
        region.lazLayers()->setGisLayersDir(gisLayers);
        readFolder(&region);
        REQUIRE(waitForFrame(&region));
        frames.append(region.geoReference()->localCoordinateSystem());
    }

    REQUIRE_FALSE(frames.at(0).isEmpty());
    CHECK(frames.at(0) == frames.at(1));
}

TEST_CASE("The first tile that can be placed anchors, and the rest wait for it",
          "[cwLocalProjectionFrame]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir gisLayers = makeGisLayersDir(tempDir);

    cwCavingRegion region;
    bindEmptyFolder(&region, gisLayers);

    // Row 0 says nothing about where it is, so the anchor belongs to row 1 —
    // and row 0 still has to load into the frame row 1 supplies.
    writeTile(gisLayers, QStringLiteral("a-tile"), cloudAt(500000.0, 4194000.0));
    writeTile(gisLayers, QStringLiteral("b-tile"), cloudAt(500100.0, 4194100.0), kUtm10N);
    writeTile(gisLayers, QStringLiteral("c-tile"), cloudAt(500200.0, 4194200.0), kUtm10N);

    readFolder(&region);
    REQUIRE(region.lazLayers()->count() == 3);
    REQUIRE(waitForFrame(&region));

    auto* geoReference = region.geoReference();
    REQUIRE(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().id == region.lazLayers()->layerAt(1)->id());

    // Every tile that knows where it is decoded into the frame rather than into
    // an empty one: in the frame these clouds are a few hundred meters from the
    // origin, and in raw UTM they are four thousand kilometers from it.
    for (int row = 1; row < 3; ++row) {
        auto* layer = region.lazLayers()->layerAt(row);
        REQUIRE(layer->loadStatus() == cwLazLayer::LoadStatus::Loaded);
        CHECK(distanceFromOrigin(layer) < kNearOriginMeters);
    }
}

TEST_CASE("A folder of tiles with no coordinate system still loads",
          "[cwLocalProjectionFrame]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir gisLayers = makeGisLayersDir(tempDir);

    cwCavingRegion region;
    bindEmptyFolder(&region, gisLayers);

    // Nothing here can place anything, which is a settled answer and not an
    // unanswered one: the clouds load untransformed, in their own units.
    writeTile(gisLayers, QStringLiteral("a-tile"), cloudAt(10.0, 20.0));
    writeTile(gisLayers, QStringLiteral("b-tile"), cloudAt(30.0, 40.0));
    writeTile(gisLayers, QStringLiteral("c-tile"), cloudAt(50.0, 60.0));

    readFolder(&region);
    REQUIRE(region.lazLayers()->count() == 3);
    REQUIRE(waitForFrame(&region));

    CHECK(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);
    CHECK(region.geoReference()->localCoordinateSystem().isEmpty());
    for (int row = 0; row < 3; ++row) {
        auto* layer = region.lazLayers()->layerAt(row);
        CHECK(layer->loadStatus() == cwLazLayer::LoadStatus::Loaded);
        CHECK(layer->pointCount() == 3);
    }
    CHECK(region.lazLayers()->layerAt(0)->bboxMin().x() == Catch::Approx(0.0).margin(0.01));
}

TEST_CASE("A tile added to an anchored project loads into the frame it finds",
          "[cwLocalProjectionFrame]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir gisLayers = makeGisLayersDir(tempDir);

    cwCavingRegion region;
    bindEmptyFolder(&region, gisLayers);
    addCaveWithFixes(&region, {makeFix(QStringLiteral("A1"), kUtm10N,
                                       500000.0, 4194000.0, 110.0)});

    auto* geoReference = region.geoReference();
    REQUIRE(geoReference->state() == cwGeoReference::Anchored);
    const QString frame = geoReference->localCoordinateSystem();
    REQUIRE_FALSE(frame.isEmpty());

    QSignalSpy frameSpy(geoReference, &cwGeoReference::localProjectionChanged);

    writeTile(gisLayers, QStringLiteral("a-tile"), cloudAt(500100.0, 4194100.0), kUtm10N);
    readFolder(&region);
    REQUIRE(waitForFrame(&region));

    // There was nothing to wait for — the frame was settled before the tile
    // arrived — and nothing the tile said could move it.
    CHECK(frameSpy.size() == 0);
    CHECK(geoReference->localCoordinateSystem() == frame);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::FixStation);

    auto* layer = region.lazLayers()->layerAt(0);
    REQUIRE(layer->loadStatus() == cwLazLayer::LoadStatus::Loaded);
    CHECK(distanceFromOrigin(layer) < kNearOriginMeters);
}

TEST_CASE("A fix station added while the headers are still arriving takes the frame",
          "[cwLocalProjectionFrame]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir gisLayers = makeGisLayersDir(tempDir);

    cwCavingRegion region;
    bindEmptyFolder(&region, gisLayers);

    writeTile(gisLayers, QStringLiteral("a-tile"), cloudAt(500000.0, 4194000.0), kUtm10N);
    writeTile(gisLayers, QStringLiteral("b-tile"), cloudAt(500100.0, 4194100.0), kUtm10N);

    readFolder(&region);

    // The reads were asked for and nothing has run the event loop since, so
    // they are all still in flight: this is the middle of an epoch.
    REQUIRE(region.lazLayers()->anyHeaderProbeInFlight());
    REQUIRE(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);

    addCaveWithFixes(&region, {makeFix(QStringLiteral("A1"), kUtm10N,
                                       500500.0, 4194500.0, 110.0)});

    // A user's own input decides the anchor immediately. It is the same answer
    // waiting would have given — fix stations come ahead of layers — so making
    // it wait would only delay every decode behind it.
    auto* geoReference = region.geoReference();
    REQUIRE(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::FixStation);
    const QString frame = geoReference->localCoordinateSystem();
    CHECK(frame == cwLocalProjection::deriveFrom(
                       kUtm10N, cwGeoPoint(500500.0, 4194500.0, 110.0)));

    REQUIRE(waitForFrame(&region));
    CHECK(geoReference->localCoordinateSystem() == frame);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::FixStation);
    for (int row = 0; row < 2; ++row) {
        CHECK(distanceFromOrigin(region.lazLayers()->layerAt(row)) < kNearOriginMeters);
    }
}

TEST_CASE("A frame that moves takes its clouds with it", "[cwLocalProjectionFrame]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir gisLayers = makeGisLayersDir(tempDir);

    cwCavingRegion region;
    bindEmptyFolder(&region, gisLayers);

    const cwFixStation fix = makeFix(QStringLiteral("A1"), kUtm10N,
                                     500000.0, 4194000.0, 110.0);
    cwCave* cave = addCaveWithFixes(&region, {fix});

    writeTile(gisLayers, QStringLiteral("a-tile"), cloudAt(500100.0, 4194100.0), kUtm10N);
    readFolder(&region);
    REQUIRE(waitForFrame(&region));

    auto* layer = region.lazLayers()->layerAt(0);
    REQUIRE(layer->loadStatus() == cwLazLayer::LoadStatus::Loaded);
    REQUIRE(distanceFromOrigin(layer) < kNearOriginMeters);

    // The wrong entrance typed first, corrected by 100 km north — far enough
    // that the origin moves. The points in memory are in the frame they were
    // decoded into, so they are only in the right place if they decode again.
    cwFixStation corrected = fix;
    corrected.setCoordinate(500000.0, 4294000.0, 110.0);
    cave->fixStations()->setFixStations({corrected});
    REQUIRE(waitForFrame(&region));

    CHECK(layer->loadStatus() == cwLazLayer::LoadStatus::Loaded);
    CHECK(layer->pointCount() == 3);
    // The tile did not move, so in the frame that did it now sits ~100 km out.
    CHECK(distanceFromOrigin(layer) > kNearOriginMeters);
}

TEST_CASE("A frame that only changes hands leaves its clouds alone",
          "[cwLocalProjectionFrame]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir gisLayers = makeGisLayersDir(tempDir);

    cwCavingRegion region;
    bindEmptyFolder(&region, gisLayers);

    const cwFixStation fix = makeFix(QStringLiteral("A1"), kUtm10N,
                                     500000.0, 4194000.0, 110.0);
    cwCave* cave = addCaveWithFixes(&region, {fix});

    writeTile(gisLayers, QStringLiteral("a-tile"), cloudAt(500100.0, 4194100.0), kUtm10N);
    readFolder(&region);
    REQUIRE(waitForFrame(&region));

    auto* geoReference = region.geoReference();
    const QString frame = geoReference->localCoordinateSystem();
    auto* layer = region.lazLayers()->layerAt(0);
    REQUIRE(layer->loadStatus() == cwLazLayer::LoadStatus::Loaded);

    QSignalSpy loadSpy(layer, &cwLazLayer::loadStatusChanged);

    // Deleting the anchor freezes the frame: the tile beside it is close enough
    // that moving the origin would be churn. Every coordinate stays where it
    // was, so re-reading a directory of clouds off disk would buy nothing.
    cave->fixStations()->setFixStations({});
    REQUIRE(geoReference->state() == cwGeoReference::Frozen);
    REQUIRE(geoReference->localCoordinateSystem() == frame);

    REQUIRE(waitForFrame(&region));
    CHECK(loadSpy.count() == 0);
    CHECK(layer->loadStatus() == cwLazLayer::LoadStatus::Loaded);
}

TEST_CASE("Clearing the layers while the headers are still arriving leaves nothing behind",
          "[cwLocalProjectionFrame]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir gisLayers = makeGisLayersDir(tempDir);

    cwCavingRegion region;
    bindEmptyFolder(&region, gisLayers);

    writeTile(gisLayers, QStringLiteral("a-tile"), cloudAt(500000.0, 4194000.0), kUtm10N);
    writeTile(gisLayers, QStringLiteral("b-tile"), cloudAt(500100.0, 4194100.0), kUtm10N);

    readFolder(&region);
    REQUIRE(region.lazLayers()->anyHeaderProbeInFlight());

    // The layers the frame was going to be derived from are gone, and so is
    // everything that was waiting on it.
    region.lazLayers()->clear();
    CHECK_FALSE(region.lazLayers()->anyHeaderProbeInFlight());

    // The reads land on threads that no longer have anywhere to deliver to,
    // and the epoch they were holding open is abandoned rather than settled.
    constexpr int kDrainSlices = 20;
    for (int slice = 0; slice < kDrainSlices; ++slice) {
        spinEventLoopSlice();
    }

    CHECK(region.lazLayers()->count() == 0);
    CHECK(region.geoReference()->state() == cwGeoReference::Ungeoreferenced);
}
