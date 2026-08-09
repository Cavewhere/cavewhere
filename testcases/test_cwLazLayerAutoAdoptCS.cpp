// test_cwLazLayerAutoAdoptCS.cpp
// Catch2 tests for cwLazLayerModel deriving the project's local projection from
// the first added LAZ when the project has no frame of its own.

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include "cwCavingRegion.h"
#include "cwFutureManagerModel.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLocalProjection.h"
#include "cwProject.h"
#include "cwRootData.h"

#include "LazFixtureHelper.h"

namespace {

// The derivation reads the header's CS and the bbox center and builds a
// transverse Mercator centered there.
const QString kUtmZone10N = utmZoneWkt(10, -123);

// bbox of minimalLazPoints() is (0,0,0)-(4,4,4), so the derivation anchors on
// its center.
const cwGeoPoint kBboxCenter{2.0, 2.0, 2.0};

//! How far east and north of kBboxCenter the existing-frame test centers its
//! stored frame: ~14 km diagonal, inside the layer's 50 km reach, so the layer
//! says nothing is wrong with the frame it finds.
constexpr double kExistingFrameOffsetMeters = 10000.0;

} // namespace

TEST_CASE("Derive frame: empty project + LAZ with embedded CS anchors on the layer",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    auto* geoReference = region->geoReference();
    REQUIRE(geoReference->state() == cwGeoReference::Ungeoreferenced);

    QSignalSpy frameSpy(geoReference, &cwGeoReference::localProjectionChanged);

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("with-cs")),
        kUtmZone10N);
    REQUIRE(!path.isEmpty());

    REQUIRE(addLazAndWait(root.get(), QStringList{path}));

    REQUIRE(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::LazLayer);
    REQUIRE(region->lazLayers()->count() == 1);
    CHECK(geoReference->anchor().id == region->lazLayers()->layerAt(0)->id());
    CHECK(geoReference->localCoordinateSystem()
          == cwLocalProjection::deriveFrom(kUtmZone10N, kBboxCenter));
    CHECK(frameSpy.size() >= 1);
}

TEST_CASE("Derive frame: a LAZ without an embedded CS leaves the project local",
          "[cwLazAutoAdoptCS]") {
    // Nothing says where the cloud is, so there is nothing to center a
    // projection on — the project stays in its floating local frame.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("no-cs")));
    REQUIRE(!path.isEmpty());

    REQUIRE(addLazAndWait(root.get(), QStringList{path}));

    CHECK(region->geoReference()->state() == cwGeoReference::Ungeoreferenced);
    CHECK(region->geoReference()->localCoordinateSystem().isEmpty());
}

TEST_CASE("Derive frame: a project that already has a frame is left untouched",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    auto* geoReference = region->geoReference();

    // A frame the project already keeps, offset from where the layer sits: near
    // enough that the layer says nothing is wrong with it, and not a string any
    // derivation from the layer could produce.
    const QString existingFrame = cwLocalProjection::deriveFrom(
        kUtmZone10N,
        cwGeoPoint{kBboxCenter.x + kExistingFrameOffsetMeters,
                   kBboxCenter.y + kExistingFrameOffsetMeters,
                   0.0});
    REQUIRE_FALSE(existingFrame.isEmpty());
    geoReference->restore(cwGeoReference::Frozen, existingFrame, {}, QString());

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("preset")),
        kUtmZone10N);
    REQUIRE(!path.isEmpty());

    REQUIRE(addLazAndWait(root.get(), QStringList{path}));

    CHECK(geoReference->localCoordinateSystem() == existingFrame);
}

TEST_CASE("Derive frame: a second add leaves the frame on the first layer",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    const QString first = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("first")), kUtmZone10N);
    REQUIRE(addLazAndWait(root.get(), QStringList{first}));

    const QString frameAfterFirst = region->geoReference()->localCoordinateSystem();
    REQUIRE_FALSE(frameAfterFirst.isEmpty());
    const QUuid anchorAfterFirst = region->geoReference()->anchor().id;

    const QString utm11n = utmZoneWkt(11, -117);

    const QString second = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("second")), utm11n);
    REQUIRE(addLazAndWait(root.get(), QStringList{second}));

    CHECK(region->geoReference()->localCoordinateSystem() == frameAfterFirst);
    CHECK(region->geoReference()->anchor().id == anchorAfterFirst);
}

TEST_CASE("The region names the GIS layer the frame is centered on",
          "[cwLazAutoAdoptCS][cwAnchorDescription]") {
    // The anchor is persisted as an id, so the name has to be resolved against
    // the layer currently carrying it — which means it has to follow a rename
    // and let go when the layer does.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("anchor-name")),
        kUtmZone10N);
    REQUIRE(!path.isEmpty());
    REQUIRE(addLazAndWait(root.get(), QStringList{path}));

    REQUIRE(region->geoReference()->anchor().kind == cwGeoReference::Anchor::LazLayer);
    REQUIRE(region->lazLayers()->count() == 1);
    cwLazLayer* layer = region->lazLayers()->layerAt(0);
    REQUIRE(layer != nullptr);

    // The layer's own name, with no cave to qualify it.
    CHECK(region->geoReference()->anchorDescription() == layer->name());

    SECTION("the description follows a layer rename") {
        QSignalSpy spy(region->geoReference(), &cwGeoReference::anchorDescriptionChanged);

        REQUIRE(region->lazLayers()->rename(0, QStringLiteral("blue-spring-entrance")));

        CHECK(spy.size() == 1);
        CHECK(region->geoReference()->anchorDescription() == QStringLiteral("blue-spring-entrance"));
        CHECK(region->geoReference()->anchorDescription() == layer->name());
    }

    SECTION("removing the layer leaves nothing to name") {
        region->lazLayers()->removeAt(0);

        CHECK(region->geoReference()->anchorDescription().isEmpty());
    }
}
