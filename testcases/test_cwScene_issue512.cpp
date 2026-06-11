// Reproducer for issue #512: cwScene::addItem cancels a pending delete when a
// freshly-allocated cwRenderObject* reuses the address of one already queued
// for deletion. Because the address is reused, the old cwRHIObject in
// cwRhiScene's m_rhiObjects / m_rhiObjectLookup is never freed — its lookup
// entry is silently overwritten by the new object's, orphaning it in the
// render list for the rest of the scene's life.
//
// The test drives the backend apply step (cwRhiScene::synchroize) directly with
// stub render/RHI objects that touch no GPU, so it is fully CPU-only and
// deterministic. Address reuse is forced with placement-new into storage we
// own, mirroring the allocator reuse seen in the 2026-05-18 LAZ-clip log.
//
// Regression guard: synchroize() must free the stale cwRHIObject before
// remapping a reused address, so exactly one cwRHIObject stays alive.

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
    first->~StubRenderObject();             // destroy logical #1; RHI #1 still alive

    // --- Logical object #2, reusing #1's address ---
    auto* second = new (storage) StubRenderObject();
    REQUIRE(static_cast<void*>(second) == reusedAddress);

    second->setScene(&scene);               // addItem() cancels #1's pending delete
    second->setParent(nullptr);

    CwRhiSceneTestAccess::synchroize(rhiScene, &scene);   // creates RHI object #2

    // Only one render object exists now (second), so the backend should hold
    // exactly one cwRHIObject. The bug overwrites #1's lookup entry with #2's
    // and never frees RHI #1, so it stays orphaned in m_rhiObjects -> two alive.
    REQUIRE(g_aliveRhiObjects == 1);        // FAILS today: == 2

    second->~StubRenderObject();
    ::operator delete(storage);
}
