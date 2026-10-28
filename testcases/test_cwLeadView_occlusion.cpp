// Pins cwLeadView::isOccluded — the gate LeadPoint.qml uses to decide whether a
// tap on a lead marker counts. A lead renders as an overlay billboard regardless
// of depth, so without this gate you could click a lead through a cave wall.
//
// isOccluded needs only a camera and a scene, so it runs here on the GUI thread
// with no window, QRhi, or QML engine: the BVH is CPU-side and the billboard
// plane is solved analytically.
//
// The sprite-slot cases at the end build a LeadView in QML over a one-lead
// region, because LeadPoint markers come from QML. They read each lead's
// instance back through the scene's sprite layer, still without a QRhi.

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Qt
#include <QMatrix4x4>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlListReference>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRay3D>
#include <QRect>
#include <QSignalSpy>
#include <QVector3D>

// Std
#include <memory>

// SUT
#include "cwCamera.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwLead.h"
#include "cwNote.h"
#include "cwRegionTreeModel.h"
#include "cwRenderSpriteBillboards.h"
#include "cwScrap.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "cwGeometry.h"
#include "cwGeometryItersecter.h"
#include "TestGeometryBuilders.h"
#include "cwLeadView.h"
#include "cwScene.h"
#include "cwSelectionManager.h"

using namespace Catch;

namespace {

constexpr int kViewportWidth = 800;
constexpr int kViewportHeight = 600;

// The camera looks down -Z, so a LARGER z is NEARER the eye. Every depth claim
// below rests on that; requireLookingStraightDownNegativeZ asserts it rather
// than trusting the default view matrix to stay identity.
constexpr float kLeadZ = 0.0f;

// How far in front of the lead the cloud point sits. Must clear the eye-ward
// slack isOccluded allows (kLeadDepthBias, 1.0 world units, file-local to
// cwLeadView.cpp) by enough that no test here is deciding a boundary case —
// 50 units is comfortably past it.
constexpr float kCloudInFrontOfLead = 50.0f;
constexpr float kCloudZ = kLeadZ + kCloudInFrontOfLead;

// The pick sphere the cloud is registered with. Any value works — the offset
// below is expressed as a multiple of it — so this is just a round number.
constexpr float kPickRadius = 1.0f;

// Perpendicular distance from the ray to the off-ray cloud point. Outside the
// pick sphere, so the ray genuinely misses the point and nothing the user can
// see covers the lead. 1.5x rather than a comfortable 10x because 1.5x is the
// distance that used to fail: the deleted near-miss fallback promoted any point
// within 2.5 radii, so this is the band a reintroduced fallback would break
// first.
constexpr float kOffRayDistance = kPickRadius * 1.5f;

static_assert(kOffRayDistance > kPickRadius,
              "The off-ray point must fall outside its own pick sphere, or the "
              "ray really does hit it and the lead is legitimately occluded — "
              "which would make this test pass while guarding nothing.");


struct Fixture
{
    cwCamera camera;
    cwScene scene;
    cwLeadView leadView;

    Fixture()
    {
        camera.setViewport(QRect(0, 0, kViewportWidth, kViewportHeight));
        camera.setProjection(camera.orthoProjectionDefault());
        leadView.setCamera(&camera);
        leadView.setScene(&scene);
    }
};

// One cloud point, registered the way a LAZ layer registers its cloud:
// Type::Points with a pick radius (production derives that radius from
// meanSpacingXY * cwRenderPointCloud::PointPickRadiusScale; nothing here
// depends on it doing so).
void addCloudPoint(cwScene& scene, const QVector3D& position, uint64_t id = 1)
{
    scene.geometryItersecter()->addObject(
        cwGeometryItersecter::Object(nullptr, id, cwTestGeometry::points({position}),
                                     QMatrix4x4(), kPickRadius));
    scene.geometryItersecter()->waitForFinish();
}

// Guards the sign convention every depth claim in this file rests on, AND
// rayPointAtZ's precondition. Pinning the exact direction rather than just
// `z() < 0`: a 45-degree camera would satisfy the sign yet make rayPointAtZ
// return the wrong depth, and the hit case below cannot notice (rayPointAtZ
// always returns a point ON the ray, so its distance check passes regardless).
void requireLookingStraightDownNegativeZ(const QRay3D& ray)
{
    REQUIRE(ray.direction().x() == Approx(0.0f).margin(1e-6));
    REQUIRE(ray.direction().y() == Approx(0.0f).margin(1e-6));
    REQUIRE(ray.direction().z() == Approx(-1.0f).margin(1e-6));
}

// The point on `ray` at world depth z. Only valid while the camera looks
// straight down -Z (assert that first) — which keeps the cloud points below
// placed relative to the ray the camera ACTUALLY casts rather than to an
// assumed x=0. A tap pixel is rounded to an integer, and one pixel is half a
// world unit at this zoom, so an assumed-centre ray would shift the offsets by
// a third of the band they have to land in.
QVector3D rayPointAtZ(const QRay3D& ray, float z)
{
    return ray.point(ray.origin().z() - z);
}

} // namespace

TEST_CASE("cwLeadView does not occlude a lead with a cloud point the ray misses",
          "[cwLeadView][occlusion]")
{
    // A lead sits in open space with one cloud point off to the side, near the
    // eye. The ray through the tapped pixel passes 1.5 pick radii from that
    // point — it misses the sphere, and on screen the splat does not cover the
    // marker. Nothing occludes the lead, so the tap must select it.
    //
    // This failed before cwGeometryItersecter's near-miss fallback was deleted:
    // that fallback fired whenever no exact hit existed anywhere, snapping to
    // the nearest point within 2.5 pick radii and reporting its CENTRE depth.
    // isOccluded is unusually exposed to it — it picks cwPickQuery::Solid with
    // no tolerance, and neither triangles nor points consult the tolerance, so
    // the fallback was the only near-miss path reachable here and had nothing
    // to lose to. The promoted point landed 50 units in front of the lead, the
    // depth compare said "covered", and the lead silently stopped responding to
    // clicks while looking perfectly visible.
    Fixture f;

    const QVector3D leadPosition(0.0f, 0.0f, kLeadZ);
    const QPointF tap = f.camera.project(leadPosition);
    const QRay3D ray = f.camera.rayFromQtViewport(tap);
    requireLookingStraightDownNegativeZ(ray);

    const QVector3D cloudPoint =
        rayPointAtZ(ray, kCloudZ) + QVector3D(kOffRayDistance, 0.0f, 0.0f);
    addCloudPoint(f.scene, cloudPoint);

    // Pin the premise rather than trusting the arithmetic that built it: the
    // ray must pass kOffRayDistance from the point, which the static_assert
    // above has already established is outside the sphere.
    REQUIRE(ray.distance(cloudPoint) == Approx(kOffRayDistance).margin(1e-4));

    CHECK_FALSE(f.leadView.isOccluded(leadPosition, tap));
}

TEST_CASE("cwLeadView occludes a lead behind a cloud point the ray hits",
          "[cwLeadView][occlusion]")
{
    // The other half, and the reason the case above can't be fixed by dropping
    // point clouds from the occlusion query: a lead genuinely behind the cloud
    // must stay unclickable. Same geometry as above with the point moved onto
    // the ray, so this is an exact sphere hit.
    Fixture f;

    const QVector3D leadPosition(0.0f, 0.0f, kLeadZ);
    const QPointF tap = f.camera.project(leadPosition);
    const QRay3D ray = f.camera.rayFromQtViewport(tap);
    requireLookingStraightDownNegativeZ(ray);

    const QVector3D cloudPoint = rayPointAtZ(ray, kCloudZ);
    addCloudPoint(f.scene, cloudPoint);

    REQUIRE(ray.distance(cloudPoint) == Approx(0.0).margin(1e-4));

    CHECK(f.leadView.isOccluded(leadPosition, tap));
}

TEST_CASE("cwLeadView does not occlude a lead in an empty scene",
          "[cwLeadView][occlusion]")
{
    // The floor the two cases above stand on: with no geometry at all the
    // answer must be "not occluded". Guards the no-hit and no-BVH early-outs
    // (cwLeadView.cpp's !hit.hit() return, and the intersecter's null-BVH
    // path), which nothing else here reaches. It does NOT catch an isOccluded
    // stubbed to always-false — the occludes case above is what does that.
    Fixture f;

    const QVector3D leadPosition(0.0f, 0.0f, kLeadZ);
    const QPointF tap = f.camera.project(leadPosition);

    CHECK_FALSE(f.leadView.isOccluded(leadPosition, tap));
}

namespace {

constexpr int kSpriteViewportSize = 400;
const QSizeF kOpenSpriteSize(20.0, 20.0);
const QSizeF kCompletedSpriteSize(30.0, 12.0);

// The one lead's LeadPoint: the first scrap's id is 1 and the lead's index 0.
const QString kLeadPointName = QStringLiteral("leadPoint1_0");

// A LeadView whose sprites list is @a spritesList, built from the openSprite
// and completedSprite items it declares.
QString leadViewQml(const QString& spritesList)
{
    return QStringLiteral(
               "import QtQuick\n"
               "import cavewherelib\n"
               "Item {\n"
               "  width: %1; height: %1\n"
               "  LeadView {\n"
               "    objectName: \"leadView\"\n"
               "    anchors.fill: parent\n"
               "    sprites: %2\n"
               "    Item { id: openSprite; width: %3; height: %4 }\n"
               "    Item { id: completedSprite; width: %5; height: %6 }\n"
               "  }\n"
               "}\n")
        .arg(kSpriteViewportSize)
        .arg(spritesList)
        .arg(kOpenSpriteSize.width())
        .arg(kOpenSpriteSize.height())
        .arg(kCompletedSpriteSize.width())
        .arg(kCompletedSpriteSize.height());
}

// One cave, trip, note and scrap holding one lead, under a region tree model,
// with a QML LeadView over it in a window that is never shown.
struct SpriteFixture
{
    cwCavingRegion region;
    cwRegionTreeModel regionModel;
    cwScene scene;
    cwCamera camera;
    QQmlEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> root;
    cwLeadView* leadView = nullptr;

    explicit SpriteFixture(const QString& spritesList)
    {
        regionModel.setCavingRegion(&region);
        auto* cave = new cwCave();
        region.addCave(cave);
        auto* trip = new cwTrip();
        cave->addTrip(trip);

        auto* scrap = new cwScrap();
        cwLead lead;
        lead.setDescription(QStringLiteral("Sprite slot lead"));
        lead.setPositionOnNote(QPointF(0.5, 0.5));
        scrap->addLead(lead);
        auto* note = new cwNote();
        note->addScrap(scrap);
        trip->notes()->addNotes({note});

        camera.setViewport(QRect(0, 0, kSpriteViewportSize, kSpriteViewportSize));
        camera.setProjection(camera.orthoProjectionDefault());
        window.resize(kSpriteViewportSize, kSpriteViewportSize);

        QQmlComponent component(&engine);
        component.setData(leadViewQml(spritesList).toUtf8(), QUrl());
        root.reset(component.create());
        INFO(component.errorString().toStdString());
        REQUIRE(root != nullptr);
        auto* rootItem = qobject_cast<QQuickItem*>(root.get());
        REQUIRE(rootItem != nullptr);
        rootItem->setParentItem(window.contentItem());

        leadView = root->findChild<cwLeadView*>(QStringLiteral("leadView"));
        REQUIRE(leadView != nullptr);
        REQUIRE(leadView->window() == &window);

        leadView->setScene(&scene);
        leadView->setCamera(&camera);
        leadView->setRegionModel(&regionModel);
    }

    QQuickItem* marker() const
    {
        auto* leadPoint = leadView->findChild<QQuickItem*>(kLeadPointName);
        if (leadPoint == nullptr) {
            return nullptr;
        }
        return leadPoint->property("marker").value<QQuickItem*>();
    }
};

} // namespace

TEST_CASE("cwLeadView names its sprite slots with the LeadSprite enum", "[cwLeadView][sprites]")
{
    CHECK(int(cwLeadView::OpenSprite) == 0);
    CHECK(int(cwLeadView::CompletedSprite) == 1);
}

TEST_CASE("cwLeadView hands its sprites list to the scene's sprite layer", "[cwLeadView][sprites]")
{
    SpriteFixture f(QStringLiteral("[openSprite, completedSprite]"));

    CHECK(QQmlListReference(f.leadView, "sprites").count() == 2);
    CHECK(f.leadView->spriteSize() == kOpenSpriteSize);

    cwRenderSpriteBillboards* layer = f.scene.spriteBillboardLayer();
    CHECK(layer->spriteCount() == 2);
    CHECK(layer->spriteSize() == kOpenSpriteSize);

    QQuickItem* marker = f.marker();
    REQUIRE(marker != nullptr);
    CHECK(marker->size() == kOpenSpriteSize);
    CHECK(marker->property("spriteIndex").toInt() == int(cwLeadView::OpenSprite));

    auto instances = layer->buildInstances();
    REQUIRE(instances.size() == 1);
    CHECK(instances.at(0).spriteIndex == uint32_t(cwLeadView::OpenSprite));

    // The marker's spriteIndex picks the lead's slot as it changes.
    REQUIRE(marker->setProperty("spriteIndex", int(cwLeadView::CompletedSprite)));
    instances = layer->buildInstances();
    REQUIRE(instances.size() == 1);
    CHECK(instances.at(0).spriteIndex == uint32_t(cwLeadView::CompletedSprite));
}

TEST_CASE("cwLeadView drops a lead whose slot the sprites list lacks", "[cwLeadView][sprites]")
{
    SpriteFixture f(QStringLiteral("[openSprite]"));

    cwRenderSpriteBillboards* layer = f.scene.spriteBillboardLayer();
    CHECK(layer->spriteCount() == 1);

    QQuickItem* marker = f.marker();
    REQUIRE(marker != nullptr);
    REQUIRE(layer->buildInstances().size() == 1);

    REQUIRE(marker->setProperty("spriteIndex", int(cwLeadView::CompletedSprite)));
    CHECK(layer->billboardCount() == 1);
    CHECK(layer->buildInstances().isEmpty());
}

TEST_CASE("cwLeadView hands the layer an empty list when it goes", "[cwLeadView][sprites]")
{
    SpriteFixture f(QStringLiteral("[openSprite, completedSprite]"));

    cwRenderSpriteBillboards* layer = f.scene.spriteBillboardLayer();
    CHECK(layer->spriteCount() == 2);

    f.root.reset();
    CHECK(layer->spriteCount() == 0);
    CHECK(layer->sprites().isEmpty());
}

TEST_CASE("cwLeadView takes a settable selection manager and hands it to its points",
          "[cwLeadView][selection]")
{
    cwSelectionManager selection;
    SpriteFixture f(QStringLiteral("[openSprite, completedSprite]"));
    auto* leadPoint = f.leadView->findChild<QQuickItem*>(kLeadPointName);
    REQUIRE(leadPoint != nullptr);

    QSignalSpy changedSpy(f.leadView, &cwLeadView::selectionManagerChanged);
    f.leadView->setSelectionManager(&selection);
    CHECK(changedSpy.count() == 1);
    CHECK(f.leadView->selectionManager() == &selection);
    CHECK(leadPoint->property("selectionManager").value<cwSelectionManager*>() == &selection);

    f.leadView->setSelectionManager(&selection);
    CHECK(changedSpy.count() == 1);
}

TEST_CASE("cwLeadView keeps another marker's selection when an unselected lead goes",
          "[cwLeadView][selection]")
{
    cwSelectionManager selection;
    // Stands in for a marker of another view sharing the manager, such as an
    // entrance point.
    QQuickItem otherMarker;
    SpriteFixture f(QStringLiteral("[openSprite, completedSprite]"));
    f.leadView->setSelectionManager(&selection);

    cwScrap* scrap = f.region.cave(0)->trip(0)->notes()->notes().at(0)->scraps().at(0);
    cwLead lead;
    lead.setDescription(QStringLiteral("Second lead"));
    lead.setPositionOnNote(QPointF(0.25, 0.25));
    scrap->addLead(lead);
    auto* secondLeadPoint = f.leadView->findChild<QQuickItem*>(QStringLiteral("leadPoint1_1"));
    REQUIRE(secondLeadPoint != nullptr);
    CHECK(secondLeadPoint->property("selectionManager").value<cwSelectionManager*>() == &selection);

    selection.setSelectedItem(&otherMarker);
    scrap->removeLead(1);
    CHECK(selection.selectedItem() == &otherMarker);

    auto* leadPoint = f.leadView->findChild<QQuickItem*>(kLeadPointName);
    REQUIRE(leadPoint != nullptr);
    selection.setSelectedItem(leadPoint);
    scrap->removeLead(0);
    CHECK(selection.selectedItem() == nullptr);
}
