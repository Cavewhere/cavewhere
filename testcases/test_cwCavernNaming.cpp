/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The format contract for the survey-scope labels CaveWhere writes into Survex.
// These labels are user-visible in two places — the driver text on the
// CavernOutputPage and any .svx the user exports — and they are also what the
// worker decodes cavern's output with, so encode and decode are pinned here
// together. Anything that changes the label of a given (name, sibling set) pair
// changes both halves at once, which is exactly the drift these cases catch.

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Cavewhere includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCavernNaming.h"
#include "cwLinePlotManager.h"
#include "cwLinePlotTask.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

// Qt includes
#include <QRegularExpression>
#include <QUuid>

namespace {

cwCave* addCaveWithSimpleShot(cwCavingRegion& region,
                              const QString& displayName,
                              const QString& fromName,
                              const QString& toName,
                              double distance)
{
    cwCave* cave = new cwCave();
    cave->setName(displayName);
    region.addCave(cave);

    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Trip"));
    cave->addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);

    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(distance)));
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    chunk->appendShot(cwStation(fromName), cwStation(toName), shot);

    return cave;
}

cwCavernNaming::ScopeEntry entry(const QString& name)
{
    return {QUuid::createUuid(), name};
}

} // namespace

TEST_CASE("sanitizeToCavernIdentifier folds a name to a legal cavern identifier",
          "[LinePlot][CavernNaming]")
{
    using cwCavernNaming::sanitizeToCavernIdentifier;

    SECTION("a plain name survives, lowercased") {
        CHECK(sanitizeToCavernIdentifier(QStringLiteral("Fisher Ridge"))
              == QStringLiteral("fisher_ridge"));
        CHECK(sanitizeToCavernIdentifier(QStringLiteral("topo1"))
              == QStringLiteral("topo1"));
    }

    SECTION("a dot is folded, never kept") {
        // A surviving dot would open a survey level nobody asked for, so
        // "St. Marks" must not become the "marks" survey inside "st".
        CHECK(sanitizeToCavernIdentifier(QStringLiteral("St. Marks"))
              == QStringLiteral("st_marks"));
    }

    SECTION("runs of punctuation collapse and trailing runs vanish") {
        CHECK(sanitizeToCavernIdentifier(QStringLiteral("Big  --  Cave!!!"))
              == QStringLiteral("big_cave"));
        CHECK(sanitizeToCavernIdentifier(QStringLiteral("  Lechuguilla  "))
              == QStringLiteral("lechuguilla"));
    }

    SECTION("a Latin diacritic drops to its base letter") {
        CHECK(sanitizeToCavernIdentifier(QStringLiteral("Cueva del Río Verde"))
              == QStringLiteral("cueva_del_rio_verde"));
    }

    SECTION("a name with nothing to keep still yields a usable label") {
        // An unnamed cave has to emit *something* legal — an empty *begin
        // label is a cavern parse error, which would take down the whole solve.
        CHECK(sanitizeToCavernIdentifier(QString()) == QStringLiteral("x"));
        CHECK(sanitizeToCavernIdentifier(QStringLiteral("!!!")) == QStringLiteral("x"));
    }

    SECTION("every result is a legal cavern survey identifier") {
        static const QRegularExpression legal(QStringLiteral("^[a-z0-9_]+$"));
        const QStringList names = {
            QStringLiteral("Fisher Ridge"),
            QStringLiteral("St. Marks"),
            QStringLiteral("Big  --  Cave!!!"),
            QStringLiteral("Cueva del Río Verde"),
            QStringLiteral("Сумган"),
            QString(),
        };
        for (const QString& name : names) {
            INFO("name: " << name.toStdString());
            CHECK(legal.match(sanitizeToCavernIdentifier(name)).hasMatch());
        }
    }
}

TEST_CASE("scopeLabels gives every sibling its own label", "[LinePlot][CavernNaming]")
{
    SECTION("distinct names keep their own identifiers") {
        const QList<cwCavernNaming::ScopeEntry> siblings = {
            entry(QStringLiteral("Fisher Ridge")),
            entry(QStringLiteral("Bat Cave")),
        };
        const QHash<QUuid, QString> labels = cwCavernNaming::scopeLabels(siblings);
        CHECK(labels.value(siblings.at(0).id) == QStringLiteral("fisher_ridge"));
        CHECK(labels.value(siblings.at(1).id) == QStringLiteral("bat_cave"));
    }

    SECTION("names that sanitize alike are still told apart") {
        // Two caves sharing a label would open the same survey twice and
        // silently merge their stations — and the decode side would route
        // every one of them into whichever cave won the map. This suffix is
        // the only thing standing between those two caves.
        const QList<cwCavernNaming::ScopeEntry> siblings = {
            entry(QStringLiteral("Big Cave")),
            entry(QStringLiteral("Big-Cave")),
            entry(QStringLiteral("BIG CAVE")),
        };
        const QHash<QUuid, QString> labels = cwCavernNaming::scopeLabels(siblings);
        CHECK(labels.value(siblings.at(0).id) == QStringLiteral("big_cave"));
        CHECK(labels.value(siblings.at(1).id) == QStringLiteral("big_cave_2"));
        CHECK(labels.value(siblings.at(2).id) == QStringLiteral("big_cave_3"));
    }

    SECTION("a name that already looks like a suffixed label does not collide") {
        const QList<cwCavernNaming::ScopeEntry> siblings = {
            entry(QStringLiteral("Big Cave")),
            entry(QStringLiteral("Big Cave 2")),
            entry(QStringLiteral("Big-Cave")),
        };
        const QHash<QUuid, QString> labels = cwCavernNaming::scopeLabels(siblings);
        CHECK(labels.value(siblings.at(0).id) == QStringLiteral("big_cave"));
        CHECK(labels.value(siblings.at(1).id) == QStringLiteral("big_cave_2"));
        CHECK(labels.value(siblings.at(2).id) == QStringLiteral("big_cave_3"));
    }

    SECTION("the assignment depends only on the ordered snapshot") {
        // The exporter labels caves on one thread and the worker decodes them
        // on another, each rebuilding the assignment from its own copy of the
        // same list. That only works if the function is pure.
        const QList<cwCavernNaming::ScopeEntry> siblings = {
            entry(QStringLiteral("Big Cave")),
            entry(QStringLiteral("Big-Cave")),
        };
        CHECK(cwCavernNaming::scopeLabels(siblings) == cwCavernNaming::scopeLabels(siblings));
    }

    SECTION("an id that is not a sibling has no label") {
        const QList<cwCavernNaming::ScopeEntry> siblings = {entry(QStringLiteral("Alpha"))};
        CHECK(cwCavernNaming::scopeLabels(siblings).value(QUuid::createUuid()).isEmpty());
    }

    SECTION("scopePrefix is the label plus the separator") {
        CHECK(cwCavernNaming::scopePrefix(QStringLiteral("fisher_ridge"))
              == QStringLiteral("fisher_ridge."));
        // A scope nothing named has no prefix, rather than a bare separator that
        // would open a survey level with an empty name.
        CHECK(cwCavernNaming::scopePrefix(QString()).isEmpty());
    }
}

TEST_CASE("A scoped station name splits back into scope and remainder",
          "[LinePlot][CavernNaming]")
{
    using cwCavernNaming::removeScopeHead;
    using cwCavernNaming::scopeHeadOf;

    SECTION("only the first segment is the scope") {
        // A nested external scope and a dotted tail both live in the
        // remainder; splitLookupByCave peels one cave level and hands the rest
        // to the cave-local lookup untouched.
        CHECK(scopeHeadOf(QStringLiteral("fisher_ridge.a1")) == QStringLiteral("fisher_ridge"));
        CHECK(removeScopeHead(QStringLiteral("fisher_ridge.a1")) == QStringLiteral("a1"));

        CHECK(scopeHeadOf(QStringLiteral("fisher_ridge.topo1.simple.a1"))
              == QStringLiteral("fisher_ridge"));
        CHECK(removeScopeHead(QStringLiteral("fisher_ridge.topo1.simple.a1"))
              == QStringLiteral("topo1.simple.a1"));
    }

    SECTION("an unscoped name carries no head and is returned whole") {
        CHECK(scopeHeadOf(QStringLiteral("a1")).isEmpty());
        CHECK(removeScopeHead(QStringLiteral("a1")) == QStringLiteral("a1"));
    }

    SECTION("a leading separator is not a scope") {
        // ".a1" is the absolute-reference form, not a name scoped by an empty
        // label; treating it as the latter would key the lookup on "".
        CHECK(scopeHeadOf(QStringLiteral(".a1")).isEmpty());
    }

    SECTION("a scope with nothing after it leaves a blank remainder") {
        // The decode has to reject these itself: the split reports a perfectly
        // good cave scope, and neither cwStationPositionLookup::setPosition nor
        // cwStation::canonicalKey trims, so a blank tail would become a lookup
        // key no chunk station can ever match. Walls' empty-name quirk is how
        // one reaches the .3d in the first place.
        CHECK(scopeHeadOf(QStringLiteral("fisher_ridge.")) == QStringLiteral("fisher_ridge"));
        CHECK(removeScopeHead(QStringLiteral("fisher_ridge.")).isEmpty());

        CHECK(removeScopeHead(QStringLiteral("fisher_ridge. ")).trimmed().isEmpty());
        CHECK(removeScopeHead(QStringLiteral("fisher_ridge.\t")).trimmed().isEmpty());
    }
}

TEST_CASE("Two-cave pipeline keeps each cave's positions keyed by its own label",
          "[LinePlot][CavernNaming]")
{
    // End-to-end gate: two caves whose station names collide ("a1" appears in
    // both). Each cave must get its own position lookup with no
    // cross-contamination, which is what the per-cave *begin label buys.
    cwCavingRegion region;
    cwCave* caveA = addCaveWithSimpleShot(region,
                                          QStringLiteral("Alpha"),
                                          QStringLiteral("a1"),
                                          QStringLiteral("a2"),
                                          10.0);
    cwCave* caveB = addCaveWithSimpleShot(region,
                                          QStringLiteral("Beta"),
                                          QStringLiteral("a1"),
                                          QStringLiteral("a2"),
                                          20.0);

    REQUIRE(caveA->id() != caveB->id());

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(&region);
    plotManager->waitToFinish();

    // Each cave's a1 anchors at the origin; the colliding a2 must NOT
    // share a position across caves. The 10m vs 20m shot distances make
    // any accidental cross-write trivially detectable.
    CHECK(caveA->stationPositionLookup().position(QStringLiteral("a1"))
          == QVector3D(0.0f, 0.0f, 0.0f));
    CHECK(caveA->stationPositionLookup().position(QStringLiteral("a2"))
          == QVector3D(0.0f, 10.0f, 0.0f));

    CHECK(caveB->stationPositionLookup().position(QStringLiteral("a1"))
          == QVector3D(0.0f, 0.0f, 0.0f));
    CHECK(caveB->stationPositionLookup().position(QStringLiteral("a2"))
          == QVector3D(0.0f, 20.0f, 0.0f));

    // Per-cave length echoes the shot input; if both caves were merged
    // into one lookup the totals would be wrong (or one cave would have
    // zero length because cavern overwrote its prefix).
    CHECK(caveA->length()->value() == 10.0);
    CHECK(caveB->length()->value() == 20.0);
}

TEST_CASE("Two caves whose names sanitize alike still solve separately",
          "[LinePlot][CavernNaming]")
{
    // The collision suffix under load: without it both caves emit
    // "*begin big_cave", cavern merges the two surveys, and every station
    // decodes into whichever cave the label map happened to keep.
    cwCavingRegion region;
    cwCave* caveA = addCaveWithSimpleShot(region,
                                          QStringLiteral("Big Cave"),
                                          QStringLiteral("a1"),
                                          QStringLiteral("a2"),
                                          10.0);
    cwCave* caveB = addCaveWithSimpleShot(region,
                                          QStringLiteral("Big-Cave"),
                                          QStringLiteral("a1"),
                                          QStringLiteral("a2"),
                                          20.0);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(&region);
    plotManager->waitToFinish();

    INFO("driver:\n" << plotManager->driverSource().toStdString());
    REQUIRE_FALSE(plotManager->hasSolveError());

    CHECK(caveA->stationPositionLookup().position(QStringLiteral("a2"))
          == QVector3D(0.0f, 10.0f, 0.0f));
    CHECK(caveB->stationPositionLookup().position(QStringLiteral("a2"))
          == QVector3D(0.0f, 20.0f, 0.0f));
    CHECK(caveA->length()->value() == 10.0);
    CHECK(caveB->length()->value() == 20.0);
}
