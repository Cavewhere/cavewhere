//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "SignalSpyChecker.h"
#include "SurveyTreeTestHelper.h"
#include "cwCave.h"
#include "cwCaveData.h"
#include "cwCavingRegion.h"
#include "cwFixStationModel.h"
#include "cwSignalSpy.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"

//Qt includes
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QSignalSpy>
#include <QUndoStack>
#include <QUuid>

//Std includes
#include <memory>
#include <utility>
#include <vector>

namespace {
    //Every node the production code builds is a cwCave, so the tests build one
    //too: the shim is what keeps qobject_cast<cwCave*> answering for any node.
    cwCave* makeNode(const QString& name)
    {
        auto* node = new cwCave();
        node->setName(name);
        return node;
    }

    cwCave* makeNode(const QString& name, cwSurveyNode::Kind kind)
    {
        cwCave* node = makeNode(name);
        node->setKind(kind);
        return node;
    }

    cwTrip* makeTrip(cwSurveyNode* node, const QString& name)
    {
        auto* trip = new cwTrip();
        trip->setName(name);
        node->addTrip(trip);
        return trip;
    }

    using SurveyTreeTestHelper::attachedFixes;
    using SurveyTreeTestHelper::fixAt;
}

TEST_CASE("cwSurveyNode inserts, removes and undoes a child node", "[SurveyNode]")
{
    cwCave parent;
    parent.setName(QStringLiteral("Parent"));

    QUndoStack undoStack;

    cwCave* child = makeNode(QStringLiteral("Child"));
    QPointer<cwSurveyNode> childPtr(child);

    SECTION("an insert pulses the node signals and leaves the trip rows alone") {
        parent.setUndoStack(&undoStack);

        auto checker = SignalSpyChecker::Constant::makeChecker(&parent);
        checker[checker.findSpy(&cwSurveyNode::beginInsertNodes)] = 1;
        checker[checker.findSpy(&cwSurveyNode::insertedNodes)] = 1;
        checker[checker.findSpy(&cwSurveyNode::childNodeCountChanged)] = 1;
        checker[checker.findSpy(&cwSurveyNode::childScopeLabelsChanged)] = 1;
        checker[checker.findSpy(&cwSurveyNode::scopeLabelsChanged)] = 1;
        checker[checker.findSpy(&cwSurveyNode::subtreeChanged)] = 1;

        parent.addNode(child);

        //Every other signal — beginRemoveNodes, removedNodes, and the whole
        //QAbstractItemModel row set — is expected 0, so a node insert that
        //touched the trip rows would fail here.
        checker.checkSpies();

        REQUIRE(parent.childNodeCount() == 1);
        CHECK(parent.childNode(0) == child);
        CHECK(parent.indexOfNode(child) == 0);
        CHECK(child->parentNode() == &parent);
        CHECK(child->parent() == &parent);
        CHECK(child->undoStack() == &undoStack);
        CHECK(parent.rowCount() == 0);

        undoStack.undo();
        CHECK(parent.childNodeCount() == 0);
        REQUIRE(!childPtr.isNull());
        //The parent pointer stays set on a removal, so undo can put the node
        //back and the QML engine cannot collect it.
        CHECK(child->parentNode() == &parent);

        undoStack.redo();
        REQUIRE(parent.childNodeCount() == 1);
        CHECK(parent.childNode(0) == child);
    }

    SECTION("with no undo stack the removal destroys the node outright") {
        parent.addNode(child);
        REQUIRE(parent.childNodeCount() == 1);

        parent.removeNode(0);
        CHECK(parent.childNodeCount() == 0);

        //deleteLater outside an event loop needs the deferred deletes asked for
        //by name; processEvents() skips them at loop level zero.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        CHECK(childPtr.isNull());
    }
}

TEST_CASE("cwSurveyNode path names the chain below the root", "[SurveyNode]")
{
    cwSurveyNode root(cwSurveyNode::RootNodeTag{});

    cwCave* a = makeNode(QStringLiteral("A"));
    root.addNode(a);
    cwCave* b = makeNode(QStringLiteral("B"));
    a->addNode(b);

    CHECK(root.isRoot());
    CHECK_FALSE(a->isRoot());

    CHECK(root.path().isEmpty());
    CHECK(a->path() == QStringList{QStringLiteral("A")});
    CHECK(b->path() == (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
    CHECK(b->pathIds() == (QList<QUuid>{a->id(), b->id()}));

    //A lone cave answers with its own name, the persistence key of today's
    //flat layout.
    cwCave solo;
    solo.setName(QStringLiteral("Solo"));
    CHECK(solo.path() == QStringList{QStringLiteral("Solo")});
}

TEST_CASE("cwSurveyNode allTrips and allNodes walk in document order", "[SurveyNode]")
{
    cwCave a;
    a.setName(QStringLiteral("A"));
    cwTrip* t1 = makeTrip(&a, QStringLiteral("T1"));

    cwCave* b = makeNode(QStringLiteral("B"));
    a.addNode(b);
    cwTrip* t2 = makeTrip(b, QStringLiteral("T2"));

    cwCave* c = makeNode(QStringLiteral("C"));
    a.addNode(c);
    cwTrip* t3 = makeTrip(c, QStringLiteral("T3"));

    cwCave* d = makeNode(QStringLiteral("D"));
    c->addNode(d);
    cwTrip* t4 = makeTrip(d, QStringLiteral("T4"));

    //Child nodes first, then the node's own trips — the row order the tree
    //model lists.
    CHECK(a.allTrips() == (QList<cwTrip*>{t2, t4, t3, t1}));
    CHECK(c->allTrips() == (QList<cwTrip*>{t4, t3}));
    CHECK(a.allNodes() == (QList<cwSurveyNode*>{b, c, d}));

    QList<const cwSurveyNode*> visited;
    a.walk([&visited](const cwSurveyNode* node) { visited.append(node); });
    CHECK(visited == (QList<const cwSurveyNode*>{&a, b, c, d}));
}

TEST_CASE("cwSurveyNode child names are unique per sibling set only", "[SurveyNode]")
{
    cwCave a;
    a.setName(QStringLiteral("A"));
    cwCave c;
    c.setName(QStringLiteral("C"));

    cwCave* underA = makeNode(QStringLiteral("B"));
    a.addNode(underA);
    cwCave* underC = makeNode(QStringLiteral("B"));
    c.addNode(underC);

    //"A › B" and "C › B" coexist: uniqueness is per sibling set.
    CHECK(underA->name() == QStringLiteral("B"));
    CHECK(underC->name() == QStringLiteral("B"));

    CHECK(a.uniqueChildName(QStringLiteral("B")) == QStringLiteral("B 2"));
    CHECK(c.uniqueChildName(QStringLiteral("B")) == QStringLiteral("B 2"));

    cwCave* secondUnderA = makeNode(QStringLiteral("B"));
    a.addNode(secondUnderA);
    CHECK(secondUnderA->name() == QStringLiteral("B 2"));

    //A rename into a sibling's name is refused...
    CHECK_FALSE(underA->validateName(QStringLiteral("B 2")).isEmpty());
    underA->setName(QStringLiteral("B 2"));
    CHECK(underA->name() == QStringLiteral("B"));

    //...while the other parent's child may take it.
    CHECK(underC->validateName(QStringLiteral("B 2")).isEmpty());
    underC->setName(QStringLiteral("B 2"));
    CHECK(underC->name() == QStringLiteral("B 2"));
}

TEST_CASE("cwSurveyNode relays fire once per level in a depth-3 tree", "[SurveyNode]")
{
    //The double-fire regression: a relay wired anywhere but connectNode, or one
    //that travels both up and down, multiplies with depth.
    cwCave a;
    a.setName(QStringLiteral("A"));

    QUndoStack undoStack;
    a.setUndoStack(&undoStack);

    cwCave* b = makeNode(QStringLiteral("B"));
    a.addNode(b);
    cwCave* c = makeNode(QStringLiteral("C"));
    b->addNode(c);

    cwSignalSpy aSubtree(&a, &cwSurveyNode::subtreeChanged);
    cwSignalSpy bSubtree(b, &cwSurveyNode::subtreeChanged);
    cwSignalSpy cSubtree(c, &cwSurveyNode::subtreeChanged);
    cwSignalSpy aScope(&a, &cwSurveyNode::scopeLabelsChanged);
    cwSignalSpy bScope(b, &cwSurveyNode::scopeLabelsChanged);
    cwSignalSpy cScope(c, &cwSurveyNode::scopeLabelsChanged);
    cwSignalSpy cTripLabels(c, &cwSurveyNode::tripScopeLabelsChanged);
    cwSignalSpy bChildLabels(b, &cwSurveyNode::childScopeLabelsChanged);
    cwSignalSpy aTripsDeleted(&a, &cwSurveyNode::tripsDeleted);
    cwSignalSpy bTripsDeleted(b, &cwSurveyNode::tripsDeleted);
    cwSignalSpy cTripsDeleted(c, &cwSurveyNode::tripsDeleted);
    cwSignalSpy aNodesDeleted(&a, &cwSurveyNode::nodesDeleted);
    cwSignalSpy bNodesDeleted(b, &cwSurveyNode::nodesDeleted);

    SECTION("a trip added at depth three pulses each level once") {
        cwTrip* trip = makeTrip(c, QStringLiteral("Topo 1"));

        CHECK(cSubtree.count() == 1);
        CHECK(bSubtree.count() == 1);
        CHECK(aSubtree.count() == 1);
        CHECK(cScope.count() == 1);
        CHECK(bScope.count() == 1);
        CHECK(aScope.count() == 1);
        CHECK(cTripLabels.count() == 1);
        //A trip is not a child node, so B's child sibling set did not move.
        CHECK(bChildLabels.count() == 0);

        trip->setName(QStringLiteral("Topo 2"));

        CHECK(cScope.count() == 2);
        CHECK(bScope.count() == 2);
        CHECK(aScope.count() == 2);
        CHECK(aSubtree.count() == 1);
    }

    SECTION("undo and redo leave exactly one connection per level") {
        makeTrip(c, QStringLiteral("Topo 1"));
        const int afterTrip = undoStack.index();
        REQUIRE(afterTrip > 0);

        undoStack.setIndex(0);
        REQUIRE(a.childNodeCount() == 0);
        undoStack.setIndex(afterTrip);
        REQUIRE(a.childNodeCount() == 1);
        REQUIRE(b->childNodeCount() == 1);
        REQUIRE(c->tripCount() == 1);

        cwSignalSpy aScopeAgain(&a, &cwSurveyNode::scopeLabelsChanged);
        cwSignalSpy bScopeAgain(b, &cwSurveyNode::scopeLabelsChanged);
        cwSignalSpy cScopeAgain(c, &cwSurveyNode::scopeLabelsChanged);

        c->trip(0)->setName(QStringLiteral("Topo 2"));

        CHECK(cScopeAgain.count() == 1);
        CHECK(bScopeAgain.count() == 1);
        CHECK(aScopeAgain.count() == 1);
    }

    SECTION("removeTrip carries the trip id up every level") {
        cwTrip* trip = makeTrip(c, QStringLiteral("Topo 1"));
        const QUuid tripId = trip->id();

        c->removeTrip(0);

        REQUIRE(cTripsDeleted.count() == 1);
        CHECK(cTripsDeleted.at(0).at(0).value<QList<QUuid>>()
              == QList<QUuid>{tripId});
        CHECK(bTripsDeleted.count() == 1);
        CHECK(aTripsDeleted.count() == 1);
        CHECK(aNodesDeleted.count() == 0);
    }

    SECTION("removeNode carries every owner id in the subtree up") {
        cwTrip* trip = makeTrip(c, QStringLiteral("Topo 1"));
        const QUuid tripId = trip->id();
        const QUuid nodeId = c->id();

        b->removeNode(0);

        REQUIRE(bNodesDeleted.count() == 1);
        const auto ids = bNodesDeleted.at(0).at(0).value<QList<QUuid>>();
        CHECK(ids.contains(nodeId));
        CHECK(ids.contains(tripId));
        CHECK(aNodesDeleted.count() == 1);
        CHECK(bTripsDeleted.count() == 0);
        CHECK(aTripsDeleted.count() == 0);
    }

    SECTION("a move says nothing about deletion") {
        a.insertNode(0, c);

        CHECK(c->parentNode() == &a);
        CHECK(b->childNodeCount() == 0);
        CHECK(a.childNodeCount() == 2);
        CHECK(bNodesDeleted.count() == 0);
        CHECK(aNodesDeleted.count() == 0);
    }

    SECTION("a node refuses itself and its own ancestors as a child") {
        //Either one closes a parent cycle, and every upward walk would run
        //forever after it.
        c->addNode(&a);
        CHECK(a.parentNode() == nullptr);
        CHECK(c->childNodeCount() == 0);

        c->addNode(b);
        CHECK(b->parentNode() == &a);
        CHECK(c->childNodeCount() == 0);

        a.addNode(&a);
        CHECK(a.childNodeCount() == 1);

        //The walks still terminate.
        CHECK(c->path() == QStringList({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")}));
    }
}

TEST_CASE("cwSurveyNode subtreeFixStationCount counts every fix at and below a node", "[SurveyNode]")
{
    //A -> B -> C, with the undo stack a user's edits run on.
    cwCave a;
    a.setName(QStringLiteral("A"));

    QUndoStack undoStack;
    a.setUndoStack(&undoStack);

    cwCave* b = makeNode(QStringLiteral("B"));
    a.addNode(b);
    cwCave* c = makeNode(QStringLiteral("C"));
    b->addNode(c);

    SECTION("a new node counts nothing") {
        cwCave fresh;
        CHECK(fresh.subtreeFixStationCount() == 0);
        CHECK(a.subtreeFixStationCount() == 0);
    }

    SECTION("an own fix raises the node and each ancestor once") {
        cwSignalSpy aChanged(&a, &cwSurveyNode::subtreeFixStationCountChanged);
        cwSignalSpy bChanged(b, &cwSurveyNode::subtreeFixStationCountChanged);
        cwSignalSpy cChanged(c, &cwSurveyNode::subtreeFixStationCountChanged);

        c->fixStations()->appendFixStation(fixAt(QStringLiteral("c1")));

        CHECK(c->subtreeFixStationCount() == 1);
        CHECK(b->subtreeFixStationCount() == 1);
        CHECK(a.subtreeFixStationCount() == 1);
        CHECK(cChanged.count() == 1);
        CHECK(bChanged.count() == 1);
        CHECK(aChanged.count() == 1);

        b->fixStations()->appendFixStation(fixAt(QStringLiteral("b1")));

        CHECK(c->subtreeFixStationCount() == 1);
        CHECK(b->subtreeFixStationCount() == 2);
        CHECK(a.subtreeFixStationCount() == 2);
        CHECK(cChanged.count() == 1);
        CHECK(bChanged.count() == 2);
        CHECK(aChanged.count() == 2);

        c->fixStations()->removeAt(0);

        CHECK(c->subtreeFixStationCount() == 0);
        CHECK(b->subtreeFixStationCount() == 1);
        CHECK(a.subtreeFixStationCount() == 1);
        CHECK(cChanged.count() == 2);
        CHECK(bChanged.count() == 3);
        CHECK(aChanged.count() == 3);
    }

    SECTION("a reset of the fix table to a new size lands on the new size") {
        c->fixStations()->setFixStations({fixAt(QStringLiteral("c1")),
                                          fixAt(QStringLiteral("c2")),
                                          fixAt(QStringLiteral("c3"))});
        CHECK(c->subtreeFixStationCount() == 3);
        CHECK(a.subtreeFixStationCount() == 3);

        c->fixStations()->setFixStations({fixAt(QStringLiteral("c1"))});
        CHECK(c->subtreeFixStationCount() == 1);
        CHECK(b->subtreeFixStationCount() == 1);
        CHECK(a.subtreeFixStationCount() == 1);
    }

    SECTION("a trip's attached fixes count toward its node and every ancestor") {
        cwTrip* trip = makeTrip(c, QStringLiteral("Topo 1"));

        trip->setAttachedFixes(attachedFixes(QStringLiteral("t"), 3));
        CHECK(c->subtreeFixStationCount() == 3);
        CHECK(b->subtreeFixStationCount() == 3);
        CHECK(a.subtreeFixStationCount() == 3);

        trip->setAttachedFixes(attachedFixes(QStringLiteral("t"), 1));
        CHECK(c->subtreeFixStationCount() == 1);
        CHECK(a.subtreeFixStationCount() == 1);

        trip->setAttachedFixes({});
        CHECK(c->subtreeFixStationCount() == 0);
        CHECK(b->subtreeFixStationCount() == 0);
        CHECK(a.subtreeFixStationCount() == 0);
    }

    SECTION("a node's own attached fixes count beside its own fixes") {
        c->fixStations()->appendFixStation(fixAt(QStringLiteral("c1")));
        c->setAttachedFixes(attachedFixes(QStringLiteral("n"), 2));
        CHECK(c->subtreeFixStationCount() == 3);
        CHECK(a.subtreeFixStationCount() == 3);

        c->setAttachedFixes({});
        CHECK(c->subtreeFixStationCount() == 1);
        CHECK(b->subtreeFixStationCount() == 1);
        CHECK(a.subtreeFixStationCount() == 1);
    }

    SECTION("adding a child that holds fixes raises the parent, removing it lowers it") {
        cwCave* d = makeNode(QStringLiteral("D"));
        d->fixStations()->appendFixStation(fixAt(QStringLiteral("d1")));
        d->fixStations()->appendFixStation(fixAt(QStringLiteral("d2")));
        REQUIRE(d->subtreeFixStationCount() == 2);

        b->addNode(d);
        CHECK(b->subtreeFixStationCount() == 2);
        CHECK(a.subtreeFixStationCount() == 2);

        b->removeNode(b->indexOfNode(d));
        CHECK(d->subtreeFixStationCount() == 2);
        CHECK(b->subtreeFixStationCount() == 0);
        CHECK(a.subtreeFixStationCount() == 0);
    }

    SECTION("a child removed for undo stays out of its old parent's count until the undo") {
        c->fixStations()->appendFixStation(fixAt(QStringLiteral("c1")));
        REQUIRE(a.subtreeFixStationCount() == 1);

        b->removeNode(b->indexOfNode(c));
        REQUIRE(b->childNodeCount() == 0);
        CHECK(b->subtreeFixStationCount() == 0);
        CHECK(a.subtreeFixStationCount() == 0);

        cwSignalSpy aChanged(&a, &cwSurveyNode::subtreeFixStationCountChanged);
        cwSignalSpy bChanged(b, &cwSurveyNode::subtreeFixStationCountChanged);

        //The removed node keeps its own count current, and its old parent
        //hears nothing of it.
        c->fixStations()->appendFixStation(fixAt(QStringLiteral("c2")));
        CHECK(c->subtreeFixStationCount() == 2);
        CHECK(b->subtreeFixStationCount() == 0);
        CHECK(a.subtreeFixStationCount() == 0);
        CHECK(aChanged.count() == 0);
        CHECK(bChanged.count() == 0);

        undoStack.undo();
        REQUIRE(c->parentNode() == b);
        CHECK(b->subtreeFixStationCount() == 2);
        CHECK(a.subtreeFixStationCount() == 2);
    }

    SECTION("removing a trip takes its attached fixes out, and undo brings them back") {
        cwTrip* trip = makeTrip(c, QStringLiteral("Topo 1"));
        trip->setAttachedFixes(attachedFixes(QStringLiteral("t"), 2));
        REQUIRE(a.subtreeFixStationCount() == 2);

        c->removeTrip(c->trips().indexOf(trip));
        CHECK(c->subtreeFixStationCount() == 0);
        CHECK(a.subtreeFixStationCount() == 0);

        undoStack.undo();
        REQUIRE(c->tripCount() == 1);
        CHECK(c->subtreeFixStationCount() == 2);
        CHECK(a.subtreeFixStationCount() == 2);
    }
}

TEST_CASE("cwSurveyNode addNodes raises the parent's fix count once for the whole batch", "[SurveyNode]")
{
    //The loader's path: subtrees built off to the side, then handed over at once.
    cwCave root;
    root.setName(QStringLiteral("Root"));

    cwCave* first = makeNode(QStringLiteral("First"));
    first->fixStations()->appendFixStation(fixAt(QStringLiteral("f1")));

    cwCave* second = makeNode(QStringLiteral("Second"));
    second->fixStations()->appendFixStation(fixAt(QStringLiteral("s1")));
    second->fixStations()->appendFixStation(fixAt(QStringLiteral("s2")));
    cwCave* nested = makeNode(QStringLiteral("Nested"));
    nested->fixStations()->appendFixStation(fixAt(QStringLiteral("n1")));
    second->addNode(nested);
    REQUIRE(second->subtreeFixStationCount() == 3);

    cwSignalSpy rootChanged(&root, &cwSurveyNode::subtreeFixStationCountChanged);

    root.addNodes({first, second});

    CHECK(root.subtreeFixStationCount() == 4);
    CHECK(rootChanged.count() == 1);
}

namespace {
    QList<cwTrip*> unparentedTrips(const QStringList& names)
    {
        QList<cwTrip*> trips;
        for(const QString& name : names) {
            auto* trip = new cwTrip();
            trip->setName(name);
            trips.append(trip);
        }
        return trips;
    }

    QStringList tripNamesOf(const cwSurveyNode& node)
    {
        QStringList names;
        for(const cwTrip* trip : node.trips()) {
            names.append(trip->name());
        }
        return names;
    }
}

TEST_CASE("cwSurveyNode addTrips appends a batch of trips as one insert and one undo step", "[SurveyNode]")
{
    cwCave node;
    node.setName(QStringLiteral("Node"));
    QUndoStack undoStack;
    node.setUndoStack(&undoStack);
    makeTrip(&node, QStringLiteral("T"));
    REQUIRE(undoStack.count() == 1);

    SECTION("fresh trips land in one ranged insert, deduped against the node and each other") {
        const QList<cwTrip*> trips =
            unparentedTrips({QStringLiteral("T"), QStringLiteral("T"), QStringLiteral("U")});

        QSignalSpy inserted(&node, &cwSurveyNode::insertedTrips);
        QSignalSpy beginInserted(&node, &cwSurveyNode::beginInsertTrips);
        QSignalSpy tripCountChanged(&node, &cwSurveyNode::tripCountChanged);
        QSignalSpy subtreeChanged(&node, &cwSurveyNode::subtreeChanged);

        node.addTrips(trips);

        CHECK(tripNamesOf(node) == QStringList({QStringLiteral("T"), QStringLiteral("T 2"),
                                                QStringLiteral("T 3"), QStringLiteral("U")}));
        REQUIRE(inserted.count() == 1);
        CHECK(inserted.at(0).at(0).toInt() == 1);
        CHECK(inserted.at(0).at(1).toInt() == 3);
        CHECK(beginInserted.count() == 1);
        CHECK(tripCountChanged.count() == 1);
        CHECK(subtreeChanged.count() == 1);
        for(const cwTrip* trip : trips) {
            CHECK(trip->parentNode() == &node);
        }

        CHECK(undoStack.count() == 2);
        undoStack.undo();
        CHECK(tripNamesOf(node) == QStringList({QStringLiteral("T")}));
        undoStack.redo();
        CHECK(node.tripCount() == 4);
    }

    SECTION("a trip another node lists moves here") {
        cwCave other;
        other.setName(QStringLiteral("Other"));
        cwTrip* moving = makeTrip(&other, QStringLiteral("Moving"));
        const QList<cwTrip*> trips = {moving, unparentedTrips({QStringLiteral("Fresh")}).first()};

        node.addTrips(trips);

        CHECK(other.tripCount() == 0);
        CHECK(tripNamesOf(node) == QStringList({QStringLiteral("T"), QStringLiteral("Moving"),
                                                QStringLiteral("Fresh")}));
        CHECK(moving->parentNode() == &node);
    }
}

namespace {
    constexpr int kFillTripCount = 50;
    constexpr int kFillGrowthFactor = 4;

    cwCaveData caveDataWithTrips(const QStringList& tripNames)
    {
        cwCaveData data;
        data.name = QStringLiteral("Filled Cave");
        for(const QString& name : tripNames) {
            cwTripData tripData;
            tripData.name = name;
            data.trips.append(tripData);
        }
        return data;
    }

    QStringList numberedTripNames(int count)
    {
        QStringList names;
        names.reserve(count);
        for(int i = 0; i < count; ++i) {
            names.append(QStringLiteral("Trip %1").arg(i + 1));
        }
        return names;
    }

    //What one setData() fill announces: the node's tripScopeLabelsChanged
    //pulses, its insertedTrips ranges, and the scopeChanged each filled trip
    //hears from its own insert onward.
    struct TripFillSignals {
        qsizetype tripScopeLabelsPulses = 0;
        QList<QList<QVariant>> insertedRanges;
        qsizetype firstTripScopeChanges = 0;
        qsizetype allTripScopeChanges = 0;
    };

    TripFillSignals fillAndCount(cwSurveyNode* node, const cwCaveData& data)
    {
        //setData() builds the trips itself, so each trip's spy attaches when
        //the node announces that trip's insert.
        std::vector<std::unique_ptr<QSignalSpy>> tripSpies;
        const QMetaObject::Connection spyAttacher =
            QObject::connect(node, &cwSurveyNode::insertedTrips, node,
                             [node, &tripSpies](int begin, int end) {
                                 for(int i = begin; i <= end; ++i) {
                                     tripSpies.push_back(std::make_unique<QSignalSpy>(
                                         node->trip(i), &cwTrip::scopeChanged));
                                 }
                             });
        QSignalSpy pulses(node, &cwSurveyNode::tripScopeLabelsChanged);
        QSignalSpy inserted(node, &cwSurveyNode::insertedTrips);

        node->setData(data);
        QObject::disconnect(spyAttacher);

        TripFillSignals result;
        result.tripScopeLabelsPulses = pulses.count();
        for(const QList<QVariant>& range : std::as_const(inserted)) {
            result.insertedRanges.append(range);
        }
        if(!tripSpies.empty()) {
            result.firstTripScopeChanges = tripSpies.front()->count();
        }
        for(const auto& spy : tripSpies) {
            result.allTripScopeChanges += spy->count();
        }
        return result;
    }
}

TEST_CASE("cwSurveyNode setData inserts its trips as one range", "[SurveyNode]")
{
    SECTION("one scope-label pulse per fill, whatever the trip count") {
        const QStringList smallNames = numberedTripNames(kFillTripCount);
        const QStringList largeNames = numberedTripNames(kFillTripCount * kFillGrowthFactor);

        cwCave smallCave;
        const TripFillSignals smallFill = fillAndCount(&smallCave, caveDataWithTrips(smallNames));
        cwCave largeCave;
        const TripFillSignals largeFill = fillAndCount(&largeCave, caveDataWithTrips(largeNames));

        CHECK(tripNamesOf(smallCave) == smallNames);
        CHECK(tripNamesOf(largeCave) == largeNames);

        CHECK(smallFill.tripScopeLabelsPulses == 1);
        CHECK(largeFill.tripScopeLabelsPulses == 1);

        CHECK(smallFill.insertedRanges.size() == 1);
        REQUIRE_FALSE(smallFill.insertedRanges.isEmpty());
        CHECK(smallFill.insertedRanges.first().at(0).toInt() == 0);
        CHECK(smallFill.insertedRanges.first().at(1).toInt() == kFillTripCount - 1);

        CHECK(smallFill.firstTripScopeChanges == 1);
        CHECK(largeFill.firstTripScopeChanges == 1);

        //Every trip hears the fill once: activations grow with the trip count,
        //not with its square.
        CHECK(smallFill.allTripScopeChanges == smallNames.size());
        CHECK(largeFill.allTripScopeChanges
              <= kFillGrowthFactor * smallFill.allTripScopeChanges);
    }

    SECTION("duplicate input names are deduped in input order") {
        cwCave node;
        const TripFillSignals fill = fillAndCount(
            &node, caveDataWithTrips({QStringLiteral("A"), QStringLiteral("A"), QStringLiteral("B")}));

        CHECK(tripNamesOf(node) == QStringList({QStringLiteral("A"), QStringLiteral("A 2"),
                                                QStringLiteral("B")}));
        CHECK(fill.tripScopeLabelsPulses == 1);
        for(const cwTrip* trip : node.trips()) {
            CHECK(trip->parentNode() == &node);
        }
    }

    SECTION("a refill replaces the trips with one pulse count at any size") {
        const QStringList firstNames({QStringLiteral("First"), QStringLiteral("First"),
                                      QStringLiteral("Other")});
        const QStringList smallNames = numberedTripNames(kFillTripCount);
        const QStringList largeNames = numberedTripNames(kFillTripCount * kFillGrowthFactor);

        cwCave smallCave;
        fillAndCount(&smallCave, caveDataWithTrips(firstNames));
        CHECK(tripNamesOf(smallCave) == QStringList({QStringLiteral("First"), QStringLiteral("First 2"),
                                                 QStringLiteral("Other")}));
        cwCave largeCave;
        fillAndCount(&largeCave, caveDataWithTrips(firstNames));

        const TripFillSignals smallRefill = fillAndCount(&smallCave, caveDataWithTrips(smallNames));
        const TripFillSignals largeRefill = fillAndCount(&largeCave, caveDataWithTrips(largeNames));

        CHECK(tripNamesOf(smallCave) == smallNames);
        CHECK(tripNamesOf(largeCave) == largeNames);
        CHECK(smallRefill.tripScopeLabelsPulses == largeRefill.tripScopeLabelsPulses);
        CHECK(smallRefill.firstTripScopeChanges == 1);
        CHECK(largeRefill.firstTripScopeChanges == 1);
    }
}

TEST_CASE("cwSurveyNode fix count survives a destroyed subtree", "[SurveyNode]")
{
    cwCave grandparent;
    grandparent.setName(QStringLiteral("Grandparent"));

    cwCave* parent = makeNode(QStringLiteral("Parent"));
    grandparent.addNode(parent);
    cwCave* child = makeNode(QStringLiteral("Child"));
    parent->addNode(child);
    child->fixStations()->appendFixStation(fixAt(QStringLiteral("c1")));
    cwTrip* trip = makeTrip(child, QStringLiteral("Topo 1"));
    trip->setAttachedFixes(attachedFixes(QStringLiteral("t"), 2));
    REQUIRE(grandparent.subtreeFixStationCount() == 3);

    QPointer<cwSurveyNode> parentPtr(parent);
    QPointer<cwSurveyNode> childPtr(child);

    SECTION("with no undo stack the removal destroys the subtree") {
        cwSignalSpy grandparentChanged(&grandparent, &cwSurveyNode::subtreeFixStationCountChanged);

        grandparent.removeNode(0);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        CHECK(parentPtr.isNull());
        CHECK(childPtr.isNull());
        CHECK(grandparent.subtreeFixStationCount() == 0);
        CHECK(grandparentChanged.count() == 1);
    }

    SECTION("a cleared undo stack destroys a removed subtree without a word upward") {
        QUndoStack undoStack;
        grandparent.setUndoStack(&undoStack);

        grandparent.removeNode(0);
        REQUIRE_FALSE(parentPtr.isNull());
        REQUIRE(grandparent.subtreeFixStationCount() == 0);

        cwSignalSpy grandparentChanged(&grandparent, &cwSurveyNode::subtreeFixStationCountChanged);
        undoStack.clear();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        CHECK(parentPtr.isNull());
        CHECK(childPtr.isNull());
        CHECK(grandparent.subtreeFixStationCount() == 0);
        CHECK(grandparentChanged.count() == 0);
    }
}

TEST_CASE("cwSurveyNode externallyBacked propagates down the tree", "[SurveyNode]")
{
    cwCave a;
    a.setName(QStringLiteral("A"));
    cwCave* b = makeNode(QStringLiteral("B"));
    a.addNode(b);
    cwCave* c = makeNode(QStringLiteral("C"));
    b->addNode(c);
    cwTrip* trip = makeTrip(c, QStringLiteral("Topo 1"));

    cwSignalSpy cBacked(c, &cwSurveyNode::externallyBackedChanged);
    cwSignalSpy tripBacked(trip, &cwTrip::externallyBackedChanged);

    CHECK_FALSE(c->externallyBacked());
    CHECK_FALSE(trip->externallyBacked());

    a.setExternalCenterline(cwExternalCenterline(QStringLiteral("x.svx")));
    CHECK(cBacked.count() == 1);
    CHECK(tripBacked.count() == 1);
    CHECK(a.externallyBacked());
    CHECK(c->externallyBacked());
    CHECK(trip->externallyBacked());
    //The attachment itself stays where it was made.
    CHECK(b->externalCenterline().isEmpty());

    //Swapping one attachment for another moves no answer, so nothing pulses.
    a.setExternalCenterline(cwExternalCenterline(QStringLiteral("y.svx")));
    CHECK(cBacked.count() == 1);
    CHECK(tripBacked.count() == 1);

    a.setExternalCenterline(cwExternalCenterline());
    CHECK(cBacked.count() == 2);
    CHECK(tripBacked.count() == 2);
    CHECK_FALSE(c->externallyBacked());
    CHECK_FALSE(trip->externallyBacked());
}

TEST_CASE("cwSurveyNode says so when a moved node inherits a new backing",
          "[SurveyNode]")
{
    //A subtree that changes parents inherits the new parent's attachment, so the
    //move itself is what moves the answer.
    cwCave a;
    a.setName(QStringLiteral("A"));
    a.setExternalCenterline(cwExternalCenterline(QStringLiteral("x.svx")));

    cwCave native;
    native.setName(QStringLiteral("Native"));
    cwCave* c = makeNode(QStringLiteral("C"));
    native.addNode(c);
    cwTrip* trip = makeTrip(c, QStringLiteral("Topo 1"));

    cwSignalSpy cBacked(c, &cwSurveyNode::externallyBackedChanged);
    cwSignalSpy tripBacked(trip, &cwTrip::externallyBackedChanged);

    CHECK_FALSE(c->externallyBacked());

    a.addNode(c);

    CHECK(c->parentNode() == &a);
    CHECK(c->externallyBacked());
    CHECK(trip->externallyBacked());
    CHECK(cBacked.count() == 1);
    CHECK(tripBacked.count() == 1);

    //Moving it under a second attached node leaves the answer where it was.
    cwCave second;
    second.setName(QStringLiteral("Second"));
    second.setExternalCenterline(cwExternalCenterline(QStringLiteral("y.svx")));
    second.addNode(c);
    CHECK(cBacked.count() == 1);
    CHECK(tripBacked.count() == 1);
}

TEST_CASE("cwSurveyNode sourceRoot and lowestCommonAncestor", "[SurveyNode]")
{
    cwCave a;
    a.setName(QStringLiteral("A"));
    cwCave* b = makeNode(QStringLiteral("B"));
    a.addNode(b);
    cwCave* c = makeNode(QStringLiteral("C"));
    a.addNode(c);

    SECTION("a native chain has no source root") {
        CHECK(a.sourceRoot() == nullptr);
        CHECK(b->sourceRoot() == nullptr);
        CHECK_FALSE(a.isSourced());
        CHECK_FALSE(a.isSourceRoot());
    }

    SECTION("the nearest sourced ancestor with no source path is the root") {
        cwSignalSpy aSource(&a, &cwSurveyNode::sourceChanged);

        const QUuid sourceId = QUuid::createUuid();
        a.setSourceId(sourceId);
        CHECK(aSource.count() == 1);
        CHECK(a.isSourced());
        CHECK(a.isSourceRoot());

        //A same-value set says nothing.
        a.setSourceId(sourceId);
        CHECK(aSource.count() == 1);

        b->setSourceId(sourceId);
        b->setSourcePath(QStringLiteral("b"));
        CHECK(b->isSourced());
        CHECK_FALSE(b->isSourceRoot());
        CHECK(b->sourceRoot() == &a);
        CHECK(a.sourceRoot() == &a);
    }

    SECTION("lowestCommonAncestor") {
        cwCave unrelated;
        unrelated.setName(QStringLiteral("Unrelated"));

        CHECK(b->lowestCommonAncestor(c) == &a);
        CHECK(a.lowestCommonAncestor(&a) == &a);
        CHECK(a.lowestCommonAncestor(b) == &a);
        CHECK(b->lowestCommonAncestor(&unrelated) == nullptr);
        CHECK(b->lowestCommonAncestor(nullptr) == nullptr);
    }
}

TEST_CASE("cwTrip::parentNode is the storage and parentCave the shim", "[SurveyNode]")
{
    cwCave cave;
    cave.setName(QStringLiteral("Fisher Ridge"));

    auto* trip = new cwTrip();
    trip->setName(QStringLiteral("Topo 1"));

    cwSignalSpy nodeSpy(trip, &cwTrip::parentNodeChanged);
    cwSignalSpy caveSpy(trip, &cwTrip::parentCaveChanged);

    cave.addTrip(trip);

    CHECK(trip->parentNode() == &cave);
    CHECK(trip->parentCave() == &cave);
    CHECK(nodeSpy.count() == 1);
    CHECK(caveSpy.count() == 1);
}

TEST_CASE("MoveNodeCommand restores the parent, row and name it left", "[SurveyNode]")
{
    QUndoStack undoStack;

    cwCave first;
    first.setName(QStringLiteral("First"));
    first.setUndoStack(&undoStack);

    cwCave second;
    second.setName(QStringLiteral("Second"));
    second.setUndoStack(&undoStack);

    cwCave* alpha = makeNode(QStringLiteral("Alpha"));
    cwCave* beta = makeNode(QStringLiteral("Beta"));
    cwCave* gamma = makeNode(QStringLiteral("Gamma"));
    first.addNode(alpha);
    first.addNode(beta);
    first.addNode(gamma);

    cwTrip* betaTrip = makeTrip(beta, QStringLiteral("Topo 1"));

    SECTION("a move to another parent is one undo step") {
        const int beforeMove = undoStack.count();

        second.insertNode(0, beta);

        CHECK(undoStack.count() == beforeMove + 1);
        CHECK(beta->parentNode() == &second);
        CHECK(second.indexOfNode(beta) == 0);
        CHECK(first.childNodes() == QList<cwSurveyNode*>({alpha, gamma}));
        CHECK(beta->trip(0) == betaTrip);
        CHECK_FALSE(first.childNameSet().contains(QStringLiteral("Beta")));
        CHECK(second.childNameSet().contains(QStringLiteral("Beta")));

        undoStack.undo();

        //Back at the parent AND the row it left, which is what an insert
        //command's undo cannot do.
        CHECK(beta->parentNode() == &first);
        CHECK(first.childNodes() == QList<cwSurveyNode*>({alpha, beta, gamma}));
        CHECK(second.childNodeCount() == 0);
        CHECK(beta->trip(0) == betaTrip);
        CHECK(first.childNameSet().contains(QStringLiteral("Beta")));
        CHECK_FALSE(second.childNameSet().contains(QStringLiteral("Beta")));

        undoStack.redo();

        CHECK(beta->parentNode() == &second);
        CHECK(second.indexOfNode(beta) == 0);
        CHECK(first.childNodes() == QList<cwSurveyNode*>({alpha, gamma}));
    }

    SECTION("the new siblings' names win, and undo gives the old name back") {
        cwCave* collision = makeNode(QStringLiteral("Beta"));
        second.addNode(collision);

        second.addNode(beta);

        CHECK(collision->name() == QStringLiteral("Beta"));
        CHECK(beta->name() != QStringLiteral("Beta"));
        const QString dedupedName = beta->name();
        CHECK(second.childNameSet().contains(dedupedName));

        undoStack.undo();

        CHECK(beta->name() == QStringLiteral("Beta"));
        CHECK(first.childNameSet().contains(QStringLiteral("Beta")));
        CHECK_FALSE(second.childNameSet().contains(dedupedName));
    }

    SECTION("a move says so, and never says the subtree was deleted") {
        cwSignalSpy beginMove(beta, &cwSurveyNode::beginMoveNode);
        cwSignalSpy moved(beta, &cwSurveyNode::nodeMoved);
        cwSignalSpy deleted(&first, &cwSurveyNode::nodesDeleted);

        second.addNode(beta);

        CHECK(beginMove.count() == 1);
        CHECK(moved.count() == 1);
        CHECK(deleted.count() == 0);

        undoStack.undo();

        CHECK(beginMove.count() == 2);
        CHECK(moved.count() == 2);
        CHECK(deleted.count() == 0);
    }

    SECTION("a move within one parent reorders the rows") {
        first.insertNode(2, alpha);

        CHECK(first.childNodes() == QList<cwSurveyNode*>({beta, gamma, alpha}));
        CHECK(alpha->parentNode() == &first);

        undoStack.undo();

        CHECK(first.childNodes() == QList<cwSurveyNode*>({alpha, beta, gamma}));
    }

    SECTION("a node refuses to land inside its own subtree") {
        cwCave* child = makeNode(QStringLiteral("Child"));
        beta->addNode(child);

        const int beforeMove = undoStack.count();
        child->insertNode(0, beta);

        CHECK(undoStack.count() == beforeMove);
        CHECK(beta->parentNode() == &first);
        CHECK(child->parentNode() == beta);
    }
}

TEST_CASE("cwSurveyNode isSection names a Folder inside a cave", "[SurveyNode]")
{
    using Kind = cwSurveyNode::Kind;

    cwCavingRegion region;
    cwSurveyNode* root = region.rootNode();
    CHECK_FALSE(root->isSection());

    cwCave* topFolder = makeNode(QStringLiteral("Kentucky"), Kind::Folder);
    root->addNode(topFolder);
    cwCave* cave = makeNode(QStringLiteral("Side Cave"), Kind::Cave);
    topFolder->addNode(cave);
    cwCave* section = makeNode(QStringLiteral("Upper level"), Kind::Folder);
    cave->addNode(section);
    cwCave* innerSection = makeNode(QStringLiteral("Crawl"), Kind::Folder);
    section->addNode(innerSection);

    CHECK_FALSE(topFolder->isSection());
    CHECK_FALSE(cave->isSection());
    CHECK(section->isSection());
    CHECK(innerSection->isSection());

    SECTION("relabeling the cave turns its sections into folders") {
        cwSignalSpy sectionSpy(section, &cwSurveyNode::isSectionChanged);
        cwSignalSpy innerSpy(innerSection, &cwSurveyNode::isSectionChanged);

        cave->setKind(Kind::Folder);
        CHECK_FALSE(cave->isSection());
        CHECK_FALSE(section->isSection());
        CHECK_FALSE(innerSection->isSection());
        CHECK(sectionSpy.count() == 1);
        CHECK(innerSpy.count() == 1);

        cave->setKind(Kind::Cave);
        CHECK(section->isSection());
        CHECK(innerSection->isSection());
        CHECK(sectionSpy.count() == 2);
    }

    SECTION("relabeling a section as a cave leaves it a cave") {
        section->setKind(Kind::Cave);
        CHECK_FALSE(section->isSection());
        CHECK(innerSection->isSection());
    }

    SECTION("a native Folder under a sourced node is a section") {
        cwCave* sourced = makeNode(QStringLiteral("Survex file"), Kind::SurvexFile);
        root->addNode(sourced);
        cwCave* nativeFolder = makeNode(QStringLiteral("Native"), Kind::Folder);
        sourced->addNode(nativeFolder);
        CHECK_FALSE(nativeFolder->isSection());

        sourced->setSourceId(QUuid::createUuid());
        CHECK(nativeFolder->isSection());
        CHECK_FALSE(sourced->isSection());
    }
}

TEST_CASE("cwSurveyNode takesCaves follows position, not a question", "[SurveyNode]")
{
    using Kind = cwSurveyNode::Kind;

    cwCavingRegion region;
    cwSurveyNode* root = region.rootNode();
    CHECK(root->takesCaves());

    cwCave* topFolder = makeNode(QStringLiteral("Kentucky"), Kind::Folder);
    root->addNode(topFolder);
    cwCave* innerFolder = makeNode(QStringLiteral("Field seasons"), Kind::Folder);
    topFolder->addNode(innerFolder);
    cwCave* cave = makeNode(QStringLiteral("Side Cave"), Kind::Cave);
    innerFolder->addNode(cave);
    cwCave* section = makeNode(QStringLiteral("Upper level"), Kind::Folder);
    cave->addNode(section);

    //Outside every cave, a Folder takes caves; a Cave and anything inside it
    //takes trips and sections.
    CHECK(topFolder->takesCaves());
    CHECK(innerFolder->takesCaves());
    CHECK_FALSE(cave->takesCaves());
    CHECK_FALSE(section->takesCaves());

    SECTION("relabeling a node moves the answer for its whole subtree") {
        cwSignalSpy caveSpy(cave, &cwSurveyNode::takesCavesChanged);
        cwSignalSpy sectionSpy(section, &cwSurveyNode::takesCavesChanged);

        cave->setKind(Kind::Folder);
        CHECK(cave->takesCaves());
        CHECK(section->takesCaves());
        CHECK(caveSpy.count() == 1);
        CHECK(sectionSpy.count() == 1);

        cave->setKind(Kind::Cave);
        CHECK_FALSE(cave->takesCaves());
        CHECK_FALSE(section->takesCaves());
        CHECK(caveSpy.count() == 2);
        CHECK(sectionSpy.count() == 2);
    }

    SECTION("relabeling a top Folder as a Cave turns every Folder under it into a section") {
        topFolder->setKind(Kind::Cave);
        CHECK_FALSE(topFolder->takesCaves());
        CHECK_FALSE(innerFolder->takesCaves());
        CHECK_FALSE(section->takesCaves());
        CHECK(root->takesCaves());
    }

    SECTION("a sourced node and everything under it takes trips") {
        cwCave* sourcedFolder = makeNode(QStringLiteral("Survex file"), Kind::Folder);
        root->addNode(sourcedFolder);
        cwCave* nativeFolderUnderSource = makeNode(QStringLiteral("Native"), Kind::Folder);
        sourcedFolder->addNode(nativeFolderUnderSource);
        CHECK(sourcedFolder->takesCaves());
        CHECK(nativeFolderUnderSource->takesCaves());

        sourcedFolder->setSourceId(QUuid::createUuid());
        CHECK_FALSE(sourcedFolder->takesCaves());
        CHECK_FALSE(nativeFolderUnderSource->takesCaves());
    }

    SECTION("a move re-answers for the moved subtree") {
        cwSignalSpy sectionSpy(section, &cwSurveyNode::takesCavesChanged);

        region.moveNode(section, topFolder, 0);
        REQUIRE(section->parentNode() == topFolder);
        CHECK(section->takesCaves());
        CHECK(sectionSpy.count() == 1);
    }
}

TEST_CASE("cwCavingRegion addNode names new nodes New <Kind>, unique among siblings",
          "[SurveyNode]")
{
    using Kind = cwSurveyNode::Kind;

    cwCavingRegion region;
    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    cwSurveyNode* firstCave = region.addNode(nullptr, Kind::Cave);
    cwSurveyNode* secondCave = region.addNode(nullptr, Kind::Cave);
    cwSurveyNode* folder = region.addNode(nullptr, Kind::Folder);
    REQUIRE(firstCave != nullptr);
    REQUIRE(secondCave != nullptr);
    REQUIRE(folder != nullptr);
    CHECK(firstCave->name() == QStringLiteral("New Cave"));
    CHECK(secondCave->name() == QStringLiteral("New Cave 2"));
    CHECK(folder->name() == QStringLiteral("New Folder"));

    //Add Section is a Folder named for where it sits, still one undo step.
    const int undoCountBefore = undoStack.count();
    cwSurveyNode* section = region.addNode(firstCave, Kind::Folder, QStringLiteral("New Section"));
    cwSurveyNode* secondSection = region.addNode(firstCave, Kind::Folder, QStringLiteral("New Section"));
    REQUIRE(section != nullptr);
    REQUIRE(secondSection != nullptr);
    CHECK(section->name() == QStringLiteral("New Section"));
    CHECK(section->kind() == Kind::Folder);
    CHECK(section->parentNode() == firstCave);
    CHECK(secondSection->name() == QStringLiteral("New Section 2"));
    CHECK(undoStack.count() == undoCountBefore + 2);

    //Uniqueness is per sibling set: another cave's first section takes the bare name.
    cwSurveyNode* otherSection = region.addNode(secondCave, Kind::Folder, QStringLiteral("New Section"));
    REQUIRE(otherSection != nullptr);
    CHECK(otherSection->name() == QStringLiteral("New Section"));

    undoStack.undo();
    CHECK(secondCave->childNodeCount() == 0);
}

TEST_CASE("cwSurveyNode isSelfOrDescendantOf follows the listed parent chain", "[SurveyNode]")
{
    cwCave a;
    a.setName(QStringLiteral("A"));
    cwCave* b = makeNode(QStringLiteral("B"));
    a.addNode(b);
    cwCave* c = makeNode(QStringLiteral("C"));
    b->addNode(c);
    cwCave* sibling = makeNode(QStringLiteral("Sibling"));
    a.addNode(sibling);

    CHECK(a.isSelfOrDescendantOf(&a));
    CHECK(b->isSelfOrDescendantOf(&a));
    CHECK(c->isSelfOrDescendantOf(&a));
    CHECK(c->isSelfOrDescendantOf(b));

    CHECK_FALSE(sibling->isSelfOrDescendantOf(b));
    CHECK_FALSE(c->isSelfOrDescendantOf(sibling));
    CHECK_FALSE(a.isSelfOrDescendantOf(b));
    CHECK_FALSE(b->isSelfOrDescendantOf(c));
    CHECK_FALSE(a.isSelfOrDescendantOf(nullptr));

    SECTION("a node removed for undo sits outside its old parent until undone") {
        QUndoStack undoStack;
        a.setUndoStack(&undoStack);

        a.removeNode(a.indexOfNode(b));
        REQUIRE(b->parentNode() == &a);
        CHECK_FALSE(b->isSelfOrDescendantOf(&a));
        CHECK_FALSE(c->isSelfOrDescendantOf(&a));
        CHECK(c->isSelfOrDescendantOf(b));

        undoStack.undo();
        CHECK(b->isSelfOrDescendantOf(&a));
        CHECK(c->isSelfOrDescendantOf(&a));
    }
}
