//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTrip.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwKeywordModel.h"
#include "cwKeyword.h"
#include "cwStationPositionLookup.h"

//Qt inculdes
#include "cwSignalSpy.h"
#include <QUndoStack>
#include <QVector3D>

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

    SECTION("a native-prefixed trip scopes to <stationPrefix>.") {
        trip.setStationPrefix(QStringLiteral("A"));
        CHECK(trip.scopePrefix() == QStringLiteral("A."));
        CHECK(trip.isScoped());
    }

    SECTION("external centerline wins over a station prefix") {
        trip.setName(QStringLiteral("Topo 1"));
        trip.setStationPrefix(QStringLiteral("A"));
        trip.setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/cave.svx")));
        CHECK(trip.scopePrefix() == QStringLiteral("topo_1."));
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

TEST_CASE("cwTrip scope changes emit stationPrefixChanged and scopeChanged", "[cwTrip][scope]")
{
    cwTrip trip;
    cwSignalSpy prefixSpy(&trip, &cwTrip::stationPrefixChanged);
    cwSignalSpy scopeSpy(&trip, &cwTrip::scopeChanged);

    trip.setStationPrefix(QStringLiteral("A"));
    CHECK(prefixSpy.count() == 1);
    CHECK(scopeSpy.count() == 1);

    // Setting the same prefix is a no-op.
    trip.setStationPrefix(QStringLiteral("A"));
    CHECK(prefixSpy.count() == 1);
    CHECK(scopeSpy.count() == 1);

    // The external centerline also moves the scope, so it fires scopeChanged.
    trip.setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/cave.svx")));
    CHECK(prefixSpy.count() == 1);
    CHECK(scopeSpy.count() == 2);

    // An external trip's scope is its own survey label, so a rename moves it.
    trip.setName(QStringLiteral("Topo 1"));
    CHECK(trip.scopePrefix() == QStringLiteral("topo_1."));
    CHECK(scopeSpy.count() == 3);
}

TEST_CASE("cwTrip solved-* accessors strip a native station prefix", "[cwTrip][scope]")
{
    // Seed a cave lookup as the solver would leave it for a native-prefixed
    // (Scope) trip: the trip's stations keyed <prefix>.<tail>, plus an unrelated
    // native station. The accessors must strip the prefix so bare note/scrap
    // names resolve, while keeping the full cave data as a superset.
    cwCave cave;
    cwTrip* trip = new cwTrip();
    trip->setStationPrefix(QStringLiteral("A"));
    cave.addTrip(trip);

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("A.s1"), QVector3D(1, 2, 3));
    lookup.setPosition(QStringLiteral("A.s2"), QVector3D(4, 5, 6));
    lookup.setPosition(QStringLiteral("outside"), QVector3D(7, 8, 9));
    cave.setStationPositionLookup(lookup);

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

