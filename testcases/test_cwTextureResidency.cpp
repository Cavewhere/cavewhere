//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Qt includes
#include <QMatrix4x4>
#include <QSize>
#include <QVector2D>
#include <QVector3D>

//Std includes
#include <cmath>

//Our includes
#include "cwGeometry.h"
#include "cwTextureResidency.h"

using namespace cw::residency;
using Catch::Matchers::WithinAbs;

namespace {

    constexpr double kTolerance = 1e-9;

    cwGeometry quadGeometry(float sizeInMeters, float uvExtent)
    {
        cwGeometry geometry({
            { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
            { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
        });
        geometry.setType(cwGeometry::Type::Triangles);

        const QVector<QVector3D> positions = {
            { 0.0f, 0.0f, 0.0f },
            { sizeInMeters, 0.0f, 0.0f },
            { sizeInMeters, sizeInMeters, 0.0f },
            { 0.0f, sizeInMeters, 0.0f }
        };
        const QVector<QVector2D> texCoords = {
            { 0.0f, 0.0f },
            { uvExtent, 0.0f },
            { uvExtent, uvExtent },
            { 0.0f, uvExtent }
        };

        geometry.set(cwGeometry::Semantic::Position, positions);
        geometry.set(cwGeometry::Semantic::TexCoord0, texCoords);
        geometry.appendTriangle(0, 1, 2);
        geometry.appendTriangle(0, 2, 3);

        return geometry;
    }

    SelectionInput orthoInput()
    {
        SelectionInput input;
        input.textureSize = QSize(4096, 4096);
        input.uvPerMeter = 1.0;
        input.modelScale = 1.0;
        input.worldBounds = QBox3D(QVector3D(-1.0f, -1.0f, -1.0f), QVector3D(1.0f, 1.0f, 1.0f));
        input.absP11 = 1.0;
        input.viewportHeightPx = 4000;
        input.screenSpaceErrorPx = 1.5;
        return input;
    }

    ResidencyStats residentItem(int residentTopLevel, quint64 lastVisibleFrame, bool visible)
    {
        ResidencyStats stats;
        stats.textureSize = QSize(2048, 2048);
        stats.format = QRhiTexture::BC7;
        stats.residentTopLevel = residentTopLevel;
        stats.lastVisibleFrame = lastVisibleFrame;
        stats.visibleThisFrame = visible;
        return stats;
    }
}

TEST_CASE("mipLevelCount and mipLevelSize halve down to 1x1", "[TextureResidency]") {
    CHECK(mipLevelCount(QSize(2048, 2048)) == 12);
    CHECK(mipLevelCount(QSize(1, 1)) == 1);
    CHECK(mipLevelCount(QSize(8192, 6720)) == 14);
    CHECK(mipLevelCount(QSize()) == 0);

    CHECK(mipLevelSize(QSize(2048, 2048), 0) == QSize(2048, 2048));
    CHECK(mipLevelSize(QSize(2048, 2048), 11) == QSize(1, 1));
    CHECK(mipLevelSize(QSize(10, 6), 1) == QSize(5, 3));
    CHECK(mipLevelSize(QSize(10, 6), 2) == QSize(2, 1));
    CHECK(mipLevelSize(QSize(10, 6), 3) == QSize(1, 1));
}

TEST_CASE("mipLevelBytes counts blocks and pixels exactly", "[TextureResidency]") {
    constexpr qint64 kFourMebibytes = 4 * 1024 * 1024;
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize(2048, 2048)) == kFourMebibytes);
    CHECK(mipLevelBytes(QRhiTexture::ASTC_4x4, QSize(2048, 2048)) == kFourMebibytes);

    //10x6 rounds up to 3x2 blocks
    CHECK(mipLevelBytes(QRhiTexture::ASTC_4x4, QSize(10, 6)) == 3 * 2 * 16);
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize(5, 1)) == 2 * 1 * 16);

    //Tail levels are a single block
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize(1, 1)) == 16);
    CHECK(mipLevelBytes(QRhiTexture::ASTC_4x4, QSize(2, 2)) == 16);

    CHECK(mipLevelBytes(QRhiTexture::RGBA8, QSize(64, 32)) == 64 * 32 * 4);
    CHECK(mipLevelBytes(QRhiTexture::UnknownFormat, QSize(64, 32)) == 0);
    CHECK(mipLevelBytes(QRhiTexture::BC7, QSize()) == 0);
}

TEST_CASE("chainBytes sums a level and everything coarser", "[TextureResidency]") {
    const QSize size(2048, 2048);

    //16 bytes per block, summed over the 12 levels of a 2048x2048 BC7 chain
    CHECK(chainBytes(QRhiTexture::BC7, size, 0) == 5592432);
    CHECK(chainBytes(QRhiTexture::BC7, size, 0) - chainBytes(QRhiTexture::BC7, size, 1)
          == 4 * 1024 * 1024);
    CHECK(chainBytes(QRhiTexture::BC7, size, 11) == 16);
    CHECK(chainBytes(QRhiTexture::BC7, size, 12) == 0);
    CHECK(chainBytes(QRhiTexture::BC7, size, -1) == chainBytes(QRhiTexture::BC7, size, 0));
}

TEST_CASE("pinnedBaseLevel finds the first level at or under 512", "[TextureResidency]") {
    CHECK(mipLevelSize(QSize(8192, 6720), 4) == QSize(512, 420));
    CHECK(pinnedBaseLevel(QSize(8192, 6720)) == 4);
    CHECK(pinnedBaseLevel(QSize(256, 256)) == 0);
    CHECK(pinnedBaseLevel(QSize(512, 512)) == 0);
    CHECK(pinnedBaseLevel(QSize(1024, 512)) == 1);
    CHECK(pinnedBaseLevel(QSize()) == 0);
}

TEST_CASE("uvPerMeter measures texel density over the geometry", "[TextureResidency]") {
    //A 2m quad carrying 0..1 uv: sqrt(1 uv^2 / 4 m^2) = 0.5
    const cwGeometry quad = quadGeometry(2.0f, 1.0f);
    CHECK_THAT(uvPerMeter(quad, QMatrix4x4()), WithinAbs(0.5, kTolerance));

    QMatrix4x4 doubled;
    doubled.scale(2.0f);
    CHECK_THAT(uvPerMeter(quad, doubled), WithinAbs(0.25, kTolerance));

    QMatrix4x4 halved;
    halved.scale(0.5f);
    CHECK_THAT(uvPerMeter(quad, halved), WithinAbs(1.0, kTolerance));

    //Doubling the uv extent doubles the density
    CHECK_THAT(uvPerMeter(quadGeometry(2.0f, 2.0f), QMatrix4x4()), WithinAbs(1.0, kTolerance));
}

TEST_CASE("uvPerMeter returns zero when the density is unknown", "[TextureResidency]") {
    CHECK_THAT(uvPerMeter(quadGeometry(0.0f, 1.0f), QMatrix4x4()), WithinAbs(0.0, kTolerance));
    CHECK_THAT(uvPerMeter(quadGeometry(2.0f, 0.0f), QMatrix4x4()), WithinAbs(0.0, kTolerance));

    cwGeometry noIndices = quadGeometry(2.0f, 1.0f);
    noIndices.clearIndexData();
    CHECK_THAT(uvPerMeter(noIndices, QMatrix4x4()), WithinAbs(0.0, kTolerance));

    cwGeometry noTexCoords({ { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 } });
    noTexCoords.set(cwGeometry::Semantic::Position,
                    QVector<QVector3D>{ { 0.0f, 0.0f, 0.0f },
                                        { 1.0f, 0.0f, 0.0f },
                                        { 1.0f, 1.0f, 0.0f } });
    noTexCoords.appendTriangle(0, 1, 2);
    CHECK_THAT(uvPerMeter(noTexCoords, QMatrix4x4()), WithinAbs(0.0, kTolerance));

    CHECK_THAT(uvPerMeter(cwGeometry(), QMatrix4x4()), WithinAbs(0.0, kTolerance));
}

TEST_CASE("desiredTopLevel picks a level for an ortho camera", "[TextureResidency]") {
    //texelsPerMeter = 4096, pixelsPerMeter = 1 * 4000 / 2 = 2000,
    //texelsPerPixel * 1.5 = 3.072 -> floor(log2) = 1
    CHECK(desiredTopLevel(orthoInput()) == 1);

    SelectionInput looser = orthoInput();
    looser.screenSpaceErrorPx = 3.0;
    CHECK(desiredTopLevel(looser) == 2);

    SelectionInput tighter = orthoInput();
    tighter.screenSpaceErrorPx = 0.5;
    CHECK(desiredTopLevel(tighter) == 0);
}

TEST_CASE("desiredTopLevel clamps at both ends", "[TextureResidency]") {
    SelectionInput zoomedOut = orthoInput();
    zoomedOut.viewportHeightPx = 10;
    //Clamped to pinnedBaseLevel(4096x4096)
    CHECK(desiredTopLevel(zoomedOut) == 3);

    SelectionInput zoomedIn = orthoInput();
    zoomedIn.viewportHeightPx = 100000;
    CHECK(desiredTopLevel(zoomedIn) == 0);

    SelectionInput smallTexture = orthoInput();
    smallTexture.textureSize = QSize(256, 256);
    CHECK(desiredTopLevel(smallTexture) == 0);
}

TEST_CASE("desiredTopLevel handles a perspective camera", "[TextureResidency]") {
    QMatrix4x4 projection;
    projection.perspective(45.0f, 1.0f, 1.0f, 1000.0f);

    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 10.0f), QVector3D(), QVector3D(0.0f, 1.0f, 0.0f));

    SelectionInput input;
    input.textureSize = QSize(8192, 8192);
    input.uvPerMeter = 0.25;
    input.modelScale = 1.0;
    input.worldBounds = QBox3D(QVector3D(-1.0f, -1.0f, -1.0f), QVector3D(1.0f, 1.0f, 1.0f));
    input.viewProjection = projection * view;
    input.absP11 = std::abs(double(projection(1, 1)));
    input.viewportHeightPx = 4320;

    //texelsPerMeter = 2048, nearest w = 9, pixelsPerMeter = 2.4142 * 4320 / 18 = 579.4,
    //texelsPerPixel * 1.5 = 5.30 -> floor(log2) = 2
    CHECK(desiredTopLevel(input) == 2);

    //Pushing the box further away needs less detail
    QMatrix4x4 farView;
    farView.lookAt(QVector3D(0.0f, 0.0f, 100.0f), QVector3D(), QVector3D(0.0f, 1.0f, 0.0f));
    SelectionInput farther = input;
    farther.viewProjection = projection * farView;
    CHECK(desiredTopLevel(farther) > desiredTopLevel(input));

    //A model scaled up covers more meters per texel
    SelectionInput scaled = input;
    scaled.modelScale = 4.0;
    CHECK(desiredTopLevel(scaled) == 0);
}

TEST_CASE("desiredTopLevel asks for full detail when inputs are unknown", "[TextureResidency]") {
    SelectionInput unknownDensity = orthoInput();
    unknownDensity.uvPerMeter = 0.0;
    CHECK(desiredTopLevel(unknownDensity) == 0);

    SelectionInput nullBox = orthoInput();
    nullBox.worldBounds = QBox3D();
    CHECK(desiredTopLevel(nullBox) == 0);

    SelectionInput noViewport = orthoInput();
    noViewport.viewportHeightPx = 0;
    CHECK(desiredTopLevel(noViewport) == 0);

    SelectionInput noProjection = orthoInput();
    noProjection.absP11 = 0.0;
    CHECK(desiredTopLevel(noProjection) == 0);
}

TEST_CASE("planEvictions demotes the least recently visible first", "[TextureResidency]") {
    const QVector<ResidencyStats> items = {
        residentItem(0, 10, true),
        residentItem(0, 5, false),
        residentItem(0, 9, true),
        residentItem(0, 3, false)
    };

    constexpr qint64 kHugeOvershoot = 1024LL * 1024 * 1024;
    const QVector<Demotion> plan = planEvictions(items, kHugeOvershoot);

    REQUIRE(plan.size() == 4);
    CHECK(plan.at(0).itemIndex == 3);
    CHECK(plan.at(1).itemIndex == 1);
    CHECK(plan.at(2).itemIndex == 2);
    CHECK(plan.at(3).itemIndex == 0);

    //Every demotion lands on the pinned base and reclaims levels 0 and 1
    const qint64 expectedReclaim = chainBytes(QRhiTexture::BC7, QSize(2048, 2048), 0)
                                   - chainBytes(QRhiTexture::BC7, QSize(2048, 2048), 2);
    for(const Demotion& demotion : plan) {
        CHECK(demotion.newTopLevel == 2);
        CHECK(demotion.reclaimedBytes == expectedReclaim);
    }
}

TEST_CASE("planEvictions stops once the overshoot is covered", "[TextureResidency]") {
    const QVector<ResidencyStats> items = {
        residentItem(0, 10, true),
        residentItem(0, 5, false),
        residentItem(0, 3, false)
    };

    const qint64 oneItem = chainBytes(QRhiTexture::BC7, QSize(2048, 2048), 0)
                           - chainBytes(QRhiTexture::BC7, QSize(2048, 2048), 2);

    CHECK(planEvictions(items, 1).size() == 1);
    CHECK(planEvictions(items, oneItem).size() == 1);
    CHECK(planEvictions(items, oneItem + 1).size() == 2);
    CHECK(planEvictions(items, 0).isEmpty());
}

TEST_CASE("planEvictions skips base-level and in-flight items", "[TextureResidency]") {
    QVector<ResidencyStats> items = {
        residentItem(2, 1, false),   //Already at the pinned base
        residentItem(3, 2, false),   //Coarser than the pinned base
        residentItem(-1, 3, false),  //Nothing resident
        residentItem(0, 4, false)
    };

    ResidencyStats inFlight = residentItem(0, 0, false);
    inFlight.demotionInFlight = true;
    items.append(inFlight);

    const QVector<Demotion> plan = planEvictions(items, 1024LL * 1024 * 1024);
    REQUIRE(plan.size() == 1);
    CHECK(plan.at(0).itemIndex == 3);
}

TEST_CASE("planEvictions returns a partial plan when it cannot cover the overshoot",
          "[TextureResidency]") {
    const QVector<ResidencyStats> items = {
        residentItem(0, 1, false),
        residentItem(0, 2, false)
    };

    constexpr qint64 kUnreachableOvershoot = 1024LL * 1024 * 1024 * 1024;
    const QVector<Demotion> plan = planEvictions(items, kUnreachableOvershoot);

    REQUIRE(plan.size() == 2);
    qint64 reclaimed = 0;
    for(const Demotion& demotion : plan) {
        reclaimed += demotion.reclaimedBytes;
    }
    CHECK(reclaimed < kUnreachableOvershoot);
}

TEST_CASE("takeFromBudget meters uploads and guarantees progress", "[TextureResidency]") {
    constexpr qint64 kBudget = 100;

    qint64 remaining = kBudget;
    CHECK(takeFromBudget(remaining, 40, false));
    CHECK(remaining == 60);

    CHECK(takeFromBudget(remaining, 60, true));
    CHECK(remaining == 0);

    CHECK_FALSE(takeFromBudget(remaining, 1, true));
    CHECK(remaining == 0);

    //A single level larger than the whole budget still uploads when nothing has yet
    qint64 fresh = kBudget;
    CHECK(takeFromBudget(fresh, kBudget * 10, false));
    CHECK(fresh < 0);
    CHECK_FALSE(takeFromBudget(fresh, 1, true));
}
