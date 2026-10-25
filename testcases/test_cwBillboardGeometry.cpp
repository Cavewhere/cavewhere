// Pure-math tests for the billboard placement helpers shared by the item layer
// (cwRenderBillboards, leads) and the text layer (cwRenderTextBillboards,
// station labels). The extraction from cwRenderBillboards.cpp must change no
// behavior, so buildModelMatrix is pinned against a frozen copy of the
// pre-extraction function for the same inputs.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QMatrix4x4>
#include <QSize>
#include <QSizeF>
#include <QVector4D>

#include "cwBillboardGeometry.h"
#include "cwRenderBillboards.h"

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
