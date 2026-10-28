// Pure-math tests for the billboard placement helpers shared by the item layer
// (cwRenderBillboards, leads) and the text layer (cwRenderTextBillboards,
// station labels). The extraction from cwRenderBillboards.cpp must change no
// behavior, so buildModelMatrix is pinned against a frozen copy of the
// pre-extraction function for the same inputs.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QMatrix4x4>
#include <QRay3D>
#include <QRect>
#include <QSize>
#include <QSizeF>
#include <QVector4D>

#include "cwBillboardGeometry.h"
#include "cwCamera.h"
#include "cwGeometryItersecter.h"
#include "cwPickQuery.h"
#include "cwRayHit.h"
#include "cwRenderBillboards.h"
#include "TestGeometryBuilders.h"

using Catch::Matchers::WithinAbs;

namespace {

// Frozen copy of cwRenderBillboards.cpp's file-scope buildModelMatrix as of
// commit 6a69b53e, before it moved into cwBillboardGeometry. Do not update it to
// track the shared helper; it is the reference the helper must reproduce.
namespace reference {
constexpr float kWorldPerPixel = 0.02f;
constexpr float kMinClipW = 1e-4f;
constexpr float kMinProjectionScale = 1e-6f;

QMatrix4x4 buildModelMatrix(const cwRenderBillboards::RenderSlot& slot,
                            const QMatrix4x4& view,
                            const QMatrix4x4& projection,
                            const QMatrix4x4& viewProjection,
                            const QSizeF& logicalViewport,
                            const cwBillboardSightLine& sightLine)
{
    const QVector3D right(view(0, 0), view(0, 1), view(0, 2));
    const QVector3D up(view(1, 0), view(1, 1), view(1, 2));
    const QVector3D forward(view(2, 0), view(2, 1), view(2, 2));

    const QVector3D position = sightLine.biasedPosition(slot.worldPosition, slot.depthBias);

    float scaleX = kWorldPerPixel;
    float scaleY = kWorldPerPixel;
    if (slot.sizeMode == cwRenderBillboards::SizeMode::ScreenConstant) {
        const QVector4D clip = viewProjection * QVector4D(slot.worldPosition, 1.0f);
        const float w = qMax(qAbs(clip.w()), kMinClipW);
        const float projX = qMax(qAbs(projection(0, 0)), kMinProjectionScale);
        const float projY = qMax(qAbs(projection(1, 1)), kMinProjectionScale);
        scaleX = 2.0f * w / (projX * float(logicalViewport.width()));
        scaleY = 2.0f * w / (projY * float(logicalViewport.height()));
    }

    QMatrix4x4 model;
    model.setColumn(0, QVector4D(right * scaleX, 0.0f));
    model.setColumn(1, QVector4D(-up * scaleY, 0.0f));
    model.setColumn(2, QVector4D(forward, 0.0f));
    model.setColumn(3, QVector4D(position, 1.0f));
    model.translate(float(-slot.contentSize.width() / 2.0),
                    float(-slot.contentSize.height() / 2.0),
                    0.0f);
    return model;
}
} // namespace reference

constexpr float kFieldOfView = 60.0f;
constexpr float kAspectRatio = 1.5f;
constexpr float kNearPlane = 1.0f;
constexpr float kFarPlane = 10000.0f;
constexpr float kOrthoHalfWidth = 30.0f;
constexpr float kOrthoHalfHeight = 20.0f;
constexpr float kOrthoDepthExtent = 10000.0f;

const QSizeF kLogicalViewport(1200.0, 800.0);
const QSizeF kContentSize(64.0, 18.0);
const QVector3D kWorldPosition(3.5f, -2.0f, 1.25f);
const QVector3D kEye(12.0f, -30.0f, 25.0f);
const QVector3D kTarget(0.0f, 0.0f, 0.0f);
const QVector3D kUp(0.0f, 0.0f, 1.0f);
constexpr float kDepthBias = 1.0f;

// Matrix elements are meter- and pixel-scale floats; the shared helper runs the
// same float operations in the same order, so the match is exact in practice.
constexpr double kMatrixEpsilon = 1e-5;

// One logical pixel, carried through a projection and the viewport mapping.
constexpr double kPixelEpsilon = 1e-3;

QMatrix4x4 makeView()
{
    QMatrix4x4 view;
    view.lookAt(kEye, kTarget, kUp);
    return view;
}

QMatrix4x4 makePerspective()
{
    QMatrix4x4 projection;
    projection.perspective(kFieldOfView, kAspectRatio, kNearPlane, kFarPlane);
    return projection;
}

QMatrix4x4 makeOrtho()
{
    QMatrix4x4 projection;
    projection.ortho(-kOrthoHalfWidth, kOrthoHalfWidth, -kOrthoHalfHeight, kOrthoHalfHeight,
                     -kOrthoDepthExtent, kOrthoDepthExtent);
    return projection;
}

void requireSameMatrix(const QMatrix4x4& actual, const QMatrix4x4& expected)
{
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            INFO("row " << row << " column " << column);
            REQUIRE_THAT(actual(row, column), WithinAbs(expected(row, column), kMatrixEpsilon));
        }
    }
}

// Logical-pixel position of a world point through @a viewProjection.
QPointF toLogicalPixels(const QMatrix4x4& viewProjection, const QVector3D& worldPosition)
{
    const QVector3D ndc = (viewProjection * QVector4D(worldPosition, 1.0f)).toVector3DAffine();
    return QPointF((ndc.x() + 1.0) * 0.5 * kLogicalViewport.width(),
                   (1.0 - ndc.y()) * 0.5 * kLogicalViewport.height());
}

} // namespace

TEST_CASE("cw::billboard::buildModelMatrix reproduces the pre-extraction matrix",
          "[cwBillboardGeometry]")
{
    const QMatrix4x4 view = makeView();

    const auto check = [&](const QMatrix4x4& projection) {
        const QMatrix4x4 viewProjection = projection * view;
        const cwBillboardSightLine sightLine(view, projection);

        for (const cwBillboardSizeMode sizeMode : {cwBillboardSizeMode::ScreenConstant,
                                                   cwBillboardSizeMode::WorldSized}) {
            for (const float depthBias : {0.0f, kDepthBias}) {
                INFO("sizeMode " << int(sizeMode) << " depthBias " << depthBias);

                cwRenderBillboards::RenderSlot slot;
                slot.worldPosition = kWorldPosition;
                slot.contentSize = kContentSize;
                slot.sizeMode = sizeMode;
                slot.depthBias = depthBias;

                const QMatrix4x4 expected = reference::buildModelMatrix(
                    slot, view, projection, viewProjection, kLogicalViewport, sightLine);
                const QMatrix4x4 actual = cw::billboard::buildModelMatrix(
                    kWorldPosition, kContentSize, sizeMode, depthBias,
                    view, projection, viewProjection, kLogicalViewport, sightLine);
                requireSameMatrix(actual, expected);
            }
        }
    };

    SECTION("perspective")
    {
        check(makePerspective());
    }

    SECTION("orthographic")
    {
        check(makeOrtho());
    }
}

TEST_CASE("cw::billboard::buildModelMatrix maps one content pixel to one screen pixel",
          "[cwBillboardGeometry]")
{
    // ScreenConstant is the mode both layers use: the content's corners must land
    // contentSize logical pixels apart on screen, centered on the station.
    const QMatrix4x4 view = makeView();

    const auto check = [&](const QMatrix4x4& projection) {
        const QMatrix4x4 viewProjection = projection * view;
        const cwBillboardSightLine sightLine(view, projection);
        const QMatrix4x4 model = cw::billboard::buildModelMatrix(
            kWorldPosition, kContentSize, cwBillboardSizeMode::ScreenConstant, 0.0f,
            view, projection, viewProjection, kLogicalViewport, sightLine);

        const QPointF topLeft = toLogicalPixels(viewProjection, model.map(QVector3D(0, 0, 0)));
        const QPointF bottomRight = toLogicalPixels(
            viewProjection,
            model.map(QVector3D(float(kContentSize.width()), float(kContentSize.height()), 0)));
        const QPointF center = toLogicalPixels(viewProjection, kWorldPosition);

        REQUIRE_THAT(bottomRight.x() - topLeft.x(), WithinAbs(kContentSize.width(), kPixelEpsilon));
        REQUIRE_THAT(bottomRight.y() - topLeft.y(), WithinAbs(kContentSize.height(), kPixelEpsilon));
        REQUIRE_THAT((topLeft.x() + bottomRight.x()) * 0.5, WithinAbs(center.x(), kPixelEpsilon));
        REQUIRE_THAT((topLeft.y() + bottomRight.y()) * 0.5, WithinAbs(center.y(), kPixelEpsilon));
    };

    SECTION("perspective")
    {
        check(makePerspective());
    }

    SECTION("orthographic")
    {
        check(makeOrtho());
    }
}

TEST_CASE("cw::billboard::FrameCamera places billboards like buildModelMatrix",
          "[cwBillboardGeometry]")
{
    // Both backends place through FrameCamera, which derives the logical viewport
    // from the target's pixel size and the job's device pixel ratio.
    constexpr float kDevicePixelRatio = 2.0f;
    const QSize pixelSize(int(kLogicalViewport.width() * kDevicePixelRatio),
                          int(kLogicalViewport.height() * kDevicePixelRatio));
    const QMatrix4x4 view = makeView();

    const auto check = [&](const QMatrix4x4& projection) {
        const QMatrix4x4 viewProjection = projection * view;
        const cwBillboardSightLine sightLine(view, projection);
        const cw::billboard::FrameCamera frameCamera(view, projection, viewProjection,
                                                     pixelSize, kDevicePixelRatio);
        requireSameMatrix(frameCamera.viewProjection(), viewProjection);

        for (const cwBillboardSizeMode sizeMode : {cwBillboardSizeMode::ScreenConstant,
                                                   cwBillboardSizeMode::WorldSized}) {
            for (const float depthBias : {0.0f, kDepthBias}) {
                INFO("sizeMode " << int(sizeMode) << " depthBias " << depthBias);
                const QMatrix4x4 expected = cw::billboard::buildModelMatrix(
                    kWorldPosition, kContentSize, sizeMode, depthBias,
                    view, projection, viewProjection, kLogicalViewport, sightLine);
                requireSameMatrix(frameCamera.modelMatrix(kWorldPosition, kContentSize,
                                                          sizeMode, depthBias),
                                  expected);
            }
        }
    };

    SECTION("perspective")
    {
        check(makePerspective());
    }

    SECTION("orthographic")
    {
        check(makeOrtho());
    }
}

TEST_CASE("cw::billboard::buildModelMatrix shifts the content by a y-up pixel offset",
          "[cwBillboardGeometry]")
{
    // The offset is in logical pixels, y-up, applied after projection: the
    // content's center lands offset.x() right of and offset.y() above the
    // station on the y-down screen, under either projection and at any depth.
    const QPointF pixelOffset(7.0, 23.0);
    const QPointF contentCenter(kContentSize.width() / 2.0, kContentSize.height() / 2.0);
    constexpr float kDevicePixelRatio = 1.0f;
    const QSize pixelSize = kLogicalViewport.toSize();
    const QMatrix4x4 view = makeView();

    const auto check = [&](const QMatrix4x4& projection) {
        const QMatrix4x4 viewProjection = projection * view;
        const cwBillboardSightLine sightLine(view, projection);
        const QPointF station = toLogicalPixels(viewProjection, kWorldPosition);
        const QPointF expected = station + QPointF(pixelOffset.x(), -pixelOffset.y());

        const auto centerOf = [&](const QMatrix4x4& model) {
            return toLogicalPixels(
                viewProjection,
                model.map(QVector3D(float(contentCenter.x()), float(contentCenter.y()), 0.0f)));
        };

        const QMatrix4x4 model = cw::billboard::buildModelMatrix(
            kWorldPosition, kContentSize, cwBillboardSizeMode::ScreenConstant, 0.0f,
            view, projection, viewProjection, kLogicalViewport, sightLine, pixelOffset);
        const QPointF center = centerOf(model);
        CHECK_THAT(center.x(), WithinAbs(expected.x(), kPixelEpsilon));
        CHECK_THAT(center.y(), WithinAbs(expected.y(), kPixelEpsilon));

        // The content keeps its size; only its placement moves.
        const QPointF topLeft = toLogicalPixels(viewProjection, model.map(QVector3D(0, 0, 0)));
        const QPointF bottomRight = toLogicalPixels(
            viewProjection,
            model.map(QVector3D(float(kContentSize.width()), float(kContentSize.height()), 0)));
        CHECK_THAT(bottomRight.x() - topLeft.x(), WithinAbs(kContentSize.width(), kPixelEpsilon));
        CHECK_THAT(bottomRight.y() - topLeft.y(), WithinAbs(kContentSize.height(), kPixelEpsilon));

        // FrameCamera forwards the offset.
        const cw::billboard::FrameCamera frameCamera(view, projection, viewProjection,
                                                     pixelSize, kDevicePixelRatio);
        const QPointF frameCenter = centerOf(frameCamera.modelMatrix(
            kWorldPosition, kContentSize, cwBillboardSizeMode::ScreenConstant, 0.0f, pixelOffset));
        CHECK_THAT(frameCenter.x(), WithinAbs(expected.x(), kPixelEpsilon));
        CHECK_THAT(frameCenter.y(), WithinAbs(expected.y(), kPixelEpsilon));
    };

    SECTION("perspective")
    {
        check(makePerspective());
    }

    SECTION("orthographic")
    {
        check(makeOrtho());
    }
}

TEST_CASE("cw::billboard::sortDepth is the NDC depth of the world position",
          "[cwBillboardGeometry]")
{
    const QMatrix4x4 viewProjection = makePerspective() * makeView();
    const float expected = (viewProjection * QVector4D(kWorldPosition, 1.0f)).toVector3DAffine().z();
    REQUIRE_THAT(cw::billboard::sortDepth(kWorldPosition, viewProjection),
                 WithinAbs(expected, kMatrixEpsilon));

    // Farther along the view direction is a larger depth.
    const QVector3D fartherPosition = kWorldPosition + (kTarget - kEye).normalized() * 10.0f;
    REQUIRE(cw::billboard::sortDepth(fartherPosition, viewProjection)
            > cw::billboard::sortDepth(kWorldPosition, viewProjection));
}

TEST_CASE("cw::billboard::sortBackToFront orders any slot type farthest-first",
          "[cwBillboardGeometry]")
{
    // The text layer sorts its own slot type with the same template.
    struct Slot {
        cwBillboardId id;
        QVector3D worldPosition;
        float sortDepth = 0.0f;
    };

    QMatrix4x4 view;
    view.lookAt(QVector3D(0, 0, 10), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    const QMatrix4x4 viewProjection = makeOrtho() * view;

    QVector<Slot> renderSlots{
        {cwBillboardId{1}, QVector3D(0, 0, 5)},    // nearest
        {cwBillboardId{2}, QVector3D(0, 0, -5)},   // farthest
        {cwBillboardId{4}, QVector3D(1, 0, 0)},    // middle, tie with 3
        {cwBillboardId{3}, QVector3D(-1, 0, 0)},   // middle, tie with 4
    };
    cw::billboard::sortBackToFront(renderSlots, viewProjection);

    REQUIRE(renderSlots.size() == 4);
    REQUIRE(renderSlots.at(0).id == cwBillboardId{2});
    REQUIRE(renderSlots.at(1).id == cwBillboardId{3});
    REQUIRE(renderSlots.at(2).id == cwBillboardId{4});
    REQUIRE(renderSlots.at(3).id == cwBillboardId{1});
}

namespace {

// The occlusion cases use cwCamera's default orthographic view: identity view
// matrix, looking down -Z, so a LARGER z is NEARER the eye, and one logical
// pixel is half a world unit. requireLookingDownNegativeZ pins that premise.
constexpr int kOcclusionViewportSize = 400;
const QSizeF kSpriteSize(16.0, 24.0);
const QVector3D kStation(0.0f, 0.0f, 0.0f);
const QPointF kCenteredAnchor(0.5, 0.5);
const QPointF kTipDownAnchor(0.5, 1.0);
constexpr float kOcclusionDepthBias = 2.0f;

// Integer pixel offsets from the station's projection (rayFromQtViewport rounds
// the tap to a whole pixel). With a 24 px tall sprite, kAboveStation is on the
// body of a tip-down pin and above a centered sprite's top edge; kBelowStation
// is in a centered sprite's lower half and below a tip-down pin's tip.
const QPointF kAboveStation(0.0, -18.0);
const QPointF kBelowStation(0.0, 6.0);
// Half the sprite height above the tip: the middle of a tip-down pin.
const QPointF kPinBodyCenter(0.0, -12.0);
// Far outside any sprite drawn at the station.
const QPointF kFarFromStation(100.0, 0.0);
// A unit ray direction component that should be exactly 0 or -1.
constexpr double kDirectionMargin = 1e-6;

constexpr uint64_t kWallId = 1;
constexpr uint64_t kGroundId = 2;

// How far in front of the station the wall stands, well past any depth bias.
constexpr float kWallInFront = 50.0f;
constexpr float kWallHalfExtent = 30.0f;
// Off-center so no tested ray runs along the quad's shared diagonal.
const QVector3D kWallCenterOffset(5.0f, 3.0f, 0.0f);

// The ground is the plane z = -kGroundSlope * y through the station: steep, so
// world below the station on screen is nearer the eye and world above it is
// farther.
constexpr float kGroundSlope = 10.0f;
constexpr float kGroundHalfExtent = 20.0f;

struct OcclusionFixture
{
    cwCamera camera;
    cwGeometryItersecter intersecter;

    OcclusionFixture()
    {
        camera.setViewport(QRect(0, 0, kOcclusionViewportSize, kOcclusionViewportSize));
        camera.setProjection(camera.orthoProjectionDefault());
    }

    QPointF tapAt(const QPointF& offset) const { return camera.project(kStation) + offset; }

    bool isOccluded(const QPointF& tap,
                    const QPointF& anchor,
                    const QSizeF& spriteSize = kSpriteSize) const
    {
        return cw::billboard::isOccluded(&intersecter, &camera, kStation, tap,
                                         spriteSize, anchor, kOcclusionDepthBias);
    }

    void addObject(uint64_t id, const cwGeometry& geometry)
    {
        intersecter.addObject(cwGeometryItersecter::Object(nullptr, id, geometry));
        intersecter.waitForFinish();
    }

    // A camera-facing wall kWallInFront nearer the eye than the station.
    void addWall(const QVector3D& center)
    {
        addObject(kWallId, cwTestGeometry::zPlaneQuad(center + QVector3D(0.0f, 0.0f, kWallInFront),
                                                      kWallHalfExtent));
    }

    void addGround()
    {
        const float lowZ = -kGroundSlope * -kGroundHalfExtent;
        const float highZ = -kGroundSlope * kGroundHalfExtent;
        addObject(kGroundId, cwTestGeometry::triangles(
                                 {QVector3D(-kGroundHalfExtent, -kGroundHalfExtent, lowZ),
                                  QVector3D(kGroundHalfExtent, -kGroundHalfExtent, lowZ),
                                  QVector3D(kGroundHalfExtent, kGroundHalfExtent, highZ),
                                  QVector3D(-kGroundHalfExtent, kGroundHalfExtent, highZ)},
                                 {0, 1, 2, 0, 2, 3}, false));
    }

    // World z where the ray through @a tap hits solid geometry.
    float hitZ(const QPointF& tap) const
    {
        const QRay3D ray = camera.rayFromQtViewport(tap);
        const cwRayHit hit = intersecter.intersectsDetailed(ray, cwPickQuery{.kinds = cwPickQuery::Solid});
        REQUIRE(hit.hit());
        return ray.point(hit.tWorld()).z();
    }

    void requireLookingDownNegativeZ() const
    {
        const QRay3D ray = camera.rayFromQtViewport(tapAt(QPointF()));
        REQUIRE(ray.direction().x() == Catch::Approx(0.0f).margin(kDirectionMargin));
        REQUIRE(ray.direction().y() == Catch::Approx(0.0f).margin(kDirectionMargin));
        REQUIRE(ray.direction().z() == Catch::Approx(-1.0f).margin(kDirectionMargin));
    }
};

} // namespace

TEST_CASE("cw::billboard::isOccluded is false on the sprite in open air and true off it",
          "[cwBillboardGeometry]")
{
    OcclusionFixture f;
    f.requireLookingDownNegativeZ();

    CHECK_FALSE(f.isOccluded(f.tapAt(kPinBodyCenter), kTipDownAnchor));

    // Beside the pin and below its tip no pin pixel is drawn.
    const QPointF beside(kSpriteSize.width(), kPinBodyCenter.y());
    CHECK(f.isOccluded(f.tapAt(beside), kTipDownAnchor));
    CHECK(f.isOccluded(f.tapAt(kBelowStation), kTipDownAnchor));
}

TEST_CASE("cw::billboard::isOccluded is true behind a wall", "[cwBillboardGeometry]")
{
    OcclusionFixture f;
    f.requireLookingDownNegativeZ();

    const QPointF tap = f.tapAt(kPinBodyCenter);
    const QRay3D ray = f.camera.rayFromQtViewport(tap);
    f.addWall(ray.point(ray.origin().z() - kStation.z()) + kWallCenterOffset);
    REQUIRE(f.hitZ(tap) == Catch::Approx(kStation.z() + kWallInFront));

    CHECK(f.isOccluded(tap, kTipDownAnchor));
}

TEST_CASE("cw::billboard::isOccluded tests the sprite's anchored quad", "[cwBillboardGeometry]")
{
    OcclusionFixture f;
    f.requireLookingDownNegativeZ();
    f.addGround();

    const QPointF tapAbove = f.tapAt(kAboveStation);
    const QPointF tapBelow = f.tapAt(kBelowStation);

    // The ground is behind the sprite plane above the station and in front of it,
    // by more than the bias, below.
    REQUIRE(f.hitZ(tapAbove) < kStation.z() - kOcclusionDepthBias);
    REQUIRE(f.hitZ(tapBelow) > kStation.z() + kOcclusionDepthBias);

    // A tip-down pin stands above the surface: its body is in front of the ground.
    CHECK_FALSE(f.isOccluded(tapAbove, kTipDownAnchor));

    // A centered sprite draws nothing that high, and its lower half is buried.
    CHECK(f.isOccluded(tapAbove, kCenteredAnchor));
    CHECK(f.isOccluded(tapBelow, kCenteredAnchor));
}

TEST_CASE("cw::billboard::isOccluded tests only the plane for an empty sprite size",
          "[cwBillboardGeometry]")
{
    // cwLeadView forwards its spriteSize, which is empty before any sprite slot
    // is declared; the plane test alone then decides.
    OcclusionFixture f;
    f.requireLookingDownNegativeZ();

    const QPointF farTap = f.tapAt(kFarFromStation);
    CHECK_FALSE(f.isOccluded(farTap, kCenteredAnchor, QSizeF()));

    const QRay3D ray = f.camera.rayFromQtViewport(farTap);
    f.addWall(ray.point(ray.origin().z() - kStation.z()) + kWallCenterOffset);
    CHECK(f.isOccluded(farTap, kCenteredAnchor, QSizeF()));
}
