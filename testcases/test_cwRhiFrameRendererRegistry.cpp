// Headless engine-level guard for the issue #512 fix. Where
// test_cwScene_issue512.cpp drives the fix through the front-end cwScene ->
// cwRhiScene::synchroize translation (and so needs the CwRhiSceneTestAccess
// friend to reach a private apply step), this test exercises the registry
// directly on cwRhiFrameRenderer's public API: register/destroy keyed by a
// stable cwRenderObjectId, with no cwScene, no friend, and no QRhi.
//
// cwRhiFrameRenderer is the draw engine extracted from cwRhiScene; a bare
// instance touches no GPU until initialize(), so the registry is fully unit-
// testable on the CPU. The observable is the same fix-agnostic alive-count as
// the scene-level test: any correct register/destroy path leaves exactly the
// render objects still mapped alive, and the issue #512 re-map guard must free
// an object displaced by a same-id re-register rather than orphan it.

#include <catch2/catch_test_macros.hpp>

#include "cwRHIObject.h"
#include "cwRenderObjectId.h"
#include "cwRhiFrameRenderer.h"

namespace {

// Counts live stub RHI objects — the orphan the #512 fix prevents is a
// cwRHIObject that outlives its mapping, so a simple alive-count catches it.
int g_aliveRhiObjects = 0;

class StubRhiObject : public cwRHIObject
{
public:
    StubRhiObject() { ++g_aliveRhiObjects; }
    ~StubRhiObject() override { --g_aliveRhiObjects; }

    // No GPU work — the registry never calls these; they exist only so the
    // class is concrete.
    void initialize(const ResourceUpdateData&) override {}
    void synchronize(const SynchronizeData&) override {}
    void updateResources(const ResourceUpdateData&) override {}
    void render(const RenderData&) override {}
};

} // namespace

TEST_CASE("cwRhiFrameRenderer registers and resolves render objects by id",
          "[cwRhiFrameRenderer][issue512]")
{
    g_aliveRhiObjects = 0;

    cwRhiFrameRenderer engine;   // bare engine, no QRhi

    constexpr auto idA = cwRenderObjectId{1};
    constexpr auto idB = cwRenderObjectId{2};

    auto* a = new StubRhiObject();
    auto* b = new StubRhiObject();
    engine.registerRenderObject(idA, a);
    engine.registerRenderObject(idB, b);

    REQUIRE(g_aliveRhiObjects == 2);
    REQUIRE(engine.renderObjectForId(idA) == a);
    REQUIRE(engine.renderObjectForId(idB) == b);
    REQUIRE(engine.renderObjects().size() == 2);
    REQUIRE(engine.renderObjectForId(cwRenderObjectId{99}) == nullptr);

    // destroyRenderObject frees the mapped object and unmaps its id, leaving
    // the other untouched.
    engine.destroyRenderObject(idA);
    REQUIRE(g_aliveRhiObjects == 1);
    REQUIRE(engine.renderObjectForId(idA) == nullptr);
    REQUIRE(engine.renderObjectForId(idB) == b);
    REQUIRE(engine.renderObjects().size() == 1);

    // Destroying an id with no mapping is a no-op.
    engine.destroyRenderObject(cwRenderObjectId{99});
    REQUIRE(g_aliveRhiObjects == 1);

    engine.destroyRenderObject(idB);
    REQUIRE(g_aliveRhiObjects == 0);
    REQUIRE(engine.renderObjects().isEmpty());
}

TEST_CASE("cwRhiFrameRenderer re-registering an id frees the displaced object "
          "(issue #512 guard)",
          "[cwRhiFrameRenderer][issue512]")
{
    // The engine-level half of the #512 fix: an object already mapped under an
    // id can be re-registered under that same id (the re-add path, where
    // cwScene::addItem has cancelled the old object's pending delete). The
    // guard must free the displaced object before remapping — overwriting the
    // lookup entry blindly would orphan it in renderObjects() forever.
    g_aliveRhiObjects = 0;

    cwRhiFrameRenderer engine;

    constexpr auto id = cwRenderObjectId{1};

    auto* first = new StubRhiObject();
    engine.registerRenderObject(id, first);
    REQUIRE(g_aliveRhiObjects == 1);

    auto* second = new StubRhiObject();
    engine.registerRenderObject(id, second);   // same id: must free `first`

    REQUIRE(g_aliveRhiObjects == 1);                       // not 2 — no orphan
    REQUIRE(engine.renderObjectForId(id) == second);
    // size == 1 (not the list's order, which is an internal gather detail) is
    // what rules out the orphan: `first` is gone, only `second` remains mapped.
    REQUIRE(engine.renderObjects().size() == 1);

    engine.destroyRenderObject(id);
    REQUIRE(g_aliveRhiObjects == 0);
}
