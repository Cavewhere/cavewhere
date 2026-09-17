//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "SignalSpyChecker.h"
#include "cwCave.h"
#include "cwSignalSpy.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"

//Qt includes
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QUndoStack>
#include <QUuid>

namespace {
    //Every node the production code builds is a cwCave, so the tests build one
    //too: the shim is what keeps qobject_cast<cwCave*> answering for any node.
    cwCave* makeNode(const QString& name)
    {
        auto* node = new cwCave();
        node->setName(name);
        return node;
    }

    cwTrip* makeTrip(cwSurveyNode* node, const QString& name)
    {
        auto* trip = new cwTrip();
        trip->setName(name);
        node->addTrip(trip);
        return trip;
    }
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
