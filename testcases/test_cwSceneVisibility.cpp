/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Phase 1 of plans/SCENE_VISIBILITY_STORE_PLAN.html (issue #579): unit tests
// for cwSceneVisibility / cwVisibilitySnapshot semantics — sparse defaults,
// AND composition across levels, version and entryVersion advance,
// copy-on-read materialization, mask buffer sharing, and scrubbing. No scene
// or render objects involved; the dual-publish wiring is covered by
// test_cwSceneVisibilityDualPublish.cpp.

#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwSceneVisibility.h"

//Qt includes
#include <QSignalSpy>

namespace {

constexpr cwRenderObjectId kObjectA = cwRenderObjectId{1000001};
constexpr cwRenderObjectId kObjectB = cwRenderObjectId{1000002};
constexpr uint32_t kSubId = 7;

}

TEST_CASE("Store defaults are sparse: everything is visible until published", "[SceneVisibility]")
{
    cwSceneVisibility store;
    CHECK(store.version() == 0);

    const cwVisibilitySnapshot snapshot = store.snapshot();
    CHECK(snapshot.version() == 0);
    CHECK(snapshot.objectVisible(kObjectA));
    CHECK(snapshot.subVisible(kObjectA, kSubId));
    CHECK(snapshot.mask(kObjectA, kSubId) == nullptr);
    CHECK(snapshot.entryVersion(kObjectA, kSubId) == 0);

    // A default-constructed snapshot (no store behind it) reads the same way
    const cwVisibilitySnapshot empty;
    CHECK(empty.version() == 0);
    CHECK(empty.objectVisible(kObjectA));
    CHECK(empty.subVisible(kObjectA, kSubId));
    CHECK(empty.mask(kObjectA, kSubId) == nullptr);
    CHECK(empty.entryVersion(kObjectA, kSubId) == 0);
}

TEST_CASE("Effective visibility ANDs the object and sub levels", "[SceneVisibility]")
{
    cwSceneVisibility store;

    SECTION("hidden object gates its subs")
    {
        store.setObjectVisible(kObjectA, false);
        const auto snapshot = store.snapshot();
        CHECK_FALSE(snapshot.objectVisible(kObjectA));
        CHECK_FALSE(snapshot.subVisible(kObjectA, kSubId));
        CHECK(snapshot.objectVisible(kObjectB));
        CHECK(snapshot.subVisible(kObjectB, kSubId));
    }

    SECTION("hidden sub leaves the object and its siblings visible")
    {
        store.setSubVisible(kObjectA, kSubId, false);
        const auto snapshot = store.snapshot();
        CHECK(snapshot.objectVisible(kObjectA));
        CHECK_FALSE(snapshot.subVisible(kObjectA, kSubId));
        CHECK(snapshot.subVisible(kObjectA, kSubId + 1));
    }

    SECTION("hidden object AND hidden sub: showing one level is not enough")
    {
        store.setObjectVisible(kObjectA, false);
        store.setSubVisible(kObjectA, kSubId, false);
        store.setObjectVisible(kObjectA, true);
        CHECK_FALSE(store.snapshot().subVisible(kObjectA, kSubId));
        store.setSubVisible(kObjectA, kSubId, true);
        CHECK(store.snapshot().subVisible(kObjectA, kSubId));
    }
}

TEST_CASE("Version advances only on effective mutations", "[SceneVisibility]")
{
    cwSceneVisibility store;
    QSignalSpy changedSpy(&store, &cwSceneVisibility::changed);

    store.setObjectVisible(kObjectA, false);
    const quint64 afterHide = store.version();
    CHECK(afterHide == 1);
    CHECK(changedSpy.size() == 1);

    // Redundant writes are no-ops: no version bump, no signal
    store.setObjectVisible(kObjectA, false);
    store.setObjectVisible(kObjectB, true);
    store.setSubVisible(kObjectA, kSubId, true);
    store.setMask(kObjectA, kSubId, QVector<quint8>());
    store.removeSub(kObjectA, kSubId);
    store.removeObject(kObjectB);
    CHECK(store.version() == afterHide);
    CHECK(changedSpy.size() == 1);

    store.setObjectVisible(kObjectA, true);
    CHECK(store.version() == afterHide + 1);
    CHECK(changedSpy.size() == 2);

    // Rewriting an identical mask is a no-op too
    const QVector<quint8> mask(4, 0x00);
    store.setMask(kObjectA, kSubId, mask);
    const quint64 afterMask = store.version();
    store.setMask(kObjectA, kSubId, mask);
    CHECK(store.version() == afterMask);
}

TEST_CASE("entryVersion tracks its own entry, not unrelated churn", "[SceneVisibility]")
{
    cwSceneVisibility store;

    store.setSubVisible(kObjectA, kSubId, false);
    const quint64 entryV1 = store.snapshot().entryVersion(kObjectA, kSubId);
    CHECK(entryV1 > 0);

    // Unrelated mutations advance the store version but not this entry —
    // the property cwRHILinePlot's upload gate relies on
    store.setObjectVisible(kObjectB, false);
    store.setSubVisible(kObjectB, kSubId, false);
    store.setMask(kObjectB, kSubId + 1, QVector<quint8>(2, 0x00));
    const auto snapshot = store.snapshot();
    CHECK(snapshot.version() > entryV1);
    CHECK(snapshot.entryVersion(kObjectA, kSubId) == entryV1);

    // A mutation of the entry itself advances it
    store.setMask(kObjectA, kSubId, QVector<quint8>(2, 0x00));
    CHECK(store.snapshot().entryVersion(kObjectA, kSubId) > entryV1);
}

TEST_CASE("Snapshots materialize on read and stay immutable", "[SceneVisibility]")
{
    cwSceneVisibility store;
    store.setMask(kObjectA, kSubId, QVector<quint8>(3, 0x00));

    // Two reads with no intervening write share one materialization
    const auto first = store.snapshot();
    const auto second = store.snapshot();
    REQUIRE(first.mask(kObjectA, kSubId) != nullptr);
    CHECK(first.mask(kObjectA, kSubId) == second.mask(kObjectA, kSubId));

    // A write after materialization leaves the taken snapshot untouched
    store.setObjectVisible(kObjectA, false);
    store.setMask(kObjectA, kSubId, QVector<quint8>());
    CHECK(first.objectVisible(kObjectA));
    CHECK(first.mask(kObjectA, kSubId) != nullptr);
    CHECK(first.version() < store.version());

    const auto third = store.snapshot();
    CHECK_FALSE(third.objectVisible(kObjectA));
    CHECK(third.mask(kObjectA, kSubId) == nullptr);
    CHECK(third.version() == store.version());
}

TEST_CASE("Masks share the publisher's buffer instead of copying it", "[SceneVisibility]")
{
    cwSceneVisibility store;
    const QVector<quint8> mask(5, 0x00);
    store.setMask(kObjectA, kSubId, mask);

    const auto snapshot = store.snapshot();
    const QVector<quint8>* stored = snapshot.mask(kObjectA, kSubId);
    REQUIRE(stored != nullptr);
    CHECK(*stored == mask);
    // Implicit sharing: same backing buffer, not a deep copy
    CHECK(stored->constData() == mask.constData());
}

TEST_CASE("removeObject scrubs the object entry and every sub under it", "[SceneVisibility]")
{
    cwSceneVisibility store;
    store.setObjectVisible(kObjectA, false);
    store.setSubVisible(kObjectA, kSubId, false);
    store.setMask(kObjectA, kSubId + 1, QVector<quint8>(2, 0x00));
    store.setSubVisible(kObjectB, kSubId, false);

    store.removeObject(kObjectA);

    const auto snapshot = store.snapshot();
    CHECK(snapshot.objectVisible(kObjectA));
    CHECK(snapshot.subVisible(kObjectA, kSubId));
    CHECK(snapshot.mask(kObjectA, kSubId + 1) == nullptr);
    CHECK(snapshot.entryVersion(kObjectA, kSubId) == 0);
    // Other objects' entries survive
    CHECK_FALSE(snapshot.subVisible(kObjectB, kSubId));
}

TEST_CASE("A recreated entry's entryVersion is fresh, never a reused value", "[SceneVisibility]")
{
    cwSceneVisibility store;

    // The property cwRHILinePlot's upload gate depends on: an entry that
    // comes back after a scrub must read as new work, never as a version a
    // consumer already consumed.
    store.setSubVisible(kObjectA, kSubId, false);
    const quint64 firstLife = store.snapshot().entryVersion(kObjectA, kSubId);
    REQUIRE(firstLife > 0);

    store.removeSub(kObjectA, kSubId);
    CHECK(store.snapshot().entryVersion(kObjectA, kSubId) == 0);

    store.setSubVisible(kObjectA, kSubId, false);
    const quint64 secondLife = store.snapshot().entryVersion(kObjectA, kSubId);
    CHECK(secondLife > firstLife);

    // Same freshness across a whole-object scrub
    store.removeObject(kObjectA);
    store.setMask(kObjectA, kSubId, QVector<quint8>(2, 0x00));
    CHECK(store.snapshot().entryVersion(kObjectA, kSubId) > secondLife);
}

TEST_CASE("removeSub scrubs one entry; showing a masked sub keeps its mask entry", "[SceneVisibility]")
{
    cwSceneVisibility store;

    store.setSubVisible(kObjectA, kSubId, false);
    store.removeSub(kObjectA, kSubId);
    CHECK(store.snapshot().subVisible(kObjectA, kSubId));
    CHECK(store.snapshot().entryVersion(kObjectA, kSubId) == 0);

    // A sub that is both hidden and masked keeps the mask when shown again
    store.setSubVisible(kObjectA, kSubId, false);
    store.setMask(kObjectA, kSubId, QVector<quint8>(2, 0x00));
    store.setSubVisible(kObjectA, kSubId, true);
    CHECK(store.snapshot().subVisible(kObjectA, kSubId));
    CHECK(store.snapshot().mask(kObjectA, kSubId) != nullptr);

    // And clearing the mask on a visible sub removes the entry entirely
    store.setMask(kObjectA, kSubId, QVector<quint8>());
    CHECK(store.snapshot().entryVersion(kObjectA, kSubId) == 0);
}
