// Reproducer for issue #512: a freshly-allocated cwRenderObject can reuse the
// address of one already queued for deletion. When identity was the raw pointer,
// cwScene::addItem then cancelled the dead object's pending delete and its
// cwRHIObject was orphaned in cwRhiScene's m_rhiObjects / m_rhiObjectLookup,
// rendered for the rest of the scene's life.
//
// The fix gives each cwRenderObject a stable, never-reused renderObjectId() and
// keys every queue / lookup on it, so a reused address can't masquerade as the
// dead object — the two get independent ids and independent RHI objects.
//
// The test drives the backend apply step (cwRhiScene::synchroize) directly with
// stub render/RHI objects that touch no GPU, so it is fully CPU-only and
// deterministic. Address reuse is forced with placement-new into storage we
// own, mirroring the allocator reuse seen in the 2026-05-18 LAZ-clip log.
//
// Regression guard: after reusing the address, exactly one cwRHIObject stays
// alive and the two render objects carry distinct ids.
//
// The stub scene/backend pair built here is the cheapest way to drive that
// handoff, so this file has since become the home for cwScene's RHI-object
// lifetime coverage generally — not just #512: a plain steady-state removal, the
// real LAZ-layer removal path, and — since the single-map handoff refactor — that
// Adds register in add order (draw order) and that a removed object cannot be
// resurrected by a late updateItem(). Only the two named #512 cases are about
// address reuse.

#include <catch2/catch_test_macros.hpp>

#include <new>

#include "cwGeometry.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLazLayersSceneNode.h"
#include "cwRHIObject.h"
#include "cwRenderObject.h"
#include "cwRenderPointCloud.h"
#include "cwRhiScene.h"
#include "cwScene.h"

#include <QCoreApplication>
#include <QDir>
#include <QList>
#include <QPointer>
#include <QTemporaryDir>
#include <QVector3D>

#include "LazFixtureHelper.h"

// Friend accessor (declared friend in cwRhiScene.h) so the test can drive the
// backend's private apply step — normally called only by cwRhiItemRenderer —
// without putting test scaffolding on the production API.
struct CwRhiSceneTestAccess {
    static void synchroize(cwRhiScene& rhiScene, cwScene* scene) {
        rhiScene.synchroize(scene, nullptr);
    }

    // Reaches the backend's registry so a test can ask whether an id still maps to
    // a live cwRHIObject. The stub tests count constructions instead; this is for
    // the real render objects, which the tests must not instrument.
    static cwRHIObject* renderObjectForId(cwRhiScene& rhiScene, cwRenderObjectId id) {
        return rhiScene.m_frame.renderObjectForId(id);
    }
};

namespace {

// Counts live stub RHI objects. The whole point of the bug is an orphaned
// cwRHIObject that outlives its render object, so a simple alive-count is the
// fix-agnostic observable: any correct add/remove path leaves exactly one.
int g_aliveRhiObjects = 0;

// Records the label of each stub render object as its cwRHIObject is created, i.e.
// in the order synchroize() registers Adds — which is the order they draw. Used to
// pin that Adds register in add order, not construction order.
QVector<char> g_registrationOrder;

class StubRhiObject : public cwRHIObject
{
public:
    StubRhiObject() { ++g_aliveRhiObjects; }
    ~StubRhiObject() override { --g_aliveRhiObjects; }

    // No GPU work — every override is a no-op so synchroize() needs no QRhi.
    void initialize(const ResourceUpdateData&) override {}
    void synchronize(const SynchronizeData&) override {}
    void updateResources(const ResourceUpdateData&) override {}
    void render(const RenderData&) override {}
};

class StubRenderObject : public cwRenderObject
{
public:
    using cwRenderObject::cwRenderObject;

    void setLabel(char label) { m_label = label; }

protected:
    cwRHIObject* createRHIObject() override
    {
        if (m_label != '\0') {
            g_registrationOrder.append(m_label);
        }
        return new StubRhiObject();
    }

private:
    char m_label = '\0';
};

// A 3x3 grid in the z = 0 plane, one unit apart — the smallest cloud that still
// loads through the real LAZ pipeline.
constexpr float kGridSpacing = 1.0f;
constexpr int kGridSide = 3;

QVector<QVector3D> gridPoints()
{
    QVector<QVector3D> points;
    for (int x = 0; x < kGridSide; ++x) {
        for (int y = 0; y < kGridSide; ++y) {
            points.append(QVector3D((x - 1) * kGridSpacing, (y - 1) * kGridSpacing, 0.0f));
        }
    }
    return points;
}

QDir prepareGisLayersDir(const QTemporaryDir& tempDir)
{
    const QString path = QDir(tempDir.path()).filePath(cwLazLayerModel::folderName());
    QDir().mkpath(path);
    return QDir(path);
}

} // namespace

TEST_CASE("issue #512: reusing a deleted render object's address must not orphan "
          "its cwRHIObject",
          "[cwScene][cwRhiScene][issue512]")
{
    g_aliveRhiObjects = 0;

    cwScene scene;
    cwRhiScene rhiScene;

    // Storage we own, so two logically-distinct render objects can be built at
    // the *same* address deterministically.
    void* storage = ::operator new(sizeof(StubRenderObject));

    // --- Logical object #1 ---
    auto* first = new (storage) StubRenderObject();
    first->setScene(&scene);
    first->setParent(nullptr);   // we manage its lifetime, not the scene

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // creates RHI object #1
    REQUIRE(g_aliveRhiObjects == 1);

    // A visibility toggle re-queues #1 for update (as the real LAZ visibility
    // thrash does), so removeItem() has a live queue entry to scrub. #1 is
    // destroyed below before the next sync, so if that scrub ever regressed,
    // synchroize()'s update loop would dereference freed memory (issue #491).
    first->setVisible(false);
    scene.removeItem(first);                // queues #1 for delete; RHI #1 still live

    const void* reusedAddress = static_cast<void*>(first);
    const cwRenderObjectId firstId = first->renderObjectId();  // capture before destroy
    first->~StubRenderObject();             // destroy logical #1; RHI #1 still alive

    // --- Logical object #2, reusing #1's address ---
    auto* second = new (storage) StubRenderObject();
    REQUIRE(static_cast<void*>(second) == reusedAddress);

    // Same address, but a distinct id — this is what keeps addItem() from
    // cancelling #1's still-pending delete and orphaning RHI #1.
    REQUIRE(second->renderObjectId() != firstId);

    second->setScene(&scene);
    second->setParent(nullptr);

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // creates RHI object #2

    // Only one render object exists now (second), so the backend should hold
    // exactly one cwRHIObject. The bug overwrites #1's lookup entry with #2's
    // and never frees RHI #1, so it stays orphaned in m_rhiObjects -> two alive.
    REQUIRE(g_aliveRhiObjects == 1);        // FAILS today: == 2

    second->~StubRenderObject();
    ::operator delete(storage);
}

// The plain steady-state removal: an object that has been synced sits in neither
// queue, because synchroize() drains both every frame. removeItem() used to read
// that emptiness as "nothing to do" and return before queueing the delete, so the
// backend kept the cwRHIObject — and, for a LAZ layer, its multi-gigabyte vertex
// buffer — for the life of the scene. Every removal the running app performs takes
// this path, yet nothing covered it: the test above dodges it by toggling
// visibility first, which puts the object back in a queue.
TEST_CASE("removing a synced render object frees its cwRHIObject",
          "[cwScene][cwRhiScene]")
{
    g_aliveRhiObjects = 0;

    cwScene scene;
    cwRhiScene rhiScene;

    auto* object = new StubRenderObject();
    object->setScene(&scene);
    object->setParent(nullptr);   // we manage its lifetime, not the scene

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // creates RHI object #1
    REQUIRE(g_aliveRhiObjects == 1);

    // No visibility toggle: this is a bare removal of a synced object, so both
    // of removeItem()'s queues are empty when it runs.
    scene.removeItem(object);
    delete object;

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // must free RHI #1

    REQUIRE(g_aliveRhiObjects == 0);        // 1 would mean RHI #1 was stranded
}

// The same steady-state removal as above, driven through the real path a user
// takes: removing a LAZ layer. cwLazLayersSceneNode::dematerialize() does
// setScene(nullptr) then delete, with no visibility toggle anywhere — so this is
// what the running app actually did every time a layer was removed, and what
// stranded a cwRHIPointCloud holding the layer's whole vertex buffer.
//
// Real render objects here, not stubs: the tests must not instrument production
// classes, so the observable is the backend's own registry — an id that still
// resolves to a cwRHIObject after the object is gone is the leak.
TEST_CASE("removing a LAZ layer frees its point cloud's cwRHIObject",
          "[cwScene][cwRhiScene][cwLazLayersSceneNode]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwScene scene;
    cwRhiScene rhiScene;

    cwLazLayersSceneNode node;
    node.setScene(&scene);

    cwLazLayerModel model;
    node.setLazLayerModel(&model);

    const QDir gisLayersDir = prepareGisLayersDir(tempDir);
    const QString filePath = gisLayersDir.filePath(QStringLiteral("rhi-lifetime-%1.laz")
                                                       .arg(QCoreApplication::applicationPid()));
    REQUIRE(writeSyntheticLazFile(filePath, gridPoints()));
    model.setGisLayersDir(gisLayersDir);
    model.rescan();

    cwLazLayer* layer = model.layerAt(model.count() - 1);
    REQUIRE(layer != nullptr);
    REQUIRE(waitForLazLayerLoaded(layer));

    cwRenderPointCloud* cloud = node.pointCloudForLayer(layer);
    REQUIRE(cloud != nullptr);

    // Captured while the cloud is alive; the id outlives it by design, which is
    // exactly what lets the delete be queued without dereferencing the pointer.
    const cwRenderObjectId id = cloud->renderObjectId();

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // creates the cwRHIPointCloud
    REQUIRE(CwRhiSceneTestAccess::renderObjectForId(rhiScene, id) != nullptr);

    QPointer<cwRenderPointCloud> guard(cloud);
    model.removeAt(0);
    REQUIRE(guard.isNull());

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // must destroy it

    REQUIRE(CwRhiSceneTestAccess::renderObjectForId(rhiScene, id) == nullptr);
}

TEST_CASE("issue #512: re-adding the same render object before a sync must free "
          "its previous cwRHIObject",
          "[cwScene][cwRhiScene][issue512]")
{
    // Drives the synchroize() add-loop guard directly. An already-synced object
    // removed then re-added before the next sync keeps its id, so its old
    // cwRHIObject is still mapped under that id; addItem() cancels the pending
    // delete, so the delete loop won't free it. The add-loop must spot the live
    // lookup entry and free it before remapping — otherwise the old cwRHIObject
    // orphans in m_rhiObjects (the #512 leak via the legitimate re-add path).
    g_aliveRhiObjects = 0;

    cwScene scene;
    cwRhiScene rhiScene;

    auto* object = new StubRenderObject();
    object->setScene(&scene);
    object->setParent(nullptr);   // we manage its lifetime, not the scene

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // creates RHI object #1
    REQUIRE(g_aliveRhiObjects == 1);

    // Remove it, queuing its id while RHI #1 stays mapped under that id.
    scene.removeItem(object);

    // Re-add the *same* object before syncing: addItem() cancels the pending
    // delete, so only the add-loop guard can free RHI #1.
    scene.addItem(object);
    object->setParent(nullptr);

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // must free RHI #1, make RHI #2

    REQUIRE(g_aliveRhiObjects == 1);        // 2 would mean RHI #1 was orphaned

    delete object;
}

// Draw order follows add order, not construction order. gatherScene() walks the
// backend's render list in registration order and bakes that index into the sort
// key, so the sync drain must register Adds in the order addItem() ran — even when
// the objects were constructed in a different order. The pending map is keyed on the
// render-object id, which is a construction-order counter; a drain that leaned on
// that key order would silently swap these two and change what draws on top. The
// queued sequence is what holds add order, and this test is its guard.
TEST_CASE("added render objects register in add order, not construction order",
          "[cwScene][cwRhiScene]")
{
    g_aliveRhiObjects = 0;
    g_registrationOrder.clear();

    cwScene scene;
    cwRhiScene rhiScene;

    // Construct A before B (so A's id < B's id), then add B before A.
    auto* a = new StubRenderObject();
    auto* b = new StubRenderObject();
    a->setLabel('A');
    b->setLabel('B');
    REQUIRE(a->renderObjectId() < b->renderObjectId());

    b->setScene(&scene);
    b->setParent(nullptr);   // we manage lifetime, not the scene
    a->setScene(&scene);
    a->setParent(nullptr);

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);

    // Registration (== draw) order is the add order B, A — not the id order A, B.
    REQUIRE(g_registrationOrder == QVector<char>({'B', 'A'}));

    delete a;
    delete b;
}

// A removed object stays removed even if updateItem() arrives for it before the next
// sync. The transition model records {Delete}, and updateItem() must not resurrect it
// into a live-pointer entry that the next synchroize() would then dereference — the
// #491 failure mode, now unrepresentable. Pre-refactor this left the freed pointer in
// the update queue: pendingItemCount() read 1 and the next sync dereferenced it.
TEST_CASE("updateItem on a removed render object does not resurrect it",
          "[cwScene][cwRhiScene]")
{
    g_aliveRhiObjects = 0;

    cwScene scene;
    cwRhiScene rhiScene;

    auto* object = new StubRenderObject();
    object->setScene(&scene);
    object->setParent(nullptr);   // we manage its lifetime, not the scene

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // creates RHI object #1
    REQUIRE(g_aliveRhiObjects == 1);

    scene.removeItem(object);     // {Delete}
    scene.updateItem(object);     // must not put the live pointer back

    // Delete holds no live pointer, so nothing the next sync will dereference.
    REQUIRE(scene.pendingItemCount() == 0);

    delete object;
    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // frees RHI #1, no deref

    REQUIRE(g_aliveRhiObjects == 0);
}
