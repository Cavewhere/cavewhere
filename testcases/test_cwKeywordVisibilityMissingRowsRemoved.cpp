/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Reproducer for the cwKeywordVisibility "rowsRemoved is ignored" bug.
//
// cwKeywordVisibility::connectModel wires only rowsInserted and modelReset
// handlers (cwKeywordVisibility.cpp). When an item is removed from the
// visible model — for example, because the filter pipeline reclassified it
// into the rejected model — no visibility update fires for the removal.
// Visibility only changes if and when the item is then inserted into the
// other model. If the item lingers in both models (a separate bug in the
// pipeline triggers exactly that — see the rescan thrash repro and the
// user's debug log from 2026-05-18), whichever model emitted rowsInserted
// last determines the render object's final visibility.
//
// This test wires cwKeywordVisibility against two cwKeywordItemModel
// instances (so the ObjectRole role number matches) and demonstrates:
//
//   1. Removing a keyword item from the visible model alone does not
//      propagate to setVisible — the renderer keeps the stale flag.
//   2. When the same render object appears in both the visible and hide
//      models simultaneously, the resulting visibility is order-dependent
//      (last-inserted wins) instead of being a stable function of the
//      current model state.

#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>

#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordVisibility.h"
#include "cwRenderObject.h"
#include "cwRenderPointCloud.h"
#include "cwScene.h"

namespace {

// Sets up an isolated visibility/model environment with a single
// cwRenderPointCloud as the visibility target.
struct VisibilityBugFixture {
    cwScene scene;
    cwKeywordItemModel visibleModel;
    cwKeywordItemModel hideModel;
    cwKeywordVisibility visibility;
    cwRenderPointCloud* renderObject;

    VisibilityBugFixture()
        : renderObject(new cwRenderPointCloud())
    {
        renderObject->setScene(&scene);
        visibility.setVisibleModel(&visibleModel);
        visibility.setHideModel(&hideModel);
    }

    ~VisibilityBugFixture() {
        // setScene(nullptr) lets the scene tear down its queue entries
        // before delete; otherwise pendingItemCount holds a dangling ptr.
        renderObject->setScene(nullptr);
        delete renderObject;
    }

    cwKeywordItem* makeItem() {
        auto* item = new cwKeywordItem();
        item->setObject(renderObject);
        return item;
    }
};

} // namespace

// Removing an item from the visible model should drop the render object
// from its "managed visible" set — but cwKeywordVisibility only listens
// to rowsInserted and modelReset on either model, so the removal is
// silent. Once the render object is no longer accepted by any model,
// its visibility is whatever the last setVisible call left behind.
//
// Expected (after fix): the visibility tracker reacts to rowsRemoved
// and either restores the prior state or hands control back so a later
// classifier can update visibility deterministically.
// Actual (bug): rowsRemoved is dropped on the floor.
TEST_CASE("BUG: cwKeywordVisibility ignores rowsRemoved on the visible model",
          "[cwKeywordVisibility][bug][rowsRemoved]")
{
    VisibilityBugFixture f;

    REQUIRE(f.renderObject->isVisible());

    // Putting the item in the hide model first locks the render object
    // to invisible.
    cwKeywordItem* hideItem = f.makeItem();
    f.hideModel.addItem(hideItem);
    REQUIRE_FALSE(f.renderObject->isVisible());

    // Removing the item from the hide model means the render object is
    // no longer "managed-hidden". A correct handler would clear the
    // hide and revert visibility (or at minimum re-apply the visible
    // model's state); the current implementation does nothing.
    QSignalSpy visibleSpy(f.renderObject, &cwRenderObject::visibleChanged);
    f.hideModel.removeItem(hideItem);

    // BUG: render object stays invisible because cwKeywordVisibility
    // never observes the removal. With a fix in place, this would
    // either restore visibility or at least emit visibleChanged once
    // as the renderer is re-evaluated.
    CHECK(f.renderObject->isVisible());
    CHECK(visibleSpy.count() == 1);
}

// If the same render object is referenced by two keyword items (one in
// each model), the final visibility ends up as a function of insertion
// ORDER rather than the model state. The last rowsInserted handler to
// run wins because rowsInserted is the only signal cwKeywordVisibility
// listens for.
//
// The test runs both orderings to the same end state (one keyword item
// in each model, both pointing at the same render object) and asserts
// that both orderings agree on the result. A correct implementation
// must yield the same visibility regardless of insertion order.
TEST_CASE("BUG: cwKeywordVisibility resolution depends on rowsInserted "
          "emission order when an object is in both visible and hide models",
          "[cwKeywordVisibility][bug][orderDependent]")
{
    auto runOrdering = [](bool hideFirst) -> bool {
        VisibilityBugFixture f;
        cwKeywordItem* hideItem = f.makeItem();
        cwKeywordItem* visibleItem = f.makeItem();
        if (hideFirst) {
            f.hideModel.addItem(hideItem);
            f.visibleModel.addItem(visibleItem);
        } else {
            f.visibleModel.addItem(visibleItem);
            f.hideModel.addItem(hideItem);
        }
        return f.renderObject->isVisible();
    };

    const bool hideFirstResult = runOrdering(true);
    const bool visibleFirstResult = runOrdering(false);

    INFO("hide-first ordering visibility: " << hideFirstResult);
    INFO("visible-first ordering visibility: " << visibleFirstResult);

    // BUG: the two orderings end with the same model state — each model
    // holds one item, both items reference the same render object — but
    // disagree on the final visibility because cwKeywordVisibility only
    // reacts to rowsInserted, so the last emission wins.
    CHECK(hideFirstResult == visibleFirstResult);
}
