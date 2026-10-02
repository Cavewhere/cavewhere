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
#include "cwCavingRegion.h"
#include "cwGeoReference.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwProject.h"
#include "cwRootData.h"

#include "LazFixtureHelper.h"

//Qt includes
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>
#include <QVector3D>

#include <memory>

namespace {

const QString kUtmZone10N = utmZoneWkt(10, -123);

//! A cloud sitting somewhere a UTM zone 10N file plausibly could, so the frame
//! derived from its bbox center is one PROJ can actually build.
QVector<QVector3D> pointsInZone10N()
{
    return {
        {500000.0f, 4194000.0f, 100.0f},
        {500010.0f, 4194010.0f, 110.0f},
        {500020.0f, 4194020.0f, 120.0f}
    };
}

} // namespace

TEST_CASE("A load reports the bounding box in the file's own CRS",
          "[cwLazSourceBbox][cwLazLayer]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("source-bbox"));
    REQUIRE(writeSyntheticLazFile(path, pointsInZone10N(), kUtmZone10N));

    cwLazLayer layer;
    // No region frame set, so the loaded bboxMin/bboxMax come back in source
    // coordinates too — but as floats, which is exactly why the raw header
    // numbers are carried separately.
    layer.setSourcePath(path);
    REQUIRE(waitForLazLayerLoaded(&layer));
    REQUIRE(layer.loadStatus() == cwLazLayer::LoadStatus::Loaded);

    CHECK(layer.sourceCS() == kUtmZone10N);
    CHECK(layer.sourceBboxMin().x == Catch::Approx(500000.0).margin(0.01));
    CHECK(layer.sourceBboxMax().x == Catch::Approx(500020.0).margin(0.01));
    CHECK(layer.sourceBboxCenter().x == Catch::Approx(500010.0).margin(0.01));
    CHECK(layer.sourceBboxCenter().y == Catch::Approx(4194010.0).margin(0.01));
}

TEST_CASE("A layer that will never load still reads its header",
          "[cwLazSourceBbox][cwLazLayer]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString path = tempLazPath(tempDir, QStringLiteral("never-loaded"));
    REQUIRE(writeSyntheticLazFile(path, pointsInZone10N(), kUtmZone10N));

    cwLazLayer layer;
    layer.setEnabled(false);
    layer.setSourcePath(path);

    // Disabled layers spend no async read on the points, but where the file
    // sits is a fact about the file: the header probe runs regardless, and
    // everything the frame is derived from comes out of it.
    REQUIRE(waitForLazLayerHeader(&layer));
    CHECK(layer.loadStatus() == cwLazLayer::LoadStatus::Probed);
    CHECK(layer.sourceCS() == kUtmZone10N);
    CHECK(layer.sourceBboxCenter().x == Catch::Approx(500010.0).margin(0.01));
    CHECK(layer.sourceBboxCenter().y == Catch::Approx(4194010.0).margin(0.01));
    CHECK(layer.pointCount() == 0);
}

TEST_CASE("A layer that never loads is still an anchor the frame can be lost with",
          "[cwLazSourceBbox][cwLocalProjectionManager]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    const QString path = tempLazPath(tempDir, QStringLiteral("disabled-anchor"));
    REQUIRE(writeSyntheticLazFile(path, pointsInZone10N(), kUtmZone10N));

    REQUIRE(addLazAndWait(root.get(), QStringList{path}));
    REQUIRE(region->lazLayers()->count() == 1);
    auto* layer = region->lazLayers()->layerAt(0);
    REQUIRE(waitForLazLayerLoaded(layer));
    REQUIRE(region->geoReference()->state() == cwGeoReference::Anchored);

    // Turning the points off says nothing about where the cloud is, so the
    // frame it anchors has no reason to move. The header is what makes the
    // layer an input, and the header is still read.
    layer->setEnabled(false);
    REQUIRE(layer->loadStatus() == cwLazLayer::LoadStatus::Probed);
    CHECK(region->geoReference()->state() == cwGeoReference::Anchored);
    CHECK(region->geoReference()->anchor().id == layer->id());

    // Deleting it is the other half of the same fact: a layer that counts as
    // present is a layer whose removal counts as a deletion.
    region->lazLayers()->removeAt(0);
    CHECK(region->geoReference()->state() == cwGeoReference::Ungeoreferenced);
}

TEST_CASE("Reloading the anchor layer leaves the frame where it is",
          "[cwLazSourceBbox][cwLocalProjectionManager]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    const QString path = tempLazPath(tempDir, QStringLiteral("reload-anchor"));
    REQUIRE(writeSyntheticLazFile(path, pointsInZone10N(), kUtmZone10N));

    REQUIRE(addLazAndWait(root.get(), QStringList{path}));
    REQUIRE(region->lazLayers()->count() == 1);
    auto* layer = region->lazLayers()->layerAt(0);
    REQUIRE(waitForLazLayerLoaded(layer));
    REQUIRE(region->geoReference()->state() == cwGeoReference::Anchored);

    const QString frame = region->geoReference()->localCoordinateSystem();
    REQUIRE_FALSE(frame.isEmpty());

    // A reload re-reads points, not the CRS, so the anchor stays an input for
    // the whole of it. Were it to drop out while Loading, the project would go
    // transiently un-georeferenced and the reload would restart untransformed.
    layer->reload();
    REQUIRE(layer->loadStatus() == cwLazLayer::LoadStatus::Loading);
    CHECK(region->geoReference()->state() == cwGeoReference::Anchored);
    CHECK(region->geoReference()->localCoordinateSystem() == frame);

    REQUIRE(waitForLazLayerLoaded(layer));
    CHECK(region->geoReference()->state() == cwGeoReference::Anchored);
    CHECK(region->geoReference()->localCoordinateSystem() == frame);
}

TEST_CASE("A LAZ layer alone anchors the local projection",
          "[cwLazSourceBbox][cwLocalProjectionManager]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    REQUIRE(region->geoReference()->state() == cwGeoReference::Ungeoreferenced);

    const QString path = tempLazPath(tempDir, QStringLiteral("anchor-layer"));
    REQUIRE(writeSyntheticLazFile(path, pointsInZone10N(), kUtmZone10N));

    REQUIRE(addLazAndWait(root.get(), QStringList{path}));
    REQUIRE(region->lazLayers()->count() == 1);

    // The frame follows the load now, not the insert: a layer only becomes an
    // anchor candidate once its header has actually been read.
    auto* layer = region->lazLayers()->layerAt(0);
    REQUIRE(waitForLazLayerLoaded(layer));
    REQUIRE(layer->loadStatus() == cwLazLayer::LoadStatus::Loaded);

    // No fix stations anywhere, so the layer is the only georeferenced input.
    CHECK(region->geoReference()->state() == cwGeoReference::Anchored);
    CHECK(region->geoReference()->anchor().kind == cwGeoReference::Anchor::LazLayer);
    CHECK(region->geoReference()->anchor().id == region->lazLayers()->layerAt(0)->id());
    CHECK_FALSE(region->geoReference()->localCoordinateSystem().isEmpty());
}
