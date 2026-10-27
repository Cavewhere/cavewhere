//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

//Our includes
#include "cwTrip.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwKeywordModel.h"
#include "cwKeyword.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwScopeLabels.h"

//Test includes
#include "CasePairTestHelpers.h"
#include "SurveyTreeTestHelper.h"
#include "SurveyEditorTestHelper.h"

//Qt inculdes
#include "cwSignalSpy.h"
#include <QSignalSpy>
#include <QUndoStack>
#include <QVector3D>

namespace {

//! Makes \a node a source root: sourced, with no path of its own.
void makeSourceRoot(cwSurveyNode* node)
{
    node->setSourceId(QUuid::createUuid());
    node->setSourcePath(QString());
}

//! A derived node under \a root named \a name, at \a sourcePath in the scan,
//! whose cavern level is \a cavernLabel.
cwCave* addDerivedNode(cwSurveyNode* root, const QString& name, const QString& sourcePath,
                       const QString& cavernLabel)
{
    auto* node = new cwCave();
    node->setName(name);
    node->setSourceId(root->sourceId());
    node->setSourcePath(sourcePath);
    node->setCavernLabel(cavernLabel);
    root->addNode(node);
    return node;
}

//! A derived trip under \a node, at \a sourcePath, holding \a stations.
cwTrip* addDerivedTrip(cwSurveyNode* node, const QString& name, const QString& sourcePath,
                       const QStringList& stations)
{
    auto* trip = new cwTrip();
    trip->setName(name);
    trip->setSourcePath(sourcePath);
    trip->setExternalStations(stations);
    node->addTrip(trip);
    return trip;
}

QMap<QString, QVector3D> byName(const QList<QPair<QString, QVector3D>>& stations)
{
    QMap<QString, QVector3D> named;
    for (const auto& station : stations) {
        named.insert(station.first, station.second);
    }
    return named;
}

} // namespace

TEST_CASE("cwTrip should strip the datestamp away from date", "[cwTrip]") {
    cwTrip trip;

    cwSignalSpy spy(&trip, &cwTrip::dateChanged);

    CHECK(trip.date().time().hour() == 0);
    CHECK(trip.date().time().minute() == 0);
    CHECK(trip.date().time().second() == 0);
    CHECK(trip.date().time().msec() == 0);
    CHECK(trip.date().date() == QDate::currentDate());

    trip.setDate(QDateTime::currentDateTime().addDays(-1));
    CHECK(trip.date().time().hour() == 0);
    CHECK(trip.date().time().minute() == 0);
    CHECK(trip.date().time().second() == 0);
    CHECK(trip.date().time().msec() == 0);
    CHECK(trip.date().date() == QDate::currentDate().addDays(-1));

    CHECK(spy.count() == 1);
}

TEST_CASE("cwTrip::scopePrefix unifies the three trip kinds", "[cwTrip][scope]")
{
    cwTrip trip;

    SECTION("a flat native trip is unscoped") {
        CHECK(trip.scopePrefix().isEmpty());
        CHECK_FALSE(trip.isScoped());
    }

    SECTION("an externally-attached trip scopes to its own survey label") {
        trip.setName(QStringLiteral("Topo 1"));
        trip.setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/cave.svx")));
        CHECK(trip.scopePrefix() == QStringLiteral("topo_1."));
        CHECK(trip.isScoped());
    }

    SECTION("a derived trip is unscoped: its node's view names its stations") {
        trip.setSourcePath(QStringLiteral("ALPHA.DAT/ALPHA1"));
        CHECK(trip.scopePrefix().isEmpty());
        CHECK_FALSE(trip.isScoped());
    }
}

TEST_CASE("cwTrip::scopePrefix labels a native trip inside a sourced subtree",
          "[cwTrip][scope]")
{
    // The driver writes such a trip as its own block in the source root's,
    // under a label unique across the subtree's native trips and apart from
    // the scopes the root's file opens (§7.2 step 5).
    cwCavingRegion region;
    cwCave* root = new cwCave();
    root->setName(QStringLiteral("Project"));
    region.addCave(root);
    makeSourceRoot(root);
    root->setExternalStations({QStringLiteral("alpha.a1"), QStringLiteral("r1")});
    cwCave* alpha = addDerivedNode(root, QStringLiteral("Alpha"), QStringLiteral("ALPHA.DAT"),
                                   QStringLiteral("alpha"));
    cwCave* bravo = addDerivedNode(root, QStringLiteral("Bravo"), QStringLiteral("BRAVO.DAT"),
                                   QStringLiteral("bravo"));

    cwTrip* underAlpha = new cwTrip();
    underAlpha->setName(QStringLiteral("Dig"));
    alpha->addTrip(underAlpha);
    cwTrip* underBravo = new cwTrip();
    underBravo->setName(QStringLiteral("Dig"));
    bravo->addTrip(underBravo);
    cwTrip* namedLikeAFile = new cwTrip();
    namedLikeAFile->setName(QStringLiteral("Alpha"));
    bravo->addTrip(namedLikeAFile);

    CHECK(underAlpha->isScoped());
    CHECK(underAlpha->scopePrefix() == QStringLiteral("dig."));
    CHECK(underBravo->scopePrefix() == QStringLiteral("dig_2."));
    CHECK(namedLikeAFile->scopePrefix() == QStringLiteral("alpha_2."));

    SECTION("the snapshot assigns the same labels") {
        const cwCavingRegionData data = region.data();
        const cwScopeLabels labels(data);
        const cwCaveData& rootData = data.caves.first();
        CHECK(labels.sourceRoot(rootData.nodes.at(1).id) == root->id());
        CHECK(labels.cavernPath(rootData.nodes.at(1).id) == QStringLiteral("bravo."));
        CHECK(cwTrip::scopePrefix(rootData.nodes.at(1).trips.at(0), labels, bravo->id())
              == QStringLiteral("dig_2."));
        CHECK(cwTrip::scopePrefix(rootData.nodes.at(1).trips.at(1), labels, bravo->id())
              == QStringLiteral("alpha_2."));
    }
}

namespace {

cwTrip* addExternalTrip(cwCave* cave, const QString& name)
{
    cwTrip* trip = new cwTrip();
    trip->setName(name);
    cave->addTrip(trip);
    trip->setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/cave.svx")));
    return trip;
}

} // namespace

TEST_CASE("Two like-named external trips in one cave get different scopes",
          "[cwTrip][scope]")
{
    // A trip's label is only unique among its cave's trips, so the accessor has
    // to look at its siblings. Two trips sharing a scope would put both files'
    // stations in one survey and tie every same-named station together.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* first = addExternalTrip(cave, QStringLiteral("Topo 1"));
    cwTrip* second = addExternalTrip(cave, QStringLiteral("Topo-1"));

    CHECK(first->scopePrefix() == QStringLiteral("topo_1."));
    CHECK(second->scopePrefix() == QStringLiteral("topo_1_2."));
}

TEST_CASE("A sibling's rename moves this trip's scope and fires scopeChanged",
          "[cwTrip][scope]")
{
    // The gap the cave-owned labels close: a trip cannot see its own collision
    // suffix move, because what moved it is a name the trip does not hold.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* first = addExternalTrip(cave, QStringLiteral("Topo 1"));
    cwTrip* second = addExternalTrip(cave, QStringLiteral("Topo-1"));
    REQUIRE(second->scopePrefix() == QStringLiteral("topo_1_2."));

    cwSignalSpy secondScopeSpy(second, &cwTrip::scopeChanged);
    cwSignalSpy regionSpy(&region, &cwCavingRegion::scopeLabelsChanged);

    // Renaming the *first* trip clears the collision, so the second drops its
    // suffix — a move nothing on the second trip caused.
    first->setName(QStringLiteral("Alpha"));

    CHECK(second->scopePrefix() == QStringLiteral("topo_1."));
    CHECK(secondScopeSpy.count() == 1);
    CHECK(regionSpy.count() == 1);
}

TEST_CASE("Inserting and removing a sibling moves an existing trip's scope",
          "[cwTrip][scope]")
{
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* existing = addExternalTrip(cave, QStringLiteral("Topo 1"));
    REQUIRE(existing->scopePrefix() == QStringLiteral("topo_1."));

    cwSignalSpy scopeSpy(existing, &cwTrip::scopeChanged);

    // Labels are assigned in list order, so a like-named trip inserted *ahead*
    // of this one takes the bare label and pushes this one onto the suffix.
    cwTrip* inserted = new cwTrip();
    inserted->setName(QStringLiteral("Topo-1"));
    cave->insertTrip(0, inserted);

    CHECK(existing->scopePrefix() == QStringLiteral("topo_1_2."));
    CHECK(scopeSpy.count() == 1);

    cave->removeTrip(0);

    CHECK(existing->scopePrefix() == QStringLiteral("topo_1."));
    CHECK(scopeSpy.count() == 2);
}

TEST_CASE("A trip removed from its cave reports no scope", "[cwTrip][scope]")
{
    // cwCave deliberately leaves the parent set on remove, so the trip still
    // answers parentCave() while the cave no longer lists it. Deriving a label
    // from the trip alone would hand it "topo_1" — the label the *live* sibling
    // holds — and the solved-* accessors would strip by it and return that
    // sibling's stations as this trip's.
    //
    // The undo stack is the configuration this matters in: it owns the removed
    // trip and keeps it alive. Without one, cwUndoer::pushUndo runs the command
    // and deletes it, which deleteLater()s the trip out from under the
    // assertions below. It only reaches the children that exist when it is set,
    // so it goes on after the cave is built.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* staying = addExternalTrip(cave, QStringLiteral("Topo 1"));
    cwTrip* removed = addExternalTrip(cave, QStringLiteral("Topo-1"));
    REQUIRE(removed->scopePrefix() == QStringLiteral("topo_1_2."));

    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    cave->removeTrip(cave->indexOf(removed));

    REQUIRE(removed->parentCave() == cave);
    // Still scoped by its own fields. The mismatch is deliberate: carrying a
    // scope is a property of the trip, having a *place* in the cave's namespace
    // is not, and only the cave can grant the second.
    CHECK(removed->isScoped());
    CHECK(removed->scopePrefix().isEmpty());
    // The label it would take alone, and so the one it must not answer with.
    CHECK(staying->scopePrefix() == QStringLiteral("topo_1."));
}

TEST_CASE("A trip the cave no longer lists stops dirtying its labels",
          "[cwTrip][scope]")
{
    // The other half of the insert/remove funnel. Without cwCave::disconnectTrip
    // a removed trip's rename would keep throwing away the cave's cache and
    // pulsing every trip still in it — and once it is re-parented, one rename
    // would pulse two caves.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* staying = addExternalTrip(cave, QStringLiteral("Topo 1"));
    cwTrip* removed = addExternalTrip(cave, QStringLiteral("Topo-1"));
    REQUIRE(staying->scopePrefix() == QStringLiteral("topo_1."));

    // The stack keeps the removed trip alive to be renamed below.
    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    cave->removeTrip(cave->indexOf(removed));

    cwSignalSpy caveSpy(cave, &cwCave::tripScopeLabelsChanged);
    cwSignalSpy stayingSpy(staying, &cwTrip::scopeChanged);
    cwSignalSpy regionSpy(&region, &cwCavingRegion::scopeLabelsChanged);

    removed->setName(QStringLiteral("Beta"));

    CHECK(caveSpy.count() == 0);
    CHECK(stayingSpy.count() == 0);
    CHECK(regionSpy.count() == 0);
    CHECK(staying->scopePrefix() == QStringLiteral("topo_1."));
}

TEST_CASE("A cave the region no longer lists stops pulsing it", "[cwCavingRegion][scope]")
{
    cwCavingRegion region;

    cwCave* staying = new cwCave();
    staying->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(staying);

    cwCave* removed = new cwCave();
    removed->setName(QStringLiteral("Fisher-Ridge"));
    region.addCave(removed);
    REQUIRE(region.caveScopeLabels().value(removed->id()) == QStringLiteral("fisher_ridge_2"));

    // The stack keeps the removed cave alive to be renamed below.
    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    region.removeCave(region.indexOf(removed));
    REQUIRE_FALSE(region.caveScopeLabels().contains(removed->id()));

    cwSignalSpy regionSpy(&region, &cwCavingRegion::scopeLabelsChanged);

    removed->setName(QStringLiteral("Beta"));
    CHECK(regionSpy.count() == 0);

    // A trip moving inside the removed cave must not reach the region either.
    addExternalTrip(removed, QStringLiteral("Topo 1"));
    CHECK(regionSpy.count() == 0);
}

TEST_CASE("Undo and redo of a trip insert leave exactly one connection",
          "[cwTrip][scope]")
{
    // Undo and redo drive removeTrips and insertTrips on the *same* trip, so the
    // wiring has to survive a round trip through both. If disconnectTrip missed
    // one, or connectTrip doubled one, a later sibling rename would fire
    // scopeChanged the wrong number of times — and compound on every cycle.
    // (Qt::UniqueConnection is a second guard on the same property; the
    // connect/disconnect symmetry alone is enough to keep this passing.)
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* first = addExternalTrip(cave, QStringLiteral("Topo 1"));

    // The stack only reaches the children that exist when it is set, so it goes
    // on here — after the first trip, before the one being undone.
    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    // Adding a trip pushes more than one command, so anchor on the stack index
    // rather than counting undo() calls.
    cwTrip* second = addExternalTrip(cave, QStringLiteral("Topo-1"));
    const int afterSecond = undoStack.index();
    REQUIRE(afterSecond > 0);
    REQUIRE(second->scopePrefix() == QStringLiteral("topo_1_2."));

    undoStack.setIndex(0);
    REQUIRE(cave->tripCount() == 1);
    undoStack.setIndex(afterSecond);
    REQUIRE(cave->tripCount() == 2);
    REQUIRE(second->scopePrefix() == QStringLiteral("topo_1_2."));

    cwSignalSpy secondScopeSpy(second, &cwTrip::scopeChanged);
    cwSignalSpy regionSpy(&region, &cwCavingRegion::scopeLabelsChanged);

    first->setName(QStringLiteral("Alpha"));

    CHECK(second->scopePrefix() == QStringLiteral("topo_1."));
    CHECK(secondScopeSpy.count() == 1);
    CHECK(regionSpy.count() == 1);
}

TEST_CASE("cwCavingRegion::scopeLabelsChanged covers cave and trip label moves",
          "[cwCavingRegion][scope]")
{
    cwCavingRegion region;
    cwCave* first = new cwCave();
    first->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(first);

    cwSignalSpy regionSpy(&region, &cwCavingRegion::scopeLabelsChanged);

    // A second cave whose name sanitizes alike takes the collision suffix, and
    // the region is the only object that can see that happen.
    cwCave* second = new cwCave();
    second->setName(QStringLiteral("Fisher-Ridge"));
    region.addCave(second);

    CHECK(regionSpy.count() == 1);
    CHECK(region.caveScopeLabels().value(first->id()) == QStringLiteral("fisher_ridge"));
    CHECK(region.caveScopeLabels().value(second->id()) == QStringLiteral("fisher_ridge_2"));

    first->setName(QStringLiteral("Alpha"));

    CHECK(regionSpy.count() == 2);
    CHECK(region.caveScopeLabels().value(second->id()) == QStringLiteral("fisher_ridge"));

    // A trip label moves inside its cave: the cave labels are untouched, but a
    // qualified station name still moved, so the region pulses too.
    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Topo 1"));
    second->addTrip(trip);

    CHECK(regionSpy.count() == 3);
    CHECK(second->tripScopeLabels().value(trip->id()) == QStringLiteral("topo_1"));
    // The pulse is region-wide, but the cave labels themselves did not move.
    CHECK(region.caveScopeLabels().value(second->id()) == QStringLiteral("fisher_ridge"));
}

TEST_CASE("cwTrip scope changes emit scopeChanged", "[cwTrip][scope]")
{
    cwTrip trip;
    cwSignalSpy scopeSpy(&trip, &cwTrip::scopeChanged);

    // A source path turns a native trip derived, which moves its scope.
    trip.setSourcePath(QStringLiteral("ALPHA.DAT/ALPHA1"));
    CHECK(scopeSpy.count() == 1);

    // Setting the same path is a no-op.
    trip.setSourcePath(QStringLiteral("ALPHA.DAT/ALPHA1"));
    CHECK(scopeSpy.count() == 1);
    trip.setSourcePath(QString());
    CHECK(scopeSpy.count() == 2);

    // The external centerline also moves the scope, so it fires scopeChanged.
    trip.setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/cave.svx")));
    CHECK(scopeSpy.count() == 3);

    // An external trip's scope is its own survey label, so a rename moves it.
    trip.setName(QStringLiteral("Topo 1"));
    CHECK(trip.scopePrefix() == QStringLiteral("topo_1."));
    CHECK(scopeSpy.count() == 4);
}

TEST_CASE("cwTrip solved-* accessors strip a native trip's label in a sourced root",
          "[cwTrip][scope]")
{
    // Seed a root lookup as the solver would leave it for a native trip a
    // source root holds: the trip's stations keyed <label>.<tail>, plus a
    // station of the root's file. The accessors must strip the label so bare
    // note/scrap names resolve, while keeping the full root data as a superset.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    makeSourceRoot(cave);
    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("A"));
    cave->addTrip(trip);
    REQUIRE(trip->scopePrefix() == QStringLiteral("a."));

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("A.s1"), QVector3D(1, 2, 3));
    lookup.setPosition(QStringLiteral("A.s2"), QVector3D(4, 5, 6));
    lookup.setPosition(QStringLiteral("outside"), QVector3D(7, 8, 9));
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);

    SECTION("solvedStationPositions aliases each scoped tail to a bare name") {
        const cwStationPositionLookup solved = trip->solvedStationPositions();
        REQUIRE(solved.hasPosition(QStringLiteral("s1")));
        REQUIRE(solved.hasPosition(QStringLiteral("s2")));
        CHECK(solved.position(QStringLiteral("s1")) == QVector3D(1, 2, 3));
        CHECK(solved.position(QStringLiteral("s2")) == QVector3D(4, 5, 6));
        // The full cave data is retained as a superset for cross-trip tie-ins.
        CHECK(solved.hasPosition(QStringLiteral("A.s1")));
        CHECK(solved.hasPosition(QStringLiteral("outside")));
    }

    SECTION("solvedStations enumerates only this trip's scope-relative tails") {
        const QList<QPair<QString, QVector3D>> stations = trip->solvedStations();
        REQUIRE(stations.size() == 2);
        QMap<QString, QVector3D> byName;
        for (const auto& pair : stations) {
            byName.insert(pair.first, pair.second);
        }
        CHECK(byName.value(QStringLiteral("s1")) == QVector3D(1, 2, 3));
        CHECK(byName.value(QStringLiteral("s2")) == QVector3D(4, 5, 6));
        CHECK_FALSE(byName.contains(QStringLiteral("outside")));
        CHECK_FALSE(byName.contains(QStringLiteral("A.s1")));
    }
}

namespace {

cwShot unitShot()
{
    cwShot shot;
    shot.setDistance(cwDistanceReading("10"));
    shot.setCompass(cwCompassReading("0"));
    shot.setBackCompass(cwCompassReading("180"));
    shot.setClino(cwClinoReading("0"));
    shot.setBackClino(cwClinoReading("0"));
    return shot;
}

void addShot(cwTrip* trip, const QString& from, const QString& to)
{
    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendShot(cwStation(from), cwStation(to), unitShot());
}

//! Issue #651's shape: cavern fixes one station per run and drops every
//! component it cannot reach, so the harvest of a two-part file learns only the
//! first part ("a1", "a2") — while the region solve, tied in per component,
//! placed "a2" and "b1". Neither source alone names all three stations.
cwTrip* attachmentWithSplitSources(cwCave* cave)
{
    cwTrip* trip = addExternalTrip(cave, QStringLiteral("Topo 1"));
    trip->setExternalStations({QStringLiteral("a1"), QStringLiteral("a2")});

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("topo_1.a2"), QVector3D(4, 5, 6));
    lookup.setPosition(QStringLiteral("topo_1.b1"), QVector3D(7, 8, 9));
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);
    return trip;
}

QStringList namesOf(const QList<cwTrip::KnownStation>& stations)
{
    QStringList names;
    for (const cwTrip::KnownStation& station : stations) {
        names.append(station.name);
    }
    return names;
}

std::optional<QVector3D> positionOf(const QList<cwTrip::KnownStation>& stations,
                                    const QString& name)
{
    for (const cwTrip::KnownStation& station : stations) {
        if (station.name == name) {
            return station.position;
        }
    }
    return std::optional<QVector3D>();
}

} // namespace

TEST_CASE("cwTrip::knownStations unions every source that names a station",
          "[cwTrip][knownStations]")
{
    // The defect this accessor exists to end: consumers that each preferred a
    // different source disagreed about which stations a trip has, on one screen.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);

    SECTION("a native trip keeps a chunk station the solve never placed") {
        cwTrip* trip = new cwTrip();
        cave->addTrip(trip);
        addShot(trip, QStringLiteral("s1"), QStringLiteral("s2"));

        cwStationPositionLookup lookup;
        lookup.setPosition(QStringLiteral("s1"), QVector3D(1, 2, 3));
        SurveyTreeTestHelper::setScopedPositions(cave, lookup);

        const QList<cwTrip::KnownStation> known = trip->knownStations();
        CHECK(namesOf(known) == QStringList({QStringLiteral("s1"), QStringLiteral("s2")}));
        CHECK(positionOf(known, QStringLiteral("s1")) == QVector3D(1, 2, 3));
        CHECK_FALSE(positionOf(known, QStringLiteral("s2")).has_value());
    }

    SECTION("an attachment gets the harvest's names and the solve's alike") {
        cwTrip* trip = attachmentWithSplitSources(cave);

        const QList<cwTrip::KnownStation> known = trip->knownStations();
        CHECK(namesOf(known) == QStringList({QStringLiteral("a1"),
                                             QStringLiteral("a2"),
                                             QStringLiteral("b1")}));
        CHECK_FALSE(positionOf(known, QStringLiteral("a1")).has_value());
        CHECK(positionOf(known, QStringLiteral("a2")) == QVector3D(4, 5, 6));
        CHECK(positionOf(known, QStringLiteral("b1")) == QVector3D(7, 8, 9));
    }

    SECTION("the authored spelling outranks the solver's lowercased echo") {
        // A person typed "S1"; the cave lookup keys it canonically. Listing the
        // key back would show the station in a case its author never used.
        makeSourceRoot(cave);
        cwTrip* trip = new cwTrip();
        trip->setName(QStringLiteral("A"));
        cave->addTrip(trip);
        addShot(trip, QStringLiteral("S1"), QStringLiteral("S2"));

        cwStationPositionLookup lookup;
        lookup.setPosition(QStringLiteral("a.s1"), QVector3D(1, 2, 3));
        SurveyTreeTestHelper::setScopedPositions(cave, lookup);

        const QList<cwTrip::KnownStation> known = trip->knownStations();
        CHECK(namesOf(known) == QStringList({QStringLiteral("S1"), QStringLiteral("S2")}));
        CHECK(positionOf(known, QStringLiteral("S1")) == QVector3D(1, 2, 3));
    }

    SECTION("stations come out in natural order, counting the way a caver does") {
        cwTrip* trip = addExternalTrip(cave, QStringLiteral("Topo 1"));
        trip->setExternalStations({QStringLiteral("a10"),
                                   QStringLiteral("a2"),
                                   QStringLiteral("a1")});

        CHECK(namesOf(trip->knownStations()) == QStringList({QStringLiteral("a1"),
                                                             QStringLiteral("a2"),
                                                             QStringLiteral("a10")}));
    }
}

TEST_CASE("cwTrip::knownStationNames answers for a snapshot as the live trip does",
          "[cwTrip][knownStations]")
{
    // The worker-side passes hold a cwTripData, not a trip. They must not get a
    // second opinion — a snapshot carries no cave lookup, so the solved names it
    // cannot derive are handed in.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = attachmentWithSplitSources(cave);

    const QStringList snapshot =
        cwTrip::knownStationNames(trip->data(), {QStringLiteral("a2"), QStringLiteral("b1")});
    CHECK(snapshot == namesOf(trip->knownStations()));

    SECTION("with nothing solved it is the harvest alone") {
        CHECK(cwTrip::knownStationNames(trip->data(), {})
              == QStringList({QStringLiteral("a1"), QStringLiteral("a2")}));
    }
}

TEST_CASE("cwTrip::knownStationsChanged is the one pulse for knownStations",
          "[cwTrip][knownStations]")
{
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* trip = addExternalTrip(cave, QStringLiteral("Topo 1"));
    cwTrip* sibling = addExternalTrip(cave, QStringLiteral("Topo 2"));

    cwSignalSpy spy(trip, &cwTrip::knownStationsChanged);

    SECTION("the cave's solve fires it") {
        cwStationPositionLookup lookup;
        lookup.setPosition(QStringLiteral("topo_1.a1"), QVector3D(1, 2, 3));
        SurveyTreeTestHelper::setScopedPositions(cave, lookup);
        CHECK(spy.count() == 1);
    }

    SECTION("a sibling's harvest does not, because it moved no station of this trip's") {
        // The cave's aggregate carries that instead. Relaying it back into every
        // trip would double the pulse each trip already fires for itself.
        cwSignalSpy caveSpy(cave, &cwCave::tripExternalStationsChanged);
        sibling->setExternalStations({QStringLiteral("b1")});
        CHECK(spy.count() == 0);
        CHECK(caveSpy.count() == 1);
    }

    SECTION("this trip's harvest reaches the cave's aggregate exactly once") {
        // The tie suggester binds to one trip and reads the whole cave's
        // stations. Without the aggregate a candidate trip's names arrive only
        // if the scan happens to request a re-solve as well.
        cwSignalSpy caveSpy(cave, &cwCave::tripExternalStationsChanged);
        trip->setExternalStations({QStringLiteral("a1")});
        CHECK(spy.count() == 1);
        CHECK(caveSpy.count() == 1);
    }

    SECTION("a trip the cave no longer lists stops reaching the aggregate") {
        // The stack keeps the removed trip alive to be spied on; without one
        // cwUndoer::pushUndo deletes the command and deleteLater()s the trip.
        QUndoStack undoStack;
        region.setUndoStack(&undoStack);
        cave->removeTrip(cave->indexOf(trip));

        cwSignalSpy caveSpy(cave, &cwCave::tripExternalStationsChanged);
        trip->setExternalStations({QStringLiteral("a1")});
        CHECK(caveSpy.count() == 0);
    }
}

TEST_CASE("cwTrip::linePlotKeywordModel carries Type=Line Plot and extends the trip model",
          "[cwTrip][keyword]")
{
    cwTrip trip;
    trip.setName(QStringLiteral("Trip A"));

    cwKeywordModel* linePlotModel = trip.linePlotKeywordModel();
    REQUIRE(linePlotModel != nullptr);

    SECTION("lazily created and stable across calls") {
        CHECK(trip.linePlotKeywordModel() == linePlotModel);
    }

    SECTION("aggregates Type=Line Plot and the trip's keywords via the extension") {
        // keywords() walks extensions, so the line plot model reports both its
        // own Type=Line Plot and the trip's keywords (Trip name, ...).
        const auto all = linePlotModel->keywords();
        CHECK(all.contains(cwKeyword(cwKeywordModel::TypeKey, QStringLiteral("Line Plot"))));
        CHECK(all.contains(cwKeyword(cwKeywordModel::TripNameKey, QStringLiteral("Trip A"))));
    }

    SECTION("the Type identity stays off the trip's own model") {
        // Scraps/notes/leads extend trip->keywordModel(); putting Type=Line Plot
        // there would make them wrongly inherit "Line Plot".
        CHECK_FALSE(trip.keywordModel()->keywords().contains(
            cwKeyword(cwKeywordModel::TypeKey, QStringLiteral("Line Plot"))));
    }
}


TEST_CASE("cwTrip::externallyBacked covers the trip's own attachment and its cave's",
          "[cwTrip][scope]")
{
    // The predicate the note-station validator and TripPage's panel swap both
    // ask. It has to answer for a derived trip — no attachment, no chunks, all
    // its stations coming from its root's file — as well as for an attached
    // trip.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Topo 1"));
    cave->addTrip(trip);

    cwSignalSpy backedSpy(trip, &cwTrip::externallyBackedChanged);

    // A native trip in a native cave owns nothing external.
    CHECK_FALSE(trip->externallyBacked());

    // Its own attachment backs it.
    trip->setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/cave.svx")));
    CHECK(trip->externallyBacked());
    CHECK(backedSpy.count() == 1);

    trip->setExternalCenterline(cwExternalCenterline());
    CHECK_FALSE(trip->externallyBacked());
    CHECK(backedSpy.count() == 2);

    // The cave's attachment backs every trip in it, both directions, and the
    // trip hears about it through the cave's relay.
    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("blocks.svx")));
    CHECK(trip->externallyBacked());
    CHECK(backedSpy.count() == 3);

    // Inside a sourced subtree a native trip is the user's own survey, while a
    // derived one comes from the file.
    makeSourceRoot(cave);
    CHECK_FALSE(trip->externallyBacked());
    trip->setSourcePath(QStringLiteral("#own"));
    CHECK(trip->externallyBacked());
    trip->setSourcePath(QString());
    cave->setSourceId(QUuid());

    cave->setExternalCenterline(cwExternalCenterline());
    CHECK_FALSE(trip->externallyBacked());

    // A trip with no cave answers from its own fields alone.
    cwTrip orphan;
    CHECK_FALSE(orphan.externallyBacked());
    orphan.setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/east.svx")));
    CHECK(orphan.externallyBacked());
}

TEST_CASE("A trip the cave no longer lists stops hearing its attachment",
          "[cwTrip][scope]")
{
    // The disconnect half of the relay. The undo stack keeps the removed trip
    // alive; without one cwUndoer::pushUndo deletes the command and the trip
    // with it.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* staying = new cwTrip();
    staying->setName(QStringLiteral("Topo 1"));
    cave->addTrip(staying);

    cwTrip* removed = new cwTrip();
    removed->setName(QStringLiteral("Topo 2"));
    cave->addTrip(removed);

    QUndoStack undoStack;
    region.setUndoStack(&undoStack);

    cave->removeTrip(cave->indexOf(removed));

    cwSignalSpy stayingSpy(staying, &cwTrip::externallyBackedChanged);
    cwSignalSpy removedSpy(removed, &cwTrip::externallyBackedChanged);

    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("blocks.svx")));

    CHECK(stayingSpy.count() == 1);
    CHECK(removedSpy.count() == 0);
}

TEST_CASE("cwTrip::windowsWholeNode names the source root's own unit",
          "[cwTrip][scope]")
{
    // The root's own unit (the entry file's own shots) owns every station no
    // other unit claims. The truth table is what the geometry pass and the trip
    // panel branch on.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Fisher Ridge"));
    region.addCave(cave);

    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("blocks"));
    cave->addTrip(trip);

    const auto snapshotSaysWindow = [&]() {
        const cwCaveData caveData = cave->data();
        REQUIRE(caveData.trips.size() == 1);
        return cwTrip::windowsWholeNode(caveData.trips.first(), caveData);
    };

    SECTION("a trip in a native node is no window") {
        trip->setSourcePath(cwTrip::ownUnitSourcePath());
        CHECK_FALSE(trip->windowsWholeNode());
        CHECK_FALSE(snapshotSaysWindow());
    }

    SECTION("the root's own unit is the window") {
        makeSourceRoot(cave);
        trip->setSourcePath(cwTrip::ownUnitSourcePath());
        CHECK(trip->windowsWholeNode());
        CHECK(snapshotSaysWindow());
        //Its scope is the root itself.
        CHECK(trip->scopePrefix().isEmpty());
    }

    SECTION("another unit of the root windows only its own stations") {
        makeSourceRoot(cave);
        trip->setSourcePath(QStringLiteral("ALPHA1"));
        CHECK_FALSE(trip->windowsWholeNode());
        CHECK_FALSE(snapshotSaysWindow());
    }

    SECTION("a native trip under the root is no window") {
        makeSourceRoot(cave);
        CHECK_FALSE(trip->windowsWholeNode());
        CHECK_FALSE(snapshotSaysWindow());
    }

    SECTION("a trip with no node at all answers no") {
        cwTrip orphan;
        orphan.setSourcePath(cwTrip::ownUnitSourcePath());
        CHECK_FALSE(orphan.windowsWholeNode());
    }
}

TEST_CASE("Derived trips read their stations by unit-set membership",
          "[cwTrip][scope]")
{
    // A Survex root with shots of its own and one block: the root's view holds
    // every station of the file, keyed by its cavern-relative name.
    cwCavingRegion region;
    cwCave* root = new cwCave();
    root->setName(QStringLiteral("Fisher Ridge"));
    root->setExternalCenterline(cwExternalCenterline(QStringLiteral("root_and_block.svx")));
    region.addCave(root);
    makeSourceRoot(root);

    cwTrip* window = addDerivedTrip(root, QStringLiteral("root_and_block"),
                                    cwTrip::ownUnitSourcePath(), {QStringLiteral("r1")});
    cwCave* side = addDerivedNode(root, QStringLiteral("Side"), QStringLiteral("side"),
                                  QStringLiteral("side"));
    cwTrip* block = addDerivedTrip(side, QStringLiteral("Side"),
                                   QStringLiteral("side/#own"),
                                   {QStringLiteral("s1"), QStringLiteral("s2")});
    cwTrip* shared = addDerivedTrip(side, QStringLiteral("Shared"), QStringLiteral("side/b"),
                                    {QStringLiteral("s2"), QStringLiteral("s3")});

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("r1"), QVector3D(1, 2, 3));
    lookup.setPosition(QStringLiteral("r2"), QVector3D(4, 5, 6));
    lookup.setPosition(QStringLiteral("side.s1"), QVector3D(7, 8, 9));
    lookup.setPosition(QStringLiteral("side.s2"), QVector3D(10, 11, 12));
    lookup.setPosition(QStringLiteral("side.s3"), QVector3D(13, 14, 15));
    SurveyTreeTestHelper::setScopedPositions(root, lookup);

    SECTION("the whole-node window owns every station no unit claims") {
        const QMap<QString, QVector3D> stations = byName(window->solvedStations());
        CHECK(stations.keys() == QStringList({QStringLiteral("r1"), QStringLiteral("r2")}));
        CHECK(stations.value(QStringLiteral("r2")) == QVector3D(4, 5, 6));
    }

    SECTION("a unit's stations come back named below its node's cavern path") {
        const QMap<QString, QVector3D> stations = byName(block->solvedStations());
        CHECK(stations.keys() == QStringList({QStringLiteral("s1"), QStringLiteral("s2")}));
        CHECK(stations.value(QStringLiteral("s1")) == QVector3D(7, 8, 9));
    }

    SECTION("a station two units share belongs to both") {
        CHECK(byName(shared->solvedStations()).contains(QStringLiteral("s2")));
        CHECK(byName(block->solvedStations()).contains(QStringLiteral("s2")));
    }

    SECTION("the derived node's view is its members, its cavern path stripped") {
        const cwStationPositionLookup view = side->scopedPositions();
        CHECK(view.positions().keys()
              == QStringList({QStringLiteral("s1"), QStringLiteral("s2"), QStringLiteral("s3")}));
        CHECK(block->solvedStationPositions().hasPosition(QStringLiteral("s3")));
    }

    SECTION("a derived trip's stations travel as handles on the trip") {
        const cwStationHandle handle = block->stationHandle(QStringLiteral("s1"));
        CHECK(handle.scope() == cwStationHandle::Trip);
        CHECK(handle.containerId() == block->id());
    }

    SECTION("a native trip under the derived node reads its label scope") {
        cwTrip* native = new cwTrip();
        native->setName(QStringLiteral("Side"));
        side->addTrip(native);
        //"side" is a scope the file opens itself.
        REQUIRE(native->scopePrefix() == QStringLiteral("side_2."));

        cwStationPositionLookup withNative = lookup;
        withNative.setPosition(QStringLiteral("side_2.n1"), QVector3D(16, 17, 18));
        SurveyTreeTestHelper::setScopedPositions(root, withNative);

        CHECK(side->scopedPositions().hasPosition(QStringLiteral("side_2.n1")));
        const QMap<QString, QVector3D> stations = byName(native->solvedStations());
        CHECK(stations.keys() == QStringList({QStringLiteral("n1")}));
        // The window leaves a native trip's stations to it.
        CHECK_FALSE(byName(window->solvedStations()).contains(QStringLiteral("side_2.n1")));
    }

    SECTION("a native trip under a native Folder inside the subtree reads the root's block") {
        auto* folder = new cwCave();
        folder->setName(QStringLiteral("Folder"));
        side->addNode(folder);
        REQUIRE(folder->sourceRoot() == root);

        cwTrip* dig = new cwTrip();
        dig->setName(QStringLiteral("Dig"));
        folder->addTrip(dig);
        REQUIRE(dig->scopePrefix() == QStringLiteral("dig."));

        cwStationPositionLookup withDig = lookup;
        withDig.setPosition(QStringLiteral("dig.d1"), QVector3D(19, 20, 21));
        SurveyTreeTestHelper::setScopedPositions(root, withDig);

        CHECK(folder->scopedPositions().positions().keys()
              == QStringList({QStringLiteral("dig.d1")}));
        const QMap<QString, QVector3D> stations = byName(dig->solvedStations());
        CHECK(stations.keys() == QStringList({QStringLiteral("d1")}));
        CHECK(stations.value(QStringLiteral("d1")) == QVector3D(19, 20, 21));
    }
}

TEST_CASE("cwTrip::chunk returns nullptr for an index outside the chunk list", "[cwTrip]")
{
    cwTrip trip;
    trip.addChunk(chunkWithNames({QStringLiteral("A1"), QStringLiteral("A2")}));
    trip.addChunk(chunkWithNames({QStringLiteral("B1"), QStringLiteral("B2")}));
    REQUIRE(trip.chunkCount() == 2);

    CHECK(trip.chunk(0) == trip.chunks().at(0));
    CHECK(trip.chunk(trip.chunkCount() - 1) == trip.chunks().last());
    CHECK(trip.chunk(trip.chunkCount()) == nullptr);
    CHECK(trip.chunk(-1) == nullptr);
}

TEST_CASE("cwTrip::removeChunks clamps an end past the last chunk to the last index", "[cwTrip]")
{
    cwTrip trip;
    cwSurveyChunk* a = chunkWithNames({QStringLiteral("A1"), QStringLiteral("A2")});
    trip.addChunk(a);
    trip.addChunk(chunkWithNames({QStringLiteral("B1"), QStringLiteral("B2")}));
    trip.addChunk(chunkWithNames({QStringLiteral("C1"), QStringLiteral("C2")}));
    REQUIRE(trip.chunkCount() == 3);

    QSignalSpy aboutToRemoveSpy(&trip, &cwTrip::chunksAboutToBeRemoved);
    QSignalSpy removedSpy(&trip, &cwTrip::chunksRemoved);

    SECTION("A range that starts inside the list removes through the last chunk") {
        trip.removeChunks(1, 5);

        CHECK(trip.chunks() == QList<cwSurveyChunk*>{a});
        REQUIRE(aboutToRemoveSpy.count() == 1);
        CHECK(aboutToRemoveSpy.at(0).at(0).toInt() == 1);
        CHECK(aboutToRemoveSpy.at(0).at(1).toInt() == 2);
        REQUIRE(removedSpy.count() == 1);
        CHECK(removedSpy.at(0).at(0).toInt() == 1);
        CHECK(removedSpy.at(0).at(1).toInt() == 2);
    }

    SECTION("A range that starts past the last chunk removes nothing") {
        trip.removeChunks(3, 5);

        CHECK(trip.chunkCount() == 3);
        CHECK(aboutToRemoveSpy.count() == 0);
        CHECK(removedSpy.count() == 0);
    }
}

namespace {

//! Checks that every station of \a stations sits where casePairs::solvedLookup()
//! puts the station its name resolves to.
void checkCasePairPositions(const QList<QPair<QString, QVector3D>>& stations)
{
    const cwStationPositionLookup solved = casePairs::solvedLookup();
    for (const auto& station : stations) {
        INFO("station " << station.first.toStdString());
        CHECK(casePairs::samePoint(station.second, solved.position(station.first)));
    }
}

} // namespace

TEST_CASE("cwTrip::knownStations lists both members of an attached file's case pair",
          "[cwTrip][knownStations][CasePairs]")
{
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = addExternalTrip(cave, QStringLiteral("Topo 1"));
    trip->setExternalStations(casePairs::harvest());
    SurveyTreeTestHelper::setScopedPositions(
        cave, casePairs::solvedLookupUnder(cwStation::canonicalKey(trip->scopePrefix())));

    // Q1 pairs with nothing, so the harvest's Q1 and the solve's q1 are one.
    const QList<cwTrip::KnownStation> known = trip->knownStations();
    CHECK(casePairs::joined(namesOf(known)) == casePairs::joined(casePairs::harvest()));

    const cwStationPositionLookup solved = casePairs::solvedLookup();
    for (const QString& name : casePairs::harvest()) {
        INFO("station " << name.toStdString());
        const std::optional<QVector3D> position = positionOf(known, name);
        REQUIRE(position.has_value());
        CHECK(casePairs::samePoint(*position, solved.position(name)));
    }

    SECTION("the snapshot overload lists the same stations") {
        QStringList solvedNames;
        for (const auto& station : trip->solvedStations()) {
            solvedNames.append(station.first);
        }
        CHECK(casePairs::joined(cwTrip::knownStationNames(trip->data(), solvedNames))
              == casePairs::joined(casePairs::harvest()));
    }
}

TEST_CASE("cwTrip::knownStations keeps a native trip's A1 and a1 one station",
          "[cwTrip][knownStations][CasePairs]")
{
    // Cavern lowercases a native name, so two spellings a person typed are one
    // station, even in a cave whose own attached file has case pairs.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    const bool caveAttached = GENERATE(false, true);
    INFO("the cave has an attached file: " << caveAttached);
    if (caveAttached) {
        cave->setExternalCenterline(cwExternalCenterline(casePairs::kFileName));
    }

    cwTrip* trip = new cwTrip();
    cave->addTrip(trip);
    addShot(trip, QStringLiteral("A1"), QStringLiteral("A2"));
    addShot(trip, QStringLiteral("a1"), QStringLiteral("a3"));
    REQUIRE(trip->externallyBacked() == caveAttached);

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("a1"), QVector3D(1, 2, 3));
    lookup.setPosition(QStringLiteral("a2"), QVector3D(4, 5, 6));
    lookup.setPosition(QStringLiteral("a3"), QVector3D(7, 8, 9));
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);

    const QList<cwTrip::KnownStation> known = trip->knownStations();
    CHECK(namesOf(known) == QStringList({QStringLiteral("A1"), QStringLiteral("A2"),
                                         QStringLiteral("a3")}));
    CHECK(positionOf(known, QStringLiteral("A1")) == QVector3D(1, 2, 3));
}

TEST_CASE("A derived trip reads each case-pair member's own position",
          "[cwTrip][scope][CasePairs]")
{
    // The line-plot labels draw solvedStations(): P2's label has to sit at P2.
    cwCavingRegion region;
    cwCave* root = new cwCave();
    root->setName(QStringLiteral("Pairs"));
    root->setExternalCenterline(cwExternalCenterline(casePairs::kFileName));
    region.addCave(root);
    makeSourceRoot(root);
    cwTrip* derived = addDerivedTrip(root, QStringLiteral("Pairs"), QStringLiteral("pairs"),
                                     casePairs::harvest());
    REQUIRE(derived->scopePrefix().isEmpty());
    REQUIRE_FALSE(derived->windowsWholeNode());
    SurveyTreeTestHelper::setScopedPositions(root, casePairs::solvedLookupUnder(QString()));

    const QList<QPair<QString, QVector3D>> stations = derived->solvedStations();
    QStringList names;
    for (const auto& station : stations) {
        names.append(station.first);
    }
    CHECK(casePairs::joined(names) == casePairs::joined(casePairs::harvest()));
    checkCasePairPositions(stations);
}

TEST_CASE("A native trip's P2 reads the station cavern lowercased it to",
          "[cwTrip][scope][CasePairs]")
{
    // A cave-level Compass file with a case pair, and a native trip in the same
    // scope: cavern reads the native P2 as p2, so that is where it sits.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Pairs"));
    cave->setExternalCenterline(cwExternalCenterline(casePairs::kFileName));
    region.addCave(cave);
    cwTrip* native = new cwTrip();
    cave->addTrip(native);
    addShot(native, QStringLiteral("P2"), QStringLiteral("N1"));
    REQUIRE(native->scopePrefix().isEmpty());

    cwStationPositionLookup lookup = casePairs::solvedLookupUnder(QString());
    lookup.setPosition(QStringLiteral("n1"), QVector3D(0, 0, 0));
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);

    const QMap<QString, QVector3D> stations = byName(native->solvedStations());
    REQUIRE(stations.contains(QStringLiteral("P2")));
    CHECK(casePairs::samePoint(stations.value(QStringLiteral("P2")),
                               casePairs::solvedLookup().position(QStringLiteral("p2"))));
}
