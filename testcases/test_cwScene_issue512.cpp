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

#include <catch2/catch_test_macros.hpp>

#include <new>

#include "cwRHIObject.h"
#include "cwRenderObject.h"
#include "cwRhiScene.h"
#include "cwScene.h"

// Friend accessor (declared friend in cwRhiScene.h) so the test can drive the
// backend's private apply step — normally called only by cwRhiItemRenderer —
// without putting test scaffolding on the production API.
struct CwRhiSceneTestAccess {
    static void synchroize(cwRhiScene& rhiScene, cwScene* scene) {
        rhiScene.synchroize(scene, nullptr);
    }
};

namespace {

// Counts live stub RHI objects. The whole point of the bug is an orphaned
// cwRHIObject that outlives its render object, so a simple alive-count is the
// fix-agnostic observable: any correct add/remove path leaves exactly one.
int g_aliveRhiObjects = 0;

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

protected:
    cwRHIObject* createRHIObject() override { return new StubRhiObject(); }
};

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
    // thrash does); synchroize() leaves a synced object in none of the live
    // queues, so removeItem() would otherwise have nothing to move to delete.
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

    // Toggle visibility so the synced object lands back in a live queue, then
    // remove it — queuing its id while RHI #1 stays mapped under that id.
    object->setVisible(false);
    scene.removeItem(object);

    // Re-add the *same* object before syncing: addItem() cancels the pending
    // delete, so only the add-loop guard can free RHI #1.
    scene.addItem(object);
    object->setParent(nullptr);

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // must free RHI #1, make RHI #2

    REQUIRE(g_aliveRhiObjects == 1);        // 2 would mean RHI #1 was orphaned

    delete object;
}
