/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Phase 1 of plans/SCENE_VISIBILITY_STORE_PLAN.html (issue #579): equivalence
// between the owners' legacy authoring state and what the facades publish
// into cwSceneVisibility. No consumer reads the store yet — these tests are
// the net that dual-publish keeps both truths identical before Phases 2-3
// flip the consumers over. Store semantics themselves are covered by
// test_cwSceneVisibility.cpp; today's end-to-end behavior is pinned by
// test_cwSceneVisibilityCharacterization.cpp.

#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRenderLinePlot.h"
#include "cwRenderTexturedItems.h"
#include "cwScene.h"
#include "cwSceneVisibility.h"

//Qt includes
#include <QVector3D>
#include <QVector>

using namespace Catch;

namespace {

// The line plot publishes its whole mask under one fixed sub-slot (see
// kLinePlotSubId in cwRenderLinePlot.cpp)
constexpr uint32_t kLinePlotSubId = 0;

QVector<QVector3D> twoShots()
{
    return {
        QVector3D(0.0f, 0.0f, 0.0f), QVector3D(1.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f), QVector3D(1.0f, 1.0f, 0.0f),
    };
}

void checkMaskMatchesOwner(const cwVisibilitySnapshot& snapshot, const cwRenderLinePlot& plot)
{
    const QVector<quint8> ownerVisibility = plot.visibility();
    const QVector<quint8>* storeMask = snapshot.mask(plot.renderObjectId(), kLinePlotSubId);
    if (ownerVisibility.contains(cwRenderLinePlot::kHidden)) {
        REQUIRE(storeMask != nullptr);
        CHECK(*storeMask == ownerVisibility);
        // Shared with the owner's buffer, not deep-copied
        CHECK(storeMask->constData() == ownerVisibility.constData());
    } else {
        CHECK(storeMask == nullptr);
    }
}

}

TEST_CASE("Whole-object visibility dual-publishes to the scene store", "[SceneVisibility]")
{
    cwScene scene;
    auto* object = new cwRenderTexturedItems();

    // Authoring state set before attach reaches the store at attach
    object->setVisible(false);
    CHECK(scene.visibility()->version() == 0);

    object->setScene(&scene);
    CHECK(scene.visibility()->snapshot().objectVisible(object->renderObjectId())
          == object->isVisible());
    CHECK_FALSE(scene.visibility()->snapshot().objectVisible(object->renderObjectId()));

    // Post-attach toggles follow, both directions
    object->setVisible(true);
    CHECK(scene.visibility()->snapshot().objectVisible(object->renderObjectId()));
    object->setVisible(false);
    CHECK_FALSE(scene.visibility()->snapshot().objectVisible(object->renderObjectId()));

    // Detaching scrubs the object's published state
    object->setScene(nullptr);
    CHECK(scene.visibility()->snapshot().objectVisible(object->renderObjectId()));
    CHECK_FALSE(object->isVisible()); // the authoring copy is untouched

    delete object;
}

TEST_CASE("Textured item visibility dual-publishes per sub-item", "[SceneVisibility]")
{
    cwScene scene;
    auto* items = new cwRenderTexturedItems();

    // Seeded before attach: one visible, one hidden
    cwRenderTexturedItems::Item visibleItem;
    const uint32_t visibleId = items->addItem(visibleItem);
    cwRenderTexturedItems::Item hiddenItem;
    hiddenItem.visible = false;
    const uint32_t hiddenId = items->addItem(hiddenItem);

    items->setScene(&scene);
    const cwRenderObjectId ownerId = items->renderObjectId();

    auto checkItemsMatchOwner = [&]() {
        const auto snapshot = scene.visibility()->snapshot();
        for (const uint32_t id : {visibleId, hiddenId}) {
            CHECK(snapshot.subVisible(ownerId, id) == items->item(id).visible);
        }
    };

    checkItemsMatchOwner();
    CHECK_FALSE(scene.visibility()->snapshot().subVisible(ownerId, hiddenId));

    // Post-attach toggles follow, both directions
    items->setItemVisible(visibleId, false);
    items->setItemVisible(hiddenId, true);
    checkItemsMatchOwner();
    CHECK_FALSE(scene.visibility()->snapshot().subVisible(ownerId, visibleId));
    CHECK(scene.visibility()->snapshot().subVisible(ownerId, hiddenId));

    // An item added after attach publishes immediately
    cwRenderTexturedItems::Item lateItem;
    lateItem.visible = false;
    const uint32_t lateId = items->addItem(lateItem);
    CHECK_FALSE(scene.visibility()->snapshot().subVisible(ownerId, lateId));

    // Removing an item scrubs its entry
    items->removeItem(lateId);
    CHECK(scene.visibility()->snapshot().subVisible(ownerId, lateId));
    CHECK(scene.visibility()->snapshot().entryVersion(ownerId, lateId) == 0);

    // Deleting the owner while attached scrubs everything it published
    items->setItemVisible(visibleId, false);
    delete items;
    const auto snapshot = scene.visibility()->snapshot();
    CHECK(snapshot.objectVisible(ownerId));
    CHECK(snapshot.subVisible(ownerId, visibleId));
    CHECK(snapshot.entryVersion(ownerId, visibleId) == 0);
}

TEST_CASE("Line-plot range visibility dual-publishes its mask", "[SceneVisibility]")
{
    cwScene scene;
    auto* plot = new cwRenderLinePlot();

    // Geometry and a hidden range seeded before attach
    plot->setGeometry(twoShots());
    plot->setRangeVisible(0, 2, false);

    plot->setScene(&scene);
    checkMaskMatchesOwner(scene.visibility()->snapshot(), *plot);

    // Post-attach flips follow
    plot->setRangeVisible(2, 2, false);
    checkMaskMatchesOwner(scene.visibility()->snapshot(), *plot);

    // Restoring everything returns the store to the all-visible default
    plot->setRangeVisible(0, 4, true);
    checkMaskMatchesOwner(scene.visibility()->snapshot(), *plot);
    CHECK(scene.visibility()->snapshot().mask(plot->renderObjectId(), kLinePlotSubId) == nullptr);

    // New geometry clears a published mask: the old bytes no longer map to
    // the new vertex layout
    plot->setRangeVisible(0, 2, false);
    REQUIRE(scene.visibility()->snapshot().mask(plot->renderObjectId(), kLinePlotSubId) != nullptr);
    plot->setGeometry(twoShots());
    checkMaskMatchesOwner(scene.visibility()->snapshot(), *plot);
    CHECK(scene.visibility()->snapshot().mask(plot->renderObjectId(), kLinePlotSubId) == nullptr);

    delete plot;
}
