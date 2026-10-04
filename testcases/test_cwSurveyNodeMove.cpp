//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwEquateModel.h"
#include "cwFixStationModel.h"
#include "cwNote.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNode.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "LoadProjectHelper.h"
#include "SurveyTreeTestHelper.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QUndoStack>

namespace {

cwSurveyNode* addNode(cwCavingRegion& region, cwSurveyNode* parent,
                      cwSurveyNode::Kind kind, const QString& name)
{
    cwSurveyNode* node = region.addNode(parent, kind, name);
    REQUIRE(node != nullptr);
    return node;
}

//! A native trip under \a node whose stations run along \a stations, one shot
//! per neighboring pair.
cwTrip* addTrip(cwSurveyNode* node, const QString& name, const QStringList& stations)
{
    auto* trip = new cwTrip();
    trip->setName(name);
    node->addTrip(trip);

    auto* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    for(int i = 1; i < stations.size(); ++i) {
        cwShot shot;
        shot.setDistance(cwDistanceReading(QStringLiteral("10.0")));
        shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
        shot.setClino(cwClinoReading(QStringLiteral("0.0")));
        chunk->appendShot(cwStation(stations.at(i - 1)), cwStation(stations.at(i)), shot);
    }
    return trip;
}

cwFixStation fixAt(const QString& stationName)
{
    cwFixStation fix;
    fix.setStationName(stationName);
    return fix;
}

QStringList fixNames(const cwSurveyNode* node)
{
    QStringList names;
    for(const cwFixStation& fix : node->fixStations()->fixStations()) {
        names.append(fix.stationName());
    }
    return names;
}

cwEquate tie(const cwSurveyNode* first, const QString& firstTail,
             const cwSurveyNode* second, const QString& secondTail)
{
    return cwEquate(QList<cwStationHandle>({
        cwStationHandle(cwStationHandle::NativeCave, first->id(), firstTail),
        cwStationHandle(cwStationHandle::NativeCave, second->id(), secondTail)}));
}

//! Kentucky (Folder) › Side Cave (Cave) › Upper level (Folder: a Section),
//! plus a top-level Cave, Other Cave. Side Cave holds Trip 1 (a1 a2 a3) and
//! Trip 2 (a2 b1); Upper level holds Trip 3 (a3 s1).
struct Tree {
    QUndoStack undoStack;
    cwCavingRegion region;
    cwSurveyNode* kentucky = nullptr;
    cwSurveyNode* sideCave = nullptr;
    cwSurveyNode* upperLevel = nullptr;
    cwSurveyNode* otherCave = nullptr;
    cwTrip* trip1 = nullptr;
    cwTrip* trip2 = nullptr;
    cwTrip* trip3 = nullptr;

    Tree()
    {
        region.setUndoStack(&undoStack);
        kentucky = addNode(region, nullptr, cwSurveyNode::Kind::Folder, QStringLiteral("Kentucky"));
        sideCave = addNode(region, kentucky, cwSurveyNode::Kind::Cave, QStringLiteral("Side Cave"));
        upperLevel = addNode(region, sideCave, cwSurveyNode::Kind::Folder, QStringLiteral("Upper level"));
        otherCave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, QStringLiteral("Other Cave"));
        trip1 = addTrip(sideCave, QStringLiteral("Trip 1"), {QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a3")});
        trip2 = addTrip(sideCave, QStringLiteral("Trip 2"), {QStringLiteral("a2"), QStringLiteral("b1")});
        trip3 = addTrip(upperLevel, QStringLiteral("Trip 3"), {QStringLiteral("a3"), QStringLiteral("s1")});
    }
};

}

TEST_CASE("Move to… refuses every place a subject can't go, and says why", "[SurveyNode]")
{
    Tree tree;
    cwSurveyNode* root = tree.region.rootNode();

    SECTION("a trip lands on a cave or a section, never where caves go") {
        CHECK(cwSurveyNode::isMovable(tree.trip1));
        CHECK(tree.upperLevel->moveRefusal(tree.trip1).isEmpty());
        CHECK(tree.otherCave->moveRefusal(tree.trip1).isEmpty());
        CHECK(root->moveRefusal(tree.trip1) == QStringLiteral("Trips go in a cave or a section"));
        CHECK(tree.kentucky->moveRefusal(tree.trip1) == QStringLiteral("Trips go in a cave or a section"));
        CHECK(tree.sideCave->moveRefusal(tree.trip1) == QStringLiteral("Already here"));
    }

    SECTION("a cave never lands inside a cave, and a node never inside itself") {
        CHECK(cwSurveyNode::isMovable(tree.sideCave));
        CHECK(root->moveRefusal(tree.sideCave).isEmpty());
        CHECK(tree.kentucky->moveRefusal(tree.sideCave) == QStringLiteral("Already here"));
        CHECK(tree.otherCave->moveRefusal(tree.sideCave) == QStringLiteral("A cave can't hold a cave"));
        CHECK(tree.sideCave->moveRefusal(tree.sideCave) == QStringLiteral("A node can't move into itself"));
        CHECK(tree.upperLevel->moveRefusal(tree.sideCave) == QStringLiteral("A node can't move into itself"));

        //A folder carries its caves along, so it is a cave as far as a cave's
        //inside is concerned.
        CHECK(tree.otherCave->moveRefusal(tree.kentucky) == QStringLiteral("A cave can't hold a cave"));
    }

    SECTION("a folder under a cave becomes a section; the root takes nodes") {
        CHECK(tree.otherCave->moveRefusal(tree.upperLevel).isEmpty());
        CHECK(root->moveRefusal(tree.upperLevel).isEmpty());
        CHECK(tree.sideCave->moveRefusal(tree.upperLevel) == QStringLiteral("Already here"));
    }

    SECTION("the root and attached survey data stay put") {
        CHECK_FALSE(cwSurveyNode::isMovable(root));
        CHECK(root->moveRefusal(root) == QStringLiteral("Survey data from an attached file stays where the file puts it"));

        tree.trip2->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));
        CHECK_FALSE(cwSurveyNode::isMovable(tree.trip2));
        CHECK(tree.upperLevel->moveRefusal(tree.trip2)
              == QStringLiteral("Survey data from an attached file stays where the file puts it"));

        tree.otherCave->setExternalCenterline(cwExternalCenterline(QStringLiteral("survex_simple.svx")));
        CHECK_FALSE(cwSurveyNode::isMovable(tree.otherCave));
        CHECK(tree.otherCave->moveRefusal(tree.trip1) == QStringLiteral("An attached survey file is read-only"));
    }

    SECTION("a sourced node and everything under it stays put") {
        tree.kentucky->setSourceId(QUuid::createUuid());
        CHECK_FALSE(cwSurveyNode::isMovable(tree.kentucky));
        CHECK_FALSE(cwSurveyNode::isMovable(tree.sideCave));
        CHECK_FALSE(cwSurveyNode::isMovable(tree.trip1));
        CHECK(tree.otherCave->moveRefusal(tree.trip1)
              == QStringLiteral("Survey data from an attached file stays where the file puts it"));
    }

    SECTION("a node of another project is no destination") {
        cwCavingRegion elsewhere;
        cwSurveyNode* foreignCave = addNode(elsewhere, nullptr, cwSurveyNode::Kind::Cave, QStringLiteral("Foreign"));
        CHECK(foreignCave->moveRefusal(tree.trip1) == QStringLiteral("Not in this project"));
        CHECK_FALSE(foreignCave->moveHere(tree.trip1));
        CHECK(tree.trip1->parentNode() == tree.sideCave);
    }
}

TEST_CASE("Moving a trip across a scope boundary ties the names it leaves behind", "[SurveyNode][Equate]")
{
    Tree tree;
    cwEquateModel* equates = tree.region.equates();

    //Trip 1 shares a2 with Trip 2, which stays in Side Cave, and a3 with
    //Trip 3, already in Upper level.
    CHECK(tree.upperLevel->moveConsequences(tree.trip1)
          == QStringLiteral("Move Trip 1 to Upper level? 1 station shared with Side Cave becomes a tie. "
                            "1 station joins the same-named station in Upper level."));

    const int undoCountBefore = tree.undoStack.count();
    REQUIRE(tree.upperLevel->moveHere(tree.trip1));

    CHECK(tree.undoStack.count() == undoCountBefore + 1);
    CHECK(tree.trip1->parentNode() == tree.upperLevel);
    CHECK(tree.sideCave->trips() == QList<cwTrip*>({tree.trip2}));
    CHECK(tree.upperLevel->trips() == QList<cwTrip*>({tree.trip3, tree.trip1}));
    REQUIRE(equates->count() == 1);
    CHECK(equates->equateAt(0) == tie(tree.sideCave, QStringLiteral("a2"), tree.upperLevel, QStringLiteral("a2")));

    tree.undoStack.undo();

    CHECK(tree.trip1->parentNode() == tree.sideCave);
    CHECK(tree.sideCave->trips() == QList<cwTrip*>({tree.trip1, tree.trip2}));
    CHECK(tree.upperLevel->trips() == QList<cwTrip*>({tree.trip3}));
    CHECK(equates->count() == 0);

    tree.undoStack.redo();

    CHECK(tree.trip1->parentNode() == tree.upperLevel);
    CHECK(equates->count() == 1);

    SECTION("a move the region already ties makes no second tie") {
        tree.undoStack.undo();
        equates->appendEquate(tie(tree.sideCave, QStringLiteral("a2"), tree.upperLevel, QStringLiteral("a2")));
        REQUIRE(tree.upperLevel->moveHere(tree.trip1));
        CHECK(equates->count() == 1);
    }

    SECTION("a move that shares no name asks nothing") {
        tree.undoStack.undo();
        cwTrip* loner = addTrip(tree.sideCave, QStringLiteral("Loner"), {QStringLiteral("z1"), QStringLiteral("z2")});
        CHECK(tree.otherCave->moveConsequences(loner).isEmpty());
    }
}

TEST_CASE("Moving a trip re-keys the equates that named a station leaving with it", "[SurveyNode][Equate]")
{
    Tree tree;
    cwEquateModel* equates = tree.region.equates();

    //a1 lives only in Trip 1, so it leaves Side Cave with it; a2 stays behind
    //in Trip 2, and the tie the move makes is what keeps it connected.
    const cwEquate toA1 = tie(tree.sideCave, QStringLiteral("a1"), tree.otherCave, QStringLiteral("x1"));
    const cwEquate toA2 = tie(tree.sideCave, QStringLiteral("a2"), tree.otherCave, QStringLiteral("x2"));
    equates->appendEquate(toA1);
    equates->appendEquate(toA2);

    REQUIRE(tree.upperLevel->moveHere(tree.trip1));

    REQUIRE(equates->count() == 3);
    CHECK(equates->equateAt(0) == tie(tree.upperLevel, QStringLiteral("a1"), tree.otherCave, QStringLiteral("x1")));
    CHECK(equates->equateAt(1) == toA2);

    tree.undoStack.undo();

    CHECK(equates->equates() == QList<cwEquate>({toA1, toA2}));
}

TEST_CASE("A fix whose station leaves with a trip moves to the destination's fix table",
          "[SurveyNode][cwFixStationModel]")
{
    Tree tree;
    tree.sideCave->fixStations()->appendFixStation(fixAt(QStringLiteral("a1")));
    tree.sideCave->fixStations()->appendFixStation(fixAt(QStringLiteral("b1")));
    tree.sideCave->fixStations()->appendFixStation(fixAt(QStringLiteral("a2")));
    const cwFixStation a1Fix = tree.sideCave->fixStations()->fixStationAt(0);

    CHECK(tree.otherCave->moveConsequences(tree.trip1)
          == QStringLiteral("Move Trip 1 to Other Cave? 1 station shared with Side Cave becomes a tie. "
                            "1 fix station moves with it."));

    REQUIRE(tree.otherCave->moveHere(tree.trip1));

    //a1 left with Trip 1; b1 and a2 still have Trip 2 in Side Cave.
    CHECK(fixNames(tree.sideCave) == QStringList({QStringLiteral("b1"), QStringLiteral("a2")}));
    CHECK(fixNames(tree.otherCave) == QStringList({QStringLiteral("a1")}));
    CHECK(tree.otherCave->fixStations()->fixStationAt(0).id() == a1Fix.id());

    tree.undoStack.undo();

    CHECK(fixNames(tree.sideCave) == QStringList({QStringLiteral("a1"), QStringLiteral("b1"), QStringLiteral("a2")}));
    CHECK(tree.otherCave->fixStations()->count() == 0);

    SECTION("a station the destination already fixes keeps that fix") {
        tree.otherCave->fixStations()->appendFixStation(fixAt(QStringLiteral("A1")));
        REQUIRE(tree.otherCave->moveHere(tree.trip1));
        CHECK(fixNames(tree.sideCave).contains(QStringLiteral("a1")));
        CHECK(fixNames(tree.otherCave) == QStringList({QStringLiteral("A1")}));
    }
}

TEST_CASE("Moving a node carries its qualified fixes and equates one level up", "[SurveyNode][Equate]")
{
    Tree tree;
    cwEquateModel* equates = tree.region.equates();

    const QString oldLabel = tree.sideCave->childScopeLabels().value(tree.upperLevel->id());
    REQUIRE_FALSE(oldLabel.isEmpty());

    tree.sideCave->fixStations()->appendFixStation(fixAt(oldLabel + QStringLiteral(".s1")));
    tree.sideCave->fixStations()->appendFixStation(fixAt(QStringLiteral("b1")));
    const cwEquate intoSection = tie(tree.sideCave, oldLabel + QStringLiteral(".s1"),
                                     tree.otherCave, QStringLiteral("x1"));
    equates->appendEquate(intoSection);

    REQUIRE(tree.otherCave->moveHere(tree.upperLevel));

    CHECK(tree.upperLevel->parentNode() == tree.otherCave);
    const QString newLabel = tree.otherCave->childScopeLabels().value(tree.upperLevel->id());
    CHECK(fixNames(tree.sideCave) == QStringList({QStringLiteral("b1")}));
    CHECK(fixNames(tree.otherCave) == QStringList({newLabel + QStringLiteral(".s1")}));
    REQUIRE(equates->count() == 1);
    CHECK(equates->equateAt(0) == tie(tree.otherCave, newLabel + QStringLiteral(".s1"),
                                      tree.otherCave, QStringLiteral("x1")));

    tree.undoStack.undo();

    CHECK(tree.upperLevel->parentNode() == tree.sideCave);
    CHECK(fixNames(tree.sideCave) == QStringList({oldLabel + QStringLiteral(".s1"), QStringLiteral("b1")}));
    CHECK(tree.otherCave->fixStations()->count() == 0);
    CHECK(equates->equates() == QList<cwEquate>({intoSection}));
}

TEST_CASE("A moved trip takes a free name and keeps living after the stack lets go", "[SurveyNode]")
{
    Tree tree;
    cwTrip* namesake = addTrip(tree.upperLevel, QStringLiteral("Trip 1"), {QStringLiteral("q1"), QStringLiteral("q2")});

    REQUIRE(tree.upperLevel->moveHere(tree.trip1));

    CHECK(namesake->name() == QStringLiteral("Trip 1"));
    CHECK(tree.trip1->name() != QStringLiteral("Trip 1"));
    CHECK(tree.upperLevel->tripNameSet().contains(tree.trip1->name()));

    tree.undoStack.undo();

    CHECK(tree.trip1->name() == QStringLiteral("Trip 1"));
    CHECK(tree.sideCave->tripNameSet().contains(QStringLiteral("Trip 1")));

    tree.undoStack.redo();

    //The remove half of the move owns the trip it took off Side Cave, and a
    //command the stack drops deletes what it owns, unless a node lists it.
    QPointer<cwTrip> moved = tree.trip1;
    tree.undoStack.clear();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    REQUIRE_FALSE(moved.isNull());
    CHECK(moved->parentNode() == tree.upperLevel);
}

TEST_CASE("Move to… works with no undo stack", "[SurveyNode]")
{
    cwCavingRegion region;
    cwSurveyNode* cave = addNode(region, nullptr, cwSurveyNode::Kind::Cave, QStringLiteral("Cave"));
    cwSurveyNode* section = addNode(region, cave, cwSurveyNode::Kind::Folder, QStringLiteral("Section"));
    cwTrip* first = addTrip(cave, QStringLiteral("First"), {QStringLiteral("a1"), QStringLiteral("a2")});
    addTrip(cave, QStringLiteral("Second"), {QStringLiteral("a2"), QStringLiteral("a3")});

    REQUIRE(section->moveHere(first));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    CHECK(first->parentNode() == section);
    CHECK(region.equates()->count() == 1);
}

TEST_CASE("Move to… carries a trip's directory, note image included, and its tie to disk", "[SurveyNode]")
{
    auto rootData = std::make_unique<cwRootData>();
    cwCavingRegion* region = rootData->project()->cavingRegion();

    cwCave* sideCave = SurveyTreeTestHelper::addNode(region, nullptr, cwSurveyNode::Kind::Cave,
                                                     QStringLiteral("Side Cave"));
    cwCave* upperLevel = SurveyTreeTestHelper::addNode(region, sideCave, cwSurveyNode::Kind::Folder,
                                                       QStringLiteral("Upper level"));
    cwTrip* moving = SurveyTreeTestHelper::addTrip(sideCave, QStringLiteral("Dome climb"), QStringLiteral("a"));
    cwTrip* staying = SurveyTreeTestHelper::addTrip(sideCave, QStringLiteral("Entrance"), QStringLiteral("a"));
    REQUIRE(staying != nullptr);

    moving->notes()->addFromFiles(
        {QUrl::fromLocalFile(testcasesDatasetPath(QStringLiteral("test_cwAddImageTask/supportedImage.png")))});
    rootData->futureManagerModel()->waitForFinished();
    REQUIRE(moving->notes()->rowCount() == 1);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectFile = SurveyTreeTestHelper::saveProjectAs(rootData.get(), QDir(tempDir.path()),
                                                                    QStringLiteral("move-trip"));

    const auto loadAgain = [&projectFile]() {
        auto loaded = std::make_unique<cwRootData>();
        addTokenManager(loaded->project());
        loaded->project()->loadOrConvert(projectFile);
        loaded->project()->waitLoadToFinish();
        return loaded;
    };

    //The note image lives in its trip's directory, so where the one image of
    //the project sits says where the directory went.
    const QDir projectRootDir = QFileInfo(projectFile).absoluteDir();
    const auto imagePaths = [&projectRootDir]() {
        QStringList images;
        const QStringList files = SurveyTreeTestHelper::relativeFiles(projectRootDir);
        for(const QString& file : files) {
            if(file.endsWith(QStringLiteral(".png"))) {
                images.append(file);
            }
        }
        return images;
    };
    REQUIRE(imagePaths().size() == 1);
    CHECK_FALSE(imagePaths().first().contains(QStringLiteral("sub/")));

    //Both trips share a1 and a2, which become ties once Dome climb is in its
    //own block.
    REQUIRE(upperLevel->moveHere(moving));
    SurveyTreeTestHelper::flushSaves(rootData.get());

    REQUIRE(imagePaths().size() == 1);
    CHECK(imagePaths().first().contains(QStringLiteral("sub/")));

    {
        auto loaded = loadAgain();
        cwSurveyNode* loadedCave = loaded->project()->cavingRegion()->rootNode()->childNode(0);
        REQUIRE(loadedCave != nullptr);
        REQUIRE(loadedCave->childNodeCount() == 1);
        cwSurveyNode* loadedSection = loadedCave->childNode(0);
        CHECK(loadedCave->tripCount() == 1);
        REQUIRE(loadedSection->tripCount() == 1);
        const cwTrip* loadedTrip = loadedSection->trip(0);
        CHECK(loadedTrip->name() == QStringLiteral("Dome climb"));
        CHECK(loadedTrip->id() == moving->id());
        CHECK(loadedTrip->notes()->rowCount() == 1);
        CHECK(loaded->project()->cavingRegion()->equates()->count() == 2);
    }

    rootData->undoStack()->undo();
    SurveyTreeTestHelper::flushSaves(rootData.get());

    REQUIRE(imagePaths().size() == 1);
    CHECK_FALSE(imagePaths().first().contains(QStringLiteral("sub/")));

    {
        auto loaded = loadAgain();
        cwSurveyNode* loadedCave = loaded->project()->cavingRegion()->rootNode()->childNode(0);
        REQUIRE(loadedCave != nullptr);
        CHECK(loadedCave->tripCount() == 2);
        CHECK(loadedCave->childNode(0)->tripCount() == 0);
        CHECK(loaded->project()->cavingRegion()->equates()->count() == 0);
    }
}
