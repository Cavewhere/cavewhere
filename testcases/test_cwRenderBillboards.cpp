// GUI-thread lifecycle test for cwRenderBillboards. Without a live QRhi the
// render-thread backend never runs, so this covers the GUI side: the slot
// snapshot it hands to the render thread, that null content is skipped, and that
// clearing the list releases the referenced subscenes cleanly. The full
// materialize-and-occlude path is covered live by the app and the QML/offscreen
// suite.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QQuickItem>
#include <QQuickWindow>
#include <QMatrix4x4>
#include <QVector4D>

#include "cwRenderBillboards.h"

using Catch::Matchers::WithinAbs;

namespace {
cwRenderBillboards::RenderSlot makeSlot(uint32_t id, const QVector3D& worldPosition)
{
    cwRenderBillboards::RenderSlot slot;
    slot.id = cwBillboardId{id};
    slot.worldPosition = worldPosition;
    return slot;
}

// Normalized device coordinates of a world position: x/y are the screen position,
// z the depth the hardware test compares.
QVector3D toNdc(const QMatrix4x4& viewProjection, const QVector3D& worldPosition)
{
    return (viewProjection * QVector4D(worldPosition, 1.0f)).toVector3DAffine();
}

// The two projections the 3D view uses, sized like the turntable's: the ortho
// frustum spans the viewport at the current zoom and its near plane sits behind
// the eye, the perspective one starts at 1 m (cwCamera::orthoProjectionDefault /
// perspectiveProjectionDefault).
QMatrix4x4 makeOrtho()
{
    constexpr float kHalfExtent = 25.0f;
    constexpr float kDepthExtent = 10000.0f;
    QMatrix4x4 projection;
    projection.ortho(-kHalfExtent, kHalfExtent, -kHalfExtent, kHalfExtent,
                     -kDepthExtent, kDepthExtent);
    return projection;
}

QMatrix4x4 makePerspective()
{
    constexpr float kFieldOfView = 60.0f;
    constexpr float kAspectRatio = 1.0f;
    constexpr float kNearPlane = 1.0f;
    constexpr float kFarPlane = 10000.0f;
    QMatrix4x4 projection;
    projection.perspective(kFieldOfView, kAspectRatio, kNearPlane, kFarPlane);
    return projection;
}

// The bias the label and lead views ask for, in meters.
constexpr float kBias = 1.0f;

// Tolerance for a meter-scale world coordinate carried through a float matrix.
constexpr double kWorldEpsilon = 1e-3;

// Tolerance on a normalized device coordinate: exact for the parallel projection,
// loosened for the perspective divide.
constexpr double kNdcEpsilon = 1e-6;
constexpr double kNdcPerspectiveEpsilon = 1e-5;
}

TEST_CASE("cwRenderBillboards: snapshot reflects the billboards", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    content->setSize(QSizeF(120, 40));

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    billboard.worldPosition = QVector3D(1, 2, 3);
    billboard.sizeMode = cwRenderBillboards::SizeMode::ScreenConstant;
    billboard.depthBias = 0.5f;
    const cwBillboardId id = billboards.addBillboard(billboard);
    REQUIRE(id != cwBillboardId{});
    REQUIRE(billboards.hasBillboard(id));
    REQUIRE(billboards.billboardCount() == 1);

    const auto renderSlots = billboards.buildRenderSlots();
    REQUIRE(renderSlots.size() == 1);
    REQUIRE(renderSlots.at(0).id == id);
    REQUIRE(renderSlots.at(0).contentSize == QSizeF(120, 40));
    REQUIRE(renderSlots.at(0).worldPosition == QVector3D(1, 2, 3));
    REQUIRE(renderSlots.at(0).sizeMode == cwRenderBillboards::SizeMode::ScreenConstant);
    REQUIRE(renderSlots.at(0).depthBias == 0.5f);

    // No scene-graph sync happens in this CPU-only test, so the content's node
    // subtree is not materialized: the handle stays null (the backend skips it).
    REQUIRE(renderSlots.at(0).rootNodeHandle == 0);
}

TEST_CASE("cwRenderBillboards: updateBillboard changes the snapshot", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    content->setSize(QSizeF(10, 10));

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    billboard.worldPosition = QVector3D(0, 0, 0);
    const cwBillboardId id = billboards.addBillboard(billboard);

    billboard.worldPosition = QVector3D(5, 6, 7);
    billboards.updateBillboard(id, billboard);

    const auto renderSlots = billboards.buildRenderSlots();
    REQUIRE(renderSlots.size() == 1);
    REQUIRE(renderSlots.at(0).id == id);
    REQUIRE(renderSlots.at(0).worldPosition == QVector3D(5, 6, 7));
}

TEST_CASE("cwRenderBillboards: sizeMode and depthBias survive into the snapshot", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    content->setSize(QSizeF(10, 10));

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    billboard.sizeMode = cwRenderBillboards::SizeMode::WorldSized;
    billboard.depthBias = 2.0f;
    const cwBillboardId id = billboards.addBillboard(billboard);

    auto renderSlots = billboards.buildRenderSlots();
    REQUIRE(renderSlots.size() == 1);
    REQUIRE(renderSlots.at(0).sizeMode == cwRenderBillboards::SizeMode::WorldSized);
    REQUIRE(renderSlots.at(0).depthBias == 2.0f);

    // Changing only depthBias must not be swallowed by updateBillboard's no-op
    // equality check (it compares sizeMode and depthBias too).
    billboard.depthBias = 5.0f;
    billboards.updateBillboard(id, billboard);
    renderSlots = billboards.buildRenderSlots();
    REQUIRE(renderSlots.at(0).depthBias == 5.0f);
}

TEST_CASE("cwRenderBillboards: invisible content is skipped", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    content->setSize(QSizeF(20, 20));

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    billboards.addBillboard(billboard);

    // Visible content produces a slot.
    REQUIRE(billboards.buildRenderSlots().size() == 1);

    // A hidden content item must NOT be drawn. The subscene renders the content
    // via its render node, which ignores the visible property, so buildRenderSlots
    // is where visibility is honoured — otherwise a completed, unselected lead
    // marker (visible == false) keeps drawing and overlaps live leads.
    content->setVisible(false);
    REQUIRE(billboards.buildRenderSlots().isEmpty());

    content->setVisible(true);
    REQUIRE(billboards.buildRenderSlots().size() == 1);
}

TEST_CASE("cwRenderBillboards: null content is skipped", "[cwRenderBillboards]")
{
    QQuickWindow window;
    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;  // content left null
    billboards.addBillboard(billboard);

    REQUIRE(billboards.buildRenderSlots().isEmpty());
}

TEST_CASE("cwBillboardSightLine pulls a billboard toward the viewer", "[cwRenderBillboards]")
{
    SECTION("orthographic offsets along the view axis, not toward the eye point")
    {
        // The pose that made station labels drown in the scraps (issue #645): the
        // turntable's plan view, whose eye is parked 50 m from the pivot no matter
        // how big the cave is, so stations sit far off-axis and past the eye plane.
        QMatrix4x4 view;
        view.translate(0.0f, 0.0f, -50.0f);

        const QMatrix4x4 projection = makeOrtho();
        const QMatrix4x4 viewProjection = projection * view;

        const QVector3D station(200.0f, -400.0f, 300.0f);
        // Past the eye plane (positive view-space z): here the old eye-ward offset
        // pushed the label deeper into the scraps instead of pulling it forward.
        REQUIRE(view.map(station).z() > 0.0f);

        const QVector3D biased =
                cwBillboardSightLine(view, projection).biasedPosition(station, kBias);

        // Exactly kBias closer to the viewer, however far off-axis the station is.
        REQUIRE_THAT(view.map(biased).z(),
                     WithinAbs(view.map(station).z() + kBias, kWorldEpsilon));

        // A parallel projection has no foreshortening to hide a sideways slide, so
        // the label must not move on screen at all.
        const QVector3D before = toNdc(viewProjection, station);
        const QVector3D after = toNdc(viewProjection, biased);
        REQUIRE_THAT(after.x(), WithinAbs(before.x(), kNdcEpsilon));
        REQUIRE_THAT(after.y(), WithinAbs(before.y(), kNdcEpsilon));
        REQUIRE(after.z() < before.z());  // nearer in depth, so it wins the depth test
    }

    SECTION("orthographic gives every station the same depth gain")
    {
        // Tilted off the world axes, so the offset can only come out right by
        // following the camera's own axis.
        QMatrix4x4 view;
        view.translate(0.0f, 0.0f, -50.0f);
        view.rotate(35.0f, 1.0f, 0.0f, 0.0f);
        view.rotate(20.0f, 0.0f, 1.0f, 0.0f);

        const cwBillboardSightLine sightLine(view, makeOrtho());

        // Offsetting toward the eye point diluted the gain to its axial component,
        // so a station far off-axis kept almost none of the bias.
        const QVector3D onAxis(0.0f, 0.0f, 0.0f);
        const QVector3D farOffAxis(500.0f, 500.0f, 0.0f);

        const QVector3D biasedOnAxis = sightLine.biasedPosition(onAxis, kBias);
        const QVector3D biasedOffAxis = sightLine.biasedPosition(farOffAxis, kBias);

        REQUIRE_THAT(view.map(biasedOnAxis).z(),
                     WithinAbs(view.map(onAxis).z() + kBias, kWorldEpsilon));
        REQUIRE_THAT(view.map(biasedOffAxis).z(),
                     WithinAbs(view.map(farOffAxis).z() + kBias, kWorldEpsilon));
    }

    SECTION("perspective follows the sight line to the eye")
    {
        const QVector3D eye(0.0f, 0.0f, 60.0f);
        QMatrix4x4 view;
        view.lookAt(eye, QVector3D(0, 0, 0), QVector3D(0, 1, 0));

        const QMatrix4x4 projection = makePerspective();
        const QMatrix4x4 viewProjection = projection * view;

        const QVector3D station(20.0f, -12.0f, 0.0f);
        const QVector3D biased =
                cwBillboardSightLine(view, projection).biasedPosition(station, kBias);

        // Straight down the sight line: exactly kBias nearer the eye, and the same
        // screen position (the perspective divide cancels the move).
        REQUIRE_THAT(double((eye - biased).length()),
                     WithinAbs(double((eye - station).length()) - kBias, kWorldEpsilon));

        const QVector3D before = toNdc(viewProjection, station);
        const QVector3D after = toNdc(viewProjection, biased);
        REQUIRE_THAT(after.x(), WithinAbs(before.x(), kNdcPerspectiveEpsilon));
        REQUIRE_THAT(after.y(), WithinAbs(before.y(), kNdcPerspectiveEpsilon));
        REQUIRE(after.z() < before.z());
    }

    SECTION("a projection mid-transition picks the side it is closer to")
    {
        // cwProjectionTransition scrubs ortho <-> perspective as an element-wise
        // matrix lerp, so the frames in between are neither projection. Each must
        // still choose a sight line, and it must flip once, halfway — not on the
        // last frame of the animation, where the label would jump after the view
        // has visually stopped moving.
        QMatrix4x4 view;
        view.translate(0.0f, 0.0f, -50.0f);

        const QMatrix4x4 ortho = makeOrtho();
        const QMatrix4x4 perspective = makePerspective();
        const auto blended = [&](float progress) {
            QMatrix4x4 result;
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 4; ++column) {
                    result(row, column) = ortho(row, column) * (1.0f - progress)
                                          + perspective(row, column) * progress;
                }
            }
            return result;
        };

        // Off-axis, so the two sight lines genuinely disagree.
        const QVector3D station(30.0f, -18.0f, 0.0f);
        const QVector3D alongAxis =
                cwBillboardSightLine(view, ortho).biasedPosition(station, kBias);
        const QVector3D towardEye =
                cwBillboardSightLine(view, perspective).biasedPosition(station, kBias);
        REQUIRE(alongAxis != towardEye);

        REQUIRE(cwBillboardSightLine(view, blended(0.4f)).biasedPosition(station, kBias)
                == alongAxis);
        REQUIRE(cwBillboardSightLine(view, blended(0.6f)).biasedPosition(station, kBias)
                == towardEye);
    }

    SECTION("no bias leaves the position alone")
    {
        QMatrix4x4 view;
        view.lookAt(QVector3D(0, 0, 60), QVector3D(0, 0, 0), QVector3D(0, 1, 0));

        const QVector3D station(3.0f, 4.0f, 5.0f);
        REQUIRE(cwBillboardSightLine(view, makeOrtho()).biasedPosition(station, 0.0f) == station);
        REQUIRE(cwBillboardSightLine(view, makePerspective()).biasedPosition(station, 0.0f)
                == station);
    }

    SECTION("a billboard sitting on the perspective eye is left alone")
    {
        // Built as a pure translation so the eye it inverts to is exact.
        const QVector3D eye(0.0f, 0.0f, 60.0f);
        QMatrix4x4 view;
        view.translate(-eye);

        const QMatrix4x4 projection = makePerspective();

        // No sight line to follow — the offset direction would be undefined.
        REQUIRE(cwBillboardSightLine(view, projection).biasedPosition(eye, kBias) == eye);
    }
}

TEST_CASE("cwSortBillboardSlotsBackToFront orders billboards farthest-first", "[cwRenderBillboards]")
{
    // Camera at +z=10 looking down -z, so smaller world z is farther away. The
    // sort must put the farthest billboard first regardless of insertion order,
    // under both projection kinds — the bite only depends on draw order vs. true
    // depth order, which is what this verifies.
    QMatrix4x4 view;
    view.lookAt(QVector3D(0, 0, 10), QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    SECTION("orthographic")
    {
        QMatrix4x4 projection;
        projection.ortho(-10, 10, -10, 10, 0.1f, 100);
        const QMatrix4x4 viewProjection = projection * view;

        QVector<cwRenderBillboards::RenderSlot> renderSlots{
            makeSlot(1, QVector3D(0, 0, 5)),    // nearest
            makeSlot(2, QVector3D(0, 0, -5)),   // farthest
            makeSlot(3, QVector3D(0, 0, 0)),    // middle
        };
        cwSortBillboardSlotsBackToFront(renderSlots, viewProjection);

        REQUIRE(renderSlots.size() == 3);
        REQUIRE(renderSlots.at(0).id == cwBillboardId{2});
        REQUIRE(renderSlots.at(1).id == cwBillboardId{3});
        REQUIRE(renderSlots.at(2).id == cwBillboardId{1});
    }

    SECTION("perspective")
    {
        QMatrix4x4 projection;
        projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
        const QMatrix4x4 viewProjection = projection * view;

        QVector<cwRenderBillboards::RenderSlot> renderSlots{
            makeSlot(1, QVector3D(2, 0, 0)),     // middle (off-axis)
            makeSlot(2, QVector3D(-3, 0, -8)),   // farthest
            makeSlot(3, QVector3D(1, 0, 7)),     // nearest
        };
        cwSortBillboardSlotsBackToFront(renderSlots, viewProjection);

        REQUIRE(renderSlots.size() == 3);
        REQUIRE(renderSlots.at(0).id == cwBillboardId{2});
        REQUIRE(renderSlots.at(1).id == cwBillboardId{1});
        REQUIRE(renderSlots.at(2).id == cwBillboardId{3});
    }

    SECTION("equal depth resolves to a deterministic id order")
    {
        QMatrix4x4 projection;
        projection.ortho(-10, 10, -10, 10, 0.1f, 100);
        const QMatrix4x4 viewProjection = projection * view;

        // Same depth plane, different x, fed in non-id order. Equal-depth ties
        // resolve by ascending id (a total order), so the result is deterministic
        // regardless of incoming order — no reliance on a stable sort.
        QVector<cwRenderBillboards::RenderSlot> renderSlots{
            makeSlot(12, QVector3D(4, 0, 2)),
            makeSlot(10, QVector3D(-4, 0, 2)),
            makeSlot(11, QVector3D(0, 0, 2)),
        };
        cwSortBillboardSlotsBackToFront(renderSlots, viewProjection);

        REQUIRE(renderSlots.size() == 3);
        REQUIRE(renderSlots.at(0).id == cwBillboardId{10});
        REQUIRE(renderSlots.at(1).id == cwBillboardId{11});
        REQUIRE(renderSlots.at(2).id == cwBillboardId{12});
    }

    SECTION("fewer than two slots is a no-op")
    {
        QVector<cwRenderBillboards::RenderSlot> empty;
        cwSortBillboardSlotsBackToFront(empty, QMatrix4x4());
        REQUIRE(empty.isEmpty());

        QVector<cwRenderBillboards::RenderSlot> one{makeSlot(7, QVector3D(0, 0, 0))};
        cwSortBillboardSlotsBackToFront(one, QMatrix4x4());
        REQUIRE(one.size() == 1);
        REQUIRE(one.at(0).id == cwBillboardId{7});
    }
}

TEST_CASE("cwRenderBillboards: clearing billboards releases subscenes", "[cwRenderBillboards]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());

    cwRenderBillboards billboards;
    billboards.setWindow(&window);

    cwRenderBillboards::Billboard billboard;
    billboard.content = content.get();
    const cwBillboardId id = billboards.addBillboard(billboard);
    REQUIRE(billboards.buildRenderSlots().size() == 1);

    // Removing the billboard must deref its subscene without crashing.
    billboards.removeBillboard(id);
    REQUIRE(billboards.billboardCount() == 0);
    REQUIRE(billboards.buildRenderSlots().isEmpty());

    // The content item outlives the render object cleanly.
    REQUIRE(content != nullptr);
}
