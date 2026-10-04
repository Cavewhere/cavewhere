// Catch includes
#include <catch2/catch_test_macros.hpp>

// Our includes
#include "cwPageSelectionModel.h"
#include "cwLinkGenerator.h"
#include "cwSurveyNode.h"
#include "cwSurveyNodeChildModel.h"
#include "cwCave.h"
#include "cwTrip.h"

// Qt includes
#include <QCoreApplication>
#include <QPointer>
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

TEST_CASE("cwPageSelectionModel clearHistory preserves static pages and only clears dynamic components", "[cwPageSelectionModel]")
{
    // Verifies that clearHistory() preserves the first two levels of the page
    // tree (top-level and their direct children) while clearing depth-2+ pages.
    // The returned component set must contain only dynamic page components,
    // not those used by static pages — so cwPageView can selectively evict
    // cached items without destroying the View page's rendering state.

    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* staticComponent = makePageComponent(engine, &model);
    auto* dynamicComponent = makePageComponent(engine, &model);

    // Static pages (registered once by MainContent.qml)
    auto* view = model.registerPage(nullptr, QStringLiteral("View"), staticComponent);
    auto* source = model.registerPage(nullptr, QStringLiteral("Source"), staticComponent);
    auto* data = model.registerPage(source, QStringLiteral("Data"), staticComponent);

    // Dynamic pages (registered by QML Repeaters in DataMainPage/CavePage)
    auto* cave = model.registerPage(data, QStringLiteral("Cave=Alpha"), dynamicComponent);
    auto* trip = model.registerPage(cave, QStringLiteral("Trip=Trip 1"), dynamicComponent);

    model.gotoPage(trip);

    auto clearedComponents = model.clearHistory();

    // Static pages must survive
    REQUIRE(model.rootPage()->childPage(QStringLiteral("View")) == view);
    REQUIRE(model.rootPage()->childPage(QStringLiteral("Source")) == source);
    REQUIRE(source->childPage(QStringLiteral("Data")) == data);

    // Static pages must still be navigable
    model.gotoPageByName(nullptr, "View");
    REQUIRE(model.currentPage() == view);
    model.setCurrentPageAddress(QStringLiteral("Source/Data"));
    REQUIRE(model.currentPage() == data);

    // Dynamic pages must be cleared from the tree
    REQUIRE(data->childPage(QStringLiteral("Cave=Alpha")) == nullptr);

    // Cleared components must contain the dynamic component, not the static one
    REQUIRE(clearedComponents.contains(dynamicComponent));
    REQUIRE(!clearedComponents.contains(staticComponent));
}

TEST_CASE("cwPageSelectionModel clearHistory returns empty set when no dynamic pages exist", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);

    // Only static pages
    model.registerPage(nullptr, QStringLiteral("View"), component);
    auto* source = model.registerPage(nullptr, QStringLiteral("Source"), component);
    model.registerPage(source, QStringLiteral("Data"), component);

    auto clearedComponents = model.clearHistory();
    REQUIRE(clearedComponents.isEmpty());
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

TEST_CASE("cwPageSelectionModel resolves note pages under trip address", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);
    auto* noteComponent = makePageComponent(engine, &model);
    auto pages = createPageTree(model, component);

    // Register note pages under the trip (using note names, not indices)
    auto* note1 = model.registerPage(pages.trip, QStringLiteral("Note=photo.png"), noteComponent,
                                      {{"currentNoteIndex", 0}});
    auto* note2 = model.registerPage(pages.trip, QStringLiteral("Note=scan.glb"), noteComponent,
                                      {{"currentNoteIndex", 1}});

    REQUIRE(note1 != nullptr);
    REQUIRE(note2 != nullptr);

    // Verify parent-child relationships
    REQUIRE(pages.trip->childPage(QStringLiteral("Note=photo.png")) == note1);
    REQUIRE(pages.trip->childPage(QStringLiteral("Note=scan.glb")) == note2);

    // Verify full address resolution
    const QString note1Address = QStringLiteral("Source/Data/Cave=Alpha/Trip=Trip 1/Note=photo.png");
    model.setCurrentPageAddress(note1Address);
    REQUIRE(model.currentPage() == note1);
    REQUIRE(model.currentPageAddress() == note1Address);

    const QString note2Address = QStringLiteral("Source/Data/Cave=Alpha/Trip=Trip 1/Note=scan.glb");
    model.setCurrentPageAddress(note2Address);
    REQUIRE(model.currentPage() == note2);
    REQUIRE(model.currentPageAddress() == note2Address);
}

TEST_CASE("cwPageSelectionModel shows unknown page for invalid note address", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);
    auto* noteComponent = makePageComponent(engine, &model);
    auto pages = createPageTree(model, component);

    // Register only one note
    model.registerPage(pages.trip, QStringLiteral("Note=photo.png"), noteComponent,
                       {{"currentNoteIndex", 0}});

    // Try to navigate to a note that doesn't exist
    const QString invalidAddress = QStringLiteral("Source/Data/Cave=Alpha/Trip=Trip 1/Note=missing.png");
    model.setCurrentPageAddress(invalidAddress);
    REQUIRE(model.currentPage() == nullptr);
    REQUIRE(model.currentPageAddress() == invalidAddress);
}

TEST_CASE("cwPageSelectionModel unregister note page removes from tree", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);
    auto* noteComponent = makePageComponent(engine, &model);
    auto pages = createPageTree(model, component);

    auto* note1 = model.registerPage(pages.trip, QStringLiteral("Note=photo.png"), noteComponent,
                                      {{"currentNoteIndex", 0}});
    auto* note2 = model.registerPage(pages.trip, QStringLiteral("Note=scan.glb"), noteComponent,
                                      {{"currentNoteIndex", 1}});

    REQUIRE(pages.trip->childPage(QStringLiteral("Note=photo.png")) == note1);
    REQUIRE(pages.trip->childPage(QStringLiteral("Note=scan.glb")) == note2);

    // Unregister note1
    model.unregisterPage(note1);
    QCoreApplication::processEvents();

    REQUIRE(pages.trip->childPage(QStringLiteral("Note=photo.png")) == nullptr);
    REQUIRE(pages.trip->childPage(QStringLiteral("Note=scan.glb")) == note2);

    // Address for removed note should fail
    const QString note1Address = QStringLiteral("Source/Data/Cave=Alpha/Trip=Trip 1/Note=photo.png");
    model.setCurrentPageAddress(note1Address);
    REQUIRE(model.currentPage() == nullptr);
}

TEST_CASE("cwPageSelectionModel note page address updates when renamed", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);
    auto* noteComponent = makePageComponent(engine, &model);
    auto pages = createPageTree(model, component);

    auto* notePage = model.registerPage(pages.trip, QStringLiteral("Note=photo.png"), noteComponent,
                                         {{"currentNoteIndex", 0}});

    model.gotoPage(notePage);
    REQUIRE(model.currentPageAddress() == QStringLiteral("Source/Data/Cave=Alpha/Trip=Trip 1/Note=photo.png"));

    QSignalSpy addressChangedSpy(&model, &cwPageSelectionModel::currentPageAddressChanged);

    // Rename the note page (simulates what setNamingFunction does)
    notePage->setName(QStringLiteral("Note=renamed.png"));
    REQUIRE(addressChangedSpy.count() >= 1);
    REQUIRE(model.currentPageAddress() == QStringLiteral("Source/Data/Cave=Alpha/Trip=Trip 1/Note=renamed.png"));
}


namespace
{
cwCave* makeNode(cwSurveyNode* parent, const QString& name)
{
    auto* node = new cwCave();
    node->setName(name);
    parent->addNode(node);
    return node;
}
} // namespace

TEST_CASE("cwLinkGenerator names a node page by its whole path", "[cwPageSelectionModel][cwLinkGenerator]")
{
    cwSurveyNode root(cwSurveyNode::RootNodeTag{});
    cwCave* folder = makeNode(&root, QStringLiteral("Kentucky"));
    cwCave* cave = makeNode(folder, QStringLiteral("Side Cave"));
    cwCave* section = makeNode(cave, QStringLiteral("Upper level"));

    auto* trip = new cwTrip();
    trip->setName(QStringLiteral("Trip 1"));
    section->addTrip(trip);

    cwLinkGenerator links;
    CHECK(links.nodeLink(folder) == QStringLiteral("Source/Data/Node=Kentucky"));
    CHECK(links.nodeLink(section) == QStringLiteral("Source/Data/Node=Kentucky/Node=Side Cave/Node=Upper level"));
    CHECK(links.caveLink(cave) == links.nodeLink(cave));
    CHECK(links.tripLink(trip) == QStringLiteral("Source/Data/Node=Kentucky/Node=Side Cave/Node=Upper level/Trip=Trip 1"));
    CHECK(links.fixStationsLink(cave) == QStringLiteral("Source/Data/Node=Kentucky/Node=Side Cave/Fix Stations"));
    CHECK(links.nodeLink(&root) == links.dataPageLink());
    CHECK(links.nodeLink(nullptr).isEmpty());
    CHECK(links.fixStationsLink(nullptr).isEmpty());
}

TEST_CASE("cwPageSelectionModel resolves node pages at any depth", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);

    auto* source = model.registerPage(nullptr, QStringLiteral("Source"), component);
    auto* data = model.registerPage(source, QStringLiteral("Data"), component);
    auto* folder = model.registerPage(data, QStringLiteral("Node=Kentucky"), component);
    auto* cave = model.registerPage(folder, QStringLiteral("Node=Side Cave"), component);
    auto* section = model.registerPage(cave, QStringLiteral("Node=Upper level"), component);
    auto* trip = model.registerPage(section, QStringLiteral("Trip=Trip 1"), component);

    const QString tripAddress = QStringLiteral("Source/Data/Node=Kentucky/Node=Side Cave/Node=Upper level/Trip=Trip 1");
    model.setCurrentPageAddress(tripAddress);
    REQUIRE(model.currentPage() == trip);
    CHECK(model.currentPageAddress() == tripAddress);

    //Each crumb of the trail is a page of its own.
    model.setCurrentPageAddress(QStringLiteral("Source/Data/Node=Kentucky/Node=Side Cave"));
    CHECK(model.currentPage() == cave);
    model.setCurrentPageAddress(QStringLiteral("Source/Data/Node=Kentucky"));
    CHECK(model.currentPage() == folder);

    model.setCurrentPageAddress(QStringLiteral("Source/Data/Node=Kentucky/Node=Missing/Trip=Trip 1"));
    CHECK(model.currentPage() == nullptr);
}

TEST_CASE("cwPageSelectionModel resolves an old Cave= link to the Node= page", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* component = makePageComponent(engine, &model);

    auto* source = model.registerPage(nullptr, QStringLiteral("Source"), component);
    auto* data = model.registerPage(source, QStringLiteral("Data"), component);
    auto* cave = model.registerPage(data, QStringLiteral("Node=Alpha"), component);
    auto* trip = model.registerPage(cave, QStringLiteral("Trip=Trip 1"), component);
    auto* note = model.registerPage(trip, QStringLiteral("Note=photo.png"), component);

    model.setCurrentPageAddress(QStringLiteral("Source/Data/Cave=Alpha/Trip=Trip 1"));
    REQUIRE(model.currentPage() == trip);
    CHECK(model.currentPageAddress() == QStringLiteral("Source/Data/Node=Alpha/Trip=Trip 1"));

    model.setCurrentPageAddress(QStringLiteral("Source/Data/Cave=Alpha"));
    CHECK(model.currentPage() == cave);

    model.setCurrentPageAddress(QStringLiteral("Source/Data/Cave=Alpha/Trip=Trip 1/Note=photo.png"));
    CHECK(model.currentPage() == note);

    const QString missingCave = QStringLiteral("Source/Data/Cave=Missing/Trip=Trip 1");
    model.setCurrentPageAddress(missingCave);
    CHECK(model.currentPage() == nullptr);
    CHECK(model.currentPageAddress() == missingCave);
}

TEST_CASE("cwPageSelectionModel clearHistory frees node pages at every depth", "[cwPageSelectionModel]")
{
    QQmlEngine engine;
    cwPageSelectionModel model;
    auto* staticComponent = makePageComponent(engine, &model);
    auto* topNodeComponent = makePageComponent(engine, &model);
    auto* childNodeComponent = makePageComponent(engine, &model);
    auto* grandchildNodeComponent = makePageComponent(engine, &model);

    auto* source = model.registerPage(nullptr, QStringLiteral("Source"), staticComponent);
    auto* data = model.registerPage(source, QStringLiteral("Data"), staticComponent);

    //Each depth registers its children from a component of its own, as
    //CavePage.qml does.
    QPointer<cwPage> folder = model.registerPage(data, QStringLiteral("Node=Kentucky"), topNodeComponent);
    QPointer<cwPage> cave = model.registerPage(folder, QStringLiteral("Node=Side Cave"), childNodeComponent);
    QPointer<cwPage> section = model.registerPage(cave, QStringLiteral("Node=Upper level"), grandchildNodeComponent);
    QPointer<cwPage> trip = model.registerPage(section, QStringLiteral("Trip=Trip 1"), staticComponent);

    model.gotoPage(trip);

    const auto clearedComponents = model.clearHistory();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    CHECK(folder.isNull());
    CHECK(cave.isNull());
    CHECK(section.isNull());
    CHECK(trip.isNull());
    CHECK(data->childPages().isEmpty());

    CHECK(clearedComponents.contains(topNodeComponent));
    CHECK(clearedComponents.contains(childNodeComponent));
    CHECK(clearedComponents.contains(grandchildNodeComponent));
}

TEST_CASE("cwSurveyNodeChildModel lists a node's child nodes", "[cwPageSelectionModel][SurveyNodeChildModel]")
{
    cwSurveyNode root(cwSurveyNode::RootNodeTag{});
    cwCave* cave = makeNode(&root, QStringLiteral("Side Cave"));

    cwSurveyNodeChildModel model;
    CHECK(model.rowCount() == 0);

    model.setNode(cave);
    CHECK(model.rowCount() == 0);

    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    cwCave* upper = makeNode(cave, QStringLiteral("Upper level"));
    cwCave* lower = makeNode(cave, QStringLiteral("Lower level"));
    CHECK(insertedSpy.count() == 2);
    REQUIRE(model.rowCount() == 2);

    const auto nodeAt = [&model](int row) {
        return model.data(model.index(row), cwSurveyNodeChildModel::NodeObjectRole).value<cwSurveyNode*>();
    };
    QList<cwSurveyNode*> rows {nodeAt(0), nodeAt(1)};
    CHECK(rows.contains(upper));
    CHECK(rows.contains(lower));
    CHECK(model.roleNames().value(cwSurveyNodeChildModel::NodeObjectRole) == QByteArrayLiteral("nodeObjectRole"));

    cave->removeNode(cave->indexOfNode(upper));
    CHECK(removedSpy.count() == 1);
    REQUIRE(model.rowCount() == 1);
    CHECK(nodeAt(0) == lower);

    //A node that goes away takes its rows along.
    auto* lone = new cwCave();
    makeNode(lone, QStringLiteral("Only child"));
    model.setNode(lone);
    REQUIRE(model.rowCount() == 1);
    resetSpy.clear();
    delete lone;
    CHECK(model.node() == nullptr);
    CHECK(model.rowCount() == 0);
    CHECK(resetSpy.count() == 1);
}
