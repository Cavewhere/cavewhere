// Private-API tripwire for cwQuickItemSubscene. It exercises the GUI-thread
// ref/deref lifecycle (refWindow + refFromEffectItem on construct, the matching
// derefs on destroy) without a live QRhi. If a Qt upgrade breaks the private-API
// contract these calls depend on, this fails to build or run — not the whole app.
// The full materialize-and-render path is covered by the QML/offscreen suite.

#include <catch2/catch_test_macros.hpp>

#include <QQuickItem>
#include <QQuickWindow>

#include "cwQuickItemSubscene.h"

TEST_CASE("cwQuickItemSubscene: null arguments are inert", "[cwQuickItemSubscene]")
{
    cwQuickItemSubscene subscene(nullptr, nullptr);

    REQUIRE_FALSE(subscene.nodeReady());
    REQUIRE(subscene.rootNodeHandle() == 0);
    REQUIRE(subscene.content() == nullptr);
    REQUIRE(subscene.window() == nullptr);

    // Must not crash with nothing referenced.
    subscene.forceSync();
}

TEST_CASE("cwQuickItemSubscene: ref/deref a real item and window", "[cwQuickItemSubscene]")
{
    QQuickWindow window;
    auto content = std::make_unique<QQuickItem>(window.contentItem());
    content->setSize(QSizeF(64, 32));

    {
        cwQuickItemSubscene subscene(content.get(), &window);

        REQUIRE(subscene.content() == content.get());
        REQUIRE(subscene.window() == &window);

        // The scene graph never syncs in this CPU-only test, so the node subtree
        // is not materialized: the handle stays null and is reported not-ready.
        REQUIRE(subscene.rootNodeHandle() == 0);
        REQUIRE_FALSE(subscene.nodeReady());

        // forceSync on an unrendered window must be safe (no-op without a render).
        subscene.forceSync();
    } // destructor runs derefFromEffectItem + derefWindow — must not crash

    // The content item outlives the subscene cleanly.
    REQUIRE(content != nullptr);
}
