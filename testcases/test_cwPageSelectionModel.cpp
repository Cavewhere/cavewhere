// Catch includes
#include <catch2/catch_test_macros.hpp>

// Our includes
#include "cwPageSelectionModel.h"

// Qt includes
#include <QCoreApplication>
#include <QSignalSpy>
#include <QQmlComponent>
#include <QQmlEngine>

namespace
{
QQmlComponent* makePageComponent(QQmlEngine& engine, QObject* parent)
{
    static const char* pageQml =
        "import QtQuick 2.15\n"
        "Item {}\n";

    auto* component = new QQmlComponent(&engine, parent);
    component->setData(QByteArray(pageQml), QUrl());
    REQUIRE(component->status() == QQmlComponent::Ready);
    return component;
}

struct PageTree
{
    cwPage* source = nullptr;
    cwPage* data = nullptr;
    cwPage* cave = nullptr;
    cwPage* trip = nullptr;
    cwPage* view = nullptr;
};

PageTree createPageTree(cwPageSelectionModel& model, QQmlComponent* component)
{
    PageTree tree;
    tree.source = model.registerPage(nullptr, QStringLiteral("Source"), component);
    tree.data = model.registerPage(tree.source, QStringLiteral("Data"), component);
    tree.cave = model.registerPage(tree.data, QStringLiteral("Cave=Alpha"), component);
    tree.trip = model.registerPage(tree.cave, QStringLiteral("Trip=Trip 1"), component);
    tree.view = model.registerPage(nullptr, QStringLiteral("View"), component);
    return tree;
}
} // namespace

TEST_CASE("cwPageSelectionModel history navigation trims forward history", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);
    auto pages = createPageTree(model, component);

    model.gotoPage(pages.source);
    model.gotoPage(pages.data);
    REQUIRE(model.currentPage() == pages.data);
    REQUIRE(model.hasBackward());
    REQUIRE(!model.hasForward());
    REQUIRE(model.history().size() == 2);

    model.back();
    REQUIRE(model.currentPage() == pages.source);
    REQUIRE(!model.hasBackward());
    REQUIRE(model.hasForward());

    model.gotoPage(pages.view);
    REQUIRE(model.currentPage() == pages.view);
    REQUIRE(model.history().size() == 2);
    REQUIRE(model.history().at(0) == pages.source);
    REQUIRE(model.history().at(1) == pages.view);
    REQUIRE(model.hasBackward());
    REQUIRE(!model.hasForward());
}

TEST_CASE("cwPageSelectionModel resolves valid addresses and preserves invalid unknown address", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);
    auto pages = createPageTree(model, component);

    const QString validTripAddress = QStringLiteral("Source/Data/Cave=Alpha/Trip=Trip 1");
    model.setCurrentPageAddress(validTripAddress);
    REQUIRE(model.currentPage() == pages.trip);
    REQUIRE(model.currentPageAddress() == validTripAddress);

    const QString invalidTripAddress = QStringLiteral("Source/Data/Cave=Missing/Trip=Trip 1");
    model.setCurrentPageAddress(invalidTripAddress);
    REQUIRE(model.currentPage() == nullptr);
    REQUIRE(model.currentPageAddress() == invalidTripAddress);

    model.setCurrentPageAddress(validTripAddress);
    REQUIRE(model.currentPage() == pages.trip);
    REQUIRE(model.currentPageAddress() == validTripAddress);
}

TEST_CASE("cwPageSelectionModel emits address changes for current and ancestor renames", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);
    auto pages = createPageTree(model, component);

    model.gotoPage(pages.trip);
    QSignalSpy addressChangedSpy(&model, &cwPageSelectionModel::currentPageAddressChanged);

    pages.trip->setName(QStringLiteral("Trip=Trip 2"));
    REQUIRE(addressChangedSpy.count() >= 1);
    REQUIRE(model.currentPageAddress() == QStringLiteral("Source/Data/Cave=Alpha/Trip=Trip 2"));

    addressChangedSpy.clear();
    pages.cave->setName(QStringLiteral("Cave=Beta"));
    REQUIRE(addressChangedSpy.count() >= 1);
    REQUIRE(model.currentPageAddress() == QStringLiteral("Source/Data/Cave=Beta/Trip=Trip 2"));

    addressChangedSpy.clear();
    pages.view->setName(QStringLiteral("View Renamed"));
    REQUIRE(addressChangedSpy.count() == 0);
}

TEST_CASE("cwPageSelectionModel clearHistory disconnects rename propagation from stale page chain", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);
    auto pages = createPageTree(model, component);

    model.gotoPage(pages.trip);
    QSignalSpy addressChangedSpy(&model, &cwPageSelectionModel::currentPageAddressChanged);

    model.clearHistory();
    REQUIRE(model.currentPage() == nullptr);
    REQUIRE(model.history().isEmpty());

    pages.trip->setName(QStringLiteral("Trip=Trip 2"));
    pages.cave->setName(QStringLiteral("Cave=Beta"));
    REQUIRE(addressChangedSpy.count() == 0);
}

TEST_CASE("cwPageSelectionModel clearHistory prevents stale pages in history after project reload", "[cwPageSelectionModel][issue369]")
{
    // Verifies fix for GitHub issue #369: the sidebar's findPage() searches
    // history for the last Data-area page. Without clearHistory(), old trip
    // pages remain in history and clicking "Data" navigates to a stale page.
    // After clearHistory(), history is empty so the sidebar correctly falls
    // back to "Source/Data" (DataMainPage).

    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);

    // --- Simulate root pages registered by MainContent.onCompleted ---
    auto* view = model.registerPage(nullptr, QStringLiteral("View"), component);
    auto* source = model.registerPage(nullptr, QStringLiteral("Source"), component);
    auto* data = model.registerPage(source, QStringLiteral("Data"), component);

    // --- Simulate Project 1 caves/trips registered by Instantiators ---
    auto* caveA = model.registerPage(data, QStringLiteral("Cave=Phake Cave"), component);
    auto* trip1 = model.registerPage(caveA, QStringLiteral("Trip=Trip 1"), component);

    // Navigate: View → Data → Cave → Trip (building up history)
    model.gotoPage(view);
    model.gotoPage(data);
    model.gotoPage(caveA);
    model.gotoPage(trip1);
    REQUIRE(model.currentPage() == trip1);
    REQUIRE(model.history().size() == 4);

    // Without clearHistory, history contains the old trip page.
    // The sidebar's findPage("Source/Data") would find it and navigate there.
    REQUIRE(model.history().contains(trip1));

    // --- Simulate loading Project 2 via RootData::loadProject ---
    // This calls clearHistory() + gotoPageByName(null, "View")
    model.clearHistory();
    model.gotoPageByName(nullptr, "View");

    // After clearHistory, the old trip page is NOT in history
    REQUIRE(model.currentPage() == view);
    REQUIRE(model.history().size() == 1);
    REQUIRE(!model.history().contains(trip1));

    // Unregister old cave page (simulates Instantiator cleanup when caves are removed)
    model.unregisterPage(caveA);
    QCoreApplication::processEvents();

    // Old trip address should not resolve anymore
    model.setCurrentPageAddress(QStringLiteral("Source/Data/Cave=Phake Cave/Trip=Trip 1"));
    REQUIRE(model.currentPage() == nullptr);

    // Root pages (View, Source, Data) are preserved — not destroyed by clearHistory
    model.gotoPageByName(nullptr, "View");
    REQUIRE(model.currentPage() == view);

    model.setCurrentPageAddress(QStringLiteral("Source/Data"));
    REQUIRE(model.currentPage() == data);
}

TEST_CASE("cwPageSelectionModel unregisterPage removes page from address resolution", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);
    auto pages = createPageTree(model, component);

    REQUIRE(pages.source->childPage(QStringLiteral("Data")) == pages.data);
    model.unregisterPage(pages.data);
    QCoreApplication::processEvents();

    REQUIRE(pages.source->childPage(QStringLiteral("Data")) == nullptr);
    const QString dataAddress = QStringLiteral("Source/Data");
    model.setCurrentPageAddress(dataAddress);
    REQUIRE(model.currentPage() == nullptr);
    REQUIRE(model.currentPageAddress() == dataAddress);
}
