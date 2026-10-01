/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The scope labels the exporter, the line-plot worker and the geometry pass all
// read. Every one of them used to assign its own; these cases pin the answers
// they now share, and the two properties the sharing rests on — that the live
// caches and the snapshot pool assign identically, and that a trip label needs
// no region to be right.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCaveData.h"
#include "cwCavingRegion.h"
#include "cwCavingRegionData.h"
#include "cwScopeLabels.h"
#include "cwSignalSpy.h"
#include "cwTrip.h"
#include "cwTripData.h"

// Qt
#include <QUuid>

namespace {

cwTripData tripData(const QString& name)
{
    cwTripData trip;
    trip.id = QUuid::createUuid();
    trip.name = name;
    return trip;
}

cwCaveData caveData(const QString& name, const QStringList& tripNames = {})
{
    cwCaveData cave;
    cave.id = QUuid::createUuid();
    cave.name = name;
    for (const QString& tripName : tripNames) {
        cave.trips.append(tripData(tripName));
    }
    return cave;
}

cwCavingRegionData regionData(const QList<cwCaveData>& caves)
{
    cwCavingRegionData region;
    region.caves = caves;
    return region;
}

} // namespace

TEST_CASE("cwScopeLabels names every cave and trip in a region", "[cwScopeLabels][scope]")
{
    const cwCavingRegionData region = regionData({
        caveData(QStringLiteral("Fisher Ridge"), {QStringLiteral("Topo 1"),
                                                  QStringLiteral("Topo-1")}),
        caveData(QStringLiteral("Fisher-Ridge"), {QStringLiteral("Topo 1")}),
    });
    const cwCaveData& first = region.caves.at(0);
    const cwCaveData& second = region.caves.at(1);

    const cwScopeLabels labels(region);

    SECTION("cave labels are unique across the region") {
        CHECK(labels.label(first.id) == QStringLiteral("fisher_ridge"));
        CHECK(labels.label(second.id) == QStringLiteral("fisher_ridge_2"));
    }

    SECTION("prefix is the label the cave's stations hang under") {
        CHECK(labels.prefix(first.id) == QStringLiteral("fisher_ridge."));
    }

    SECTION("trip labels are unique only within their own cave") {
        // The collision suffix on "Topo-1" is the point: it is second in *this*
        // cave. The other cave's "Topo 1" collides with nothing, so it keeps the
        // bare label even though the same string is already taken next door.
        CHECK(labels.tripLabels(first.id).value(first.trips.at(0).id)
              == QStringLiteral("topo_1"));
        CHECK(labels.tripLabels(first.id).value(first.trips.at(1).id)
              == QStringLiteral("topo_1_2"));
        CHECK(labels.tripLabels(second.id).value(second.trips.at(0).id)
              == QStringLiteral("topo_1"));
    }

    SECTION("nodeId inverts label, which is how a solved name finds its cave") {
        CHECK(labels.nodeId({QStringLiteral("fisher_ridge")}) == first.id);
        CHECK(labels.nodeId({QStringLiteral("fisher_ridge_2")}) == second.id);
    }

    SECTION("a scope this pool never saw is answered as absent, not guessed at") {
        const QUuid stranger = QUuid::createUuid();
        CHECK(labels.label(stranger).isEmpty());
        CHECK(labels.prefix(stranger).isEmpty());
        CHECK(labels.tripLabels(stranger).isEmpty());
        CHECK(labels.nodeId({QStringLiteral("bat_cave")}).isNull());
    }

    SECTION("a default-constructed pool names nothing") {
        const cwScopeLabels empty;
        CHECK(empty.label(first.id).isEmpty());
        CHECK(empty.tripLabels(first.id).isEmpty());
    }
}

TEST_CASE("The live caches and the snapshot pool assign the same labels",
          "[cwScopeLabels][scope]")
{
    // The invariant the whole design rests on, and the only case that crosses
    // the two implementations of it. cwSiblingLabelCache assigns from the live
    // QObject children on the main thread; cwScopeLabels assigns from a
    // cwCavingRegionData on the worker. Nothing is carried between them — the
    // export/decode round trip is correct only because both evaluate the same
    // function over the same order. Neither side can drift alone without this
    // failing.
    cwCavingRegion region;

    cwCave* first = new cwCave();
    first->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(first);

    cwCave* second = new cwCave();
    second->setName(QStringLiteral("Fisher-Ridge"));
    region.addCave(second);

    const auto addTrip = [](cwCave* cave, const QString& name) {
        cwTrip* trip = new cwTrip();
        trip->setName(name);
        cave->addTrip(trip);
    };
    addTrip(first, QStringLiteral("Topo 1"));
    addTrip(first, QStringLiteral("Topo-1"));
    addTrip(second, QStringLiteral("Topo 1"));

    const cwScopeLabels snapshot(region.data());

    REQUIRE(region.caveCount() == 2);
    for (cwCave* cave : region.caves()) {
        INFO("cave: " << cave->name().toStdString());
        CHECK(region.caveScopeLabels().value(cave->id()) == snapshot.label(cave->id()));
        CHECK(cave->tripScopeLabels() == snapshot.tripLabels(cave->id()));
        CHECK_FALSE(cave->tripScopeLabels().isEmpty());
    }
}

TEST_CASE("cwScopeLabels::forNode names a cave standing alone", "[cwScopeLabels][scope]")
{
    // The single-cave exporter has no region and so no siblings: the cave takes
    // its own sanitized name, which cannot collide with anything. Its trips and
    // child nodes are labeled exactly as a region would label them.
    cwCaveData cave = caveData(QStringLiteral("Fisher Ridge"),
                               {QStringLiteral("Topo 1"), QStringLiteral("Topo-1")});
    cave.nodes.append(caveData(QStringLiteral("Upper Level"), {QStringLiteral("Dome")}));

    const cwScopeLabels labels = cwScopeLabels::forNode(cave);

    CHECK(labels.label(cave.id) == QStringLiteral("fisher_ridge"));
    CHECK(labels.prefix(cave.nodes.at(0).id) == QStringLiteral("fisher_ridge.upper_level."));
    CHECK(labels.tripLabels(cave.id).value(cave.trips.at(0).id) == QStringLiteral("topo_1"));
    CHECK(labels.tripLabels(cave.id).value(cave.trips.at(1).id) == QStringLiteral("topo_1_2"));

    SECTION("and gives its subtree the same labels a region would have") {
        const cwScopeLabels regionLabels(regionData({cave}));
        CHECK(labels.tripLabels(cave.id) == regionLabels.tripLabels(cave.id));
        const QUuid sectionId = cave.nodes.at(0).id;
        CHECK(labels.prefix(sectionId) == regionLabels.prefix(sectionId));
        CHECK(labels.tripLabels(sectionId) == regionLabels.tripLabels(sectionId));
    }
}

namespace {

//! Folder > Cave > Section, with a sibling at every level whose name sanitizes
//! to the same label as its neighbor's, and one name reused under two parents.
//!
//!   Kentucky field seasons        Kentucky-field-seasons
//!     Side Cave                     Side Cave           (reused, different parent)
//!       Upper level  Upper-level
//!         trips: Dome climb, Dome-climb
struct DepthThreeTree {
    cwCavingRegionData region;
    QUuid folder;
    QUuid folderTwin;
    QUuid sideCave;
    QUuid twinSideCave;
    QUuid section;
    QUuid sectionTwin;
    QUuid domeClimb;
    QUuid domeClimbTwin;
};

DepthThreeTree depthThreeTree()
{
    cwCaveData section = caveData(QStringLiteral("Upper level"),
                                  {QStringLiteral("Dome climb"), QStringLiteral("Dome-climb")});
    cwCaveData sectionTwin = caveData(QStringLiteral("Upper-level"));

    cwCaveData sideCave = caveData(QStringLiteral("Side Cave"), {QStringLiteral("Sump dig")});
    sideCave.nodes = {section, sectionTwin};

    cwCaveData folder = caveData(QStringLiteral("Kentucky field seasons"));
    folder.nodes = {sideCave};

    cwCaveData twinSideCave = caveData(QStringLiteral("Side Cave"));
    cwCaveData folderTwin = caveData(QStringLiteral("Kentucky-field-seasons"));
    folderTwin.nodes = {twinSideCave};

    DepthThreeTree tree;
    tree.region = regionData({folder, folderTwin});
    tree.folder = folder.id;
    tree.folderTwin = folderTwin.id;
    tree.sideCave = sideCave.id;
    tree.twinSideCave = twinSideCave.id;
    tree.section = section.id;
    tree.sectionTwin = sectionTwin.id;
    tree.domeClimb = section.trips.at(0).id;
    tree.domeClimbTwin = section.trips.at(1).id;
    return tree;
}

} // namespace

TEST_CASE("cwScopeLabels names every node at every depth", "[cwScopeLabels][scope]")
{
    const DepthThreeTree tree = depthThreeTree();
    const cwScopeLabels labels(tree.region);

    SECTION("a node's prefix joins every label from the top down") {
        CHECK(labels.prefix(tree.folder) == QStringLiteral("kentucky_field_seasons."));
        CHECK(labels.prefix(tree.sideCave) == QStringLiteral("kentucky_field_seasons.side_cave."));
        CHECK(labels.prefix(tree.section)
              == QStringLiteral("kentucky_field_seasons.side_cave.upper_level."));
    }

    SECTION("colliding sanitized names are broken within each sibling set") {
        CHECK(labels.label(tree.folderTwin) == QStringLiteral("kentucky_field_seasons_2"));
        CHECK(labels.label(tree.sectionTwin) == QStringLiteral("upper_level_2"));
        CHECK(labels.prefix(tree.sectionTwin)
              == QStringLiteral("kentucky_field_seasons.side_cave.upper_level_2."));
        CHECK(labels.tripLabels(tree.section).value(tree.domeClimb) == QStringLiteral("dome_climb"));
        CHECK(labels.tripLabels(tree.section).value(tree.domeClimbTwin)
              == QStringLiteral("dome_climb_2"));
    }

    SECTION("a name reused under another parent keeps its bare label") {
        CHECK(labels.label(tree.twinSideCave) == QStringLiteral("side_cave"));
        CHECK(labels.prefix(tree.twinSideCave)
              == QStringLiteral("kentucky_field_seasons_2.side_cave."));
    }

    SECTION("nodeId finds a node by its whole label path") {
        CHECK(labels.nodeId({QStringLiteral("kentucky_field_seasons"),
                             QStringLiteral("side_cave"),
                             QStringLiteral("upper_level_2")}) == tree.sectionTwin);
        CHECK(labels.nodeId({QStringLiteral("side_cave")}).isNull());
        CHECK(labels.nodeId({}).isNull());
    }
}

TEST_CASE("cwScopeLabels::resolve inverts prefix", "[cwScopeLabels][scope]")
{
    const DepthThreeTree tree = depthThreeTree();
    const cwScopeLabels labels(tree.region);

    SECTION("every node round-trips a station tail") {
        for (const QUuid& id : {tree.folder, tree.folderTwin, tree.sideCave, tree.twinSideCave,
                                tree.section, tree.sectionTwin}) {
            const cwScopeLabels::Resolution resolution = labels.resolve(labels.prefix(id)
                                                                        + QStringLiteral("a1"));
            CHECK(resolution.nodeId == id);
            CHECK(resolution.remainder == QStringLiteral("a1"));
        }
    }

    SECTION("a trip scope and a dotted tail stay in the remainder") {
        const cwScopeLabels::Resolution resolution = labels.resolve(
            labels.prefix(tree.section) + QStringLiteral("dome_climb.simple.a1"));
        CHECK(resolution.nodeId == tree.section);
        CHECK(resolution.remainder == QStringLiteral("dome_climb.simple.a1"));
    }

    SECTION("segments match case-insensitively, as cavern may echo a nested scope") {
        const cwScopeLabels::Resolution resolution =
            labels.resolve(QStringLiteral("Kentucky_Field_Seasons.SIDE_CAVE.x"));
        CHECK(resolution.nodeId == tree.sideCave);
        CHECK(resolution.remainder == QStringLiteral("x"));
    }

    SECTION("the walk stops at the first segment no child wears") {
        const cwScopeLabels::Resolution partial =
            labels.resolve(QStringLiteral("kentucky_field_seasons.lost_passage.x"));
        CHECK(partial.nodeId == tree.folder);
        CHECK(partial.remainder == QStringLiteral("lost_passage.x"));

        const cwScopeLabels::Resolution unknown = labels.resolve(QStringLiteral("bat_cave.x"));
        CHECK(unknown.nodeId.isNull());
        CHECK(unknown.remainder == QStringLiteral("bat_cave.x"));
    }
}

TEST_CASE("The live caches and the snapshot pool agree at every depth",
          "[cwScopeLabels][scope]")
{
    cwCavingRegion region;

    const auto addChild = [](cwSurveyNode* parent, const QString& name) {
        cwCave* node = new cwCave();
        node->setName(name);
        parent->addNode(node);
        return node;
    };

    cwCave* folder = addChild(region.rootNode(), QStringLiteral("Kentucky field seasons"));
    addChild(region.rootNode(), QStringLiteral("Kentucky-field-seasons"));
    cwCave* sideCave = addChild(folder, QStringLiteral("Side Cave"));
    addChild(sideCave, QStringLiteral("Upper level"));
    cwCave* sectionTwin = addChild(sideCave, QStringLiteral("Upper-level"));
    for (const QString& name : {QStringLiteral("Dome climb"), QStringLiteral("Dome-climb")}) {
        cwTrip* trip = new cwTrip();
        trip->setName(name);
        sectionTwin->addTrip(trip);
    }

    const cwScopeLabels snapshot(region.data());

    const QList<cwSurveyNode*> nodes = region.rootNode()->allNodes();
    REQUIRE(nodes.size() == 5);
    for (const cwSurveyNode* node : nodes) {
        INFO("node: " << node->name().toStdString());
        const cwSurveyNode* parent = node->parentNode();
        REQUIRE(parent != nullptr);
        CHECK(parent->childScopeLabels().value(node->id()) == snapshot.label(node->id()));
        CHECK(node->tripScopeLabels() == snapshot.tripLabels(node->id()));
    }
    CHECK(snapshot.label(sectionTwin->id()) == QStringLiteral("upper_level_2"));
}

TEST_CASE("A child node takes a label apart from every trip beside it",
          "[cwScopeLabels][scope]")
{
    // A node's trips and its child nodes each open a "*begin <label>" block in
    // the same scope, so one label on both would merge their stations in cavern
    // and hand the trip's stations to the child on decode. Trips claim their
    // labels first, so a trip's label depends only on its sibling trips.
    cwCaveData sideCave = caveData(QStringLiteral("Side Cave"), {QStringLiteral("Dome climb")});
    sideCave.nodes.append(caveData(QStringLiteral("Dome-climb")));
    const QUuid tripId = sideCave.trips.at(0).id;
    const QUuid childId = sideCave.nodes.at(0).id;

    const cwScopeLabels labels(regionData({sideCave}));

    CHECK(labels.tripLabels(sideCave.id).value(tripId) == QStringLiteral("dome_climb"));
    CHECK(labels.label(childId) == QStringLiteral("dome_climb_2"));
    CHECK(labels.prefix(childId) == QStringLiteral("side_cave.dome_climb_2."));

    SECTION("so a trip's station decodes into the trip's own node") {
        const cwScopeLabels::Resolution resolution =
            labels.resolve(QStringLiteral("side_cave.dome_climb.a1"));
        CHECK(resolution.nodeId == sideCave.id);
        CHECK(resolution.remainder == QStringLiteral("dome_climb.a1"));
    }

    SECTION("and the child keeps its station tail") {
        const cwScopeLabels::Resolution resolution =
            labels.resolve(QStringLiteral("side_cave.dome_climb_2.a1"));
        CHECK(resolution.nodeId == childId);
        CHECK(resolution.remainder == QStringLiteral("a1"));
    }
}

TEST_CASE("The live caches move a child's label when a trip beside it takes it",
          "[cwScopeLabels][scope]")
{
    cwCavingRegion region;

    cwCave* sideCave = new cwCave();
    sideCave->setName(QStringLiteral("Side Cave"));
    region.rootNode()->addNode(sideCave);

    cwCave* child = new cwCave();
    child->setName(QStringLiteral("Dome climb"));
    sideCave->addNode(child);

    REQUIRE(sideCave->childScopeLabels().value(child->id()) == QStringLiteral("dome_climb"));

    cwSignalSpy childLabelsSpy(sideCave, &cwSurveyNode::childScopeLabelsChanged);

    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Dome climb"));
    sideCave->addTrip(trip);

    CHECK(childLabelsSpy.count() == 1);
    CHECK(sideCave->tripScopeLabels().value(trip->id()) == QStringLiteral("dome_climb"));
    CHECK(sideCave->childScopeLabels().value(child->id()) == QStringLiteral("dome_climb_2"));
    CHECK_FALSE(sideCave->childScopeLabels().contains(trip->id()));

    const cwScopeLabels snapshot(region.data());
    CHECK(sideCave->childScopeLabels().value(child->id()) == snapshot.label(child->id()));
    CHECK(sideCave->tripScopeLabels() == snapshot.tripLabels(sideCave->id()));

    SECTION("and give it back when the trip is renamed away") {
        trip->setName(QStringLiteral("Sump dig"));

        CHECK(childLabelsSpy.count() == 2);
        CHECK(sideCave->childScopeLabels().value(child->id()) == QStringLiteral("dome_climb"));
    }

    SECTION("and keep a node with no children quiet when its trips change") {
        cwSignalSpy leafChildLabelsSpy(child, &cwSurveyNode::childScopeLabelsChanged);

        cwTrip* leafTrip = new cwTrip();
        leafTrip->setName(QStringLiteral("Sump dig"));
        child->addTrip(leafTrip);

        CHECK(leafChildLabelsSpy.count() == 0);
    }
}
