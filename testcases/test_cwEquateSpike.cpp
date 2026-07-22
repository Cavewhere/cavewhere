/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// THROWAWAY SPIKE — plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html §4 commit 1.
//
// Proves cavern can solve a cross-scope *equate at both scopes CaveWhere will
// emit from — cave scope (two isolated *begin trip_<hex> blocks inside one
// *begin cave_<hex>, tied by a cave-relative name) and region scope (two
// *begin cave_<hex> blocks tied by a fully-qualified cave_<hex>.<tail> name) —
// and that both equated names land at one coordinate, so the existing decode
// (cwSurvex3DFileReader keeps both labels) already represents the join.
//
// FINDING — the export barrier the plan feared is NOT real for our driver.
// The plan assumed a qualified *equate reaching across a nested *begin needs
// the operands to be *export-ed (cavern error 26 "not exported"), and proposed
// a "*infer exports on" line to sidestep it. The spike disproves that: error 26
// only fires when a station WAS *export-ed but not to a deep enough level
// (readval.c:329 runs only when min_export is neither 0 nor USHRT_MAX; a
// never-exported station keeps min_export == 0 and the check is a no-op). The
// survex corpus confirms the asymmetry: export.svx passes because inner.1 is
// *export-ed at BOTH levels; exporterr1b.svx fails because it is *export-ed at
// the inner level only — an incomplete chain. CaveWhere's driver never emits
// *export at all, so a bare qualified *equate just works. Emission is therefore
// a single *equate line at the tie's scope, with no *export and no *infer.
//
// Delete this file once the emission points land in cwSurvexExporterCaveTask
// (cave scope) and cwSurvexExporterRegion (region scope).

// Catch
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Cavewhere
#include "cwCavernNaming.h"
#include "cwCavernRunner.h"
#include "cwStationPositionLookup.h"
#include "cwSurvex3DFileReader.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>

namespace {

struct SolveOutcome {
    bool solved = false;
    QString log;
    cwStationPositionLookup lookup;
};

//! Write driver text to a temp .svx, run a real cavern solve, and read the
//! resulting station positions back. PID-namespaced temp dir keeps concurrent
//! test processes from colliding.
SolveOutcome solveDriver(const QString& driver)
{
    QTemporaryDir tempDir(QDir::tempPath()
        + QStringLiteral("/cwEquateSpike-%1-XXXXXX")
              .arg(QCoreApplication::applicationPid()));
    REQUIRE(tempDir.isValid());

    const QString svxPath = QDir(tempDir.path()).filePath(QStringLiteral("spike.svx"));
    const QString threeDPath = QDir(tempDir.path()).filePath(QStringLiteral("spike.3d"));

    {
        QFile svxFile(svxPath);
        REQUIRE(svxFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&svxFile);
        out << driver;
    }

    SolveOutcome outcome;
    const auto result = cwCavernRunner::run(svxPath, threeDPath);
    if (result.hasError()) {
        outcome.solved = false;
        outcome.log = result.errorMessage();
        return outcome;
    }

    outcome.solved = true;
    outcome.log = result.value().logText;

    cwSurvex3DFileReader reader;
    outcome.lookup = reader.readStationPositions(threeDPath);
    return outcome;
}

//! Assert two fully-qualified station names resolved to the same coordinate.
void checkCoincident(const cwStationPositionLookup& lookup,
                     const QString& nameA, const QString& nameB)
{
    REQUIRE(lookup.hasPosition(nameA));
    REQUIRE(lookup.hasPosition(nameB));
    const QVector3D posA = lookup.position(nameA);
    const QVector3D posB = lookup.position(nameB);
    CHECK(posA.x() == Catch::Approx(posB.x()).margin(0.001));
    CHECK(posA.y() == Catch::Approx(posB.y()).margin(0.001));
    CHECK(posA.z() == Catch::Approx(posB.z()).margin(0.001));
}

} // namespace

TEST_CASE("Cross-scope equate solves at cave scope (no export, no infer)",
          "[EquateSpike]")
{
    const QUuid caveId = QUuid::createUuid();
    const QUuid trip1Id = QUuid::createUuid();
    const QUuid trip2Id = QUuid::createUuid();

    const QString caveName = cwCavernNaming::caveName(caveId);
    const QString trip1Name = cwCavernNaming::tripName(trip1Id);
    const QString trip2Name = cwCavernNaming::tripName(trip2Id);

    // trip1 is fixed and defines "a"; trip2 floats and defines "b". The
    // cave-relative *equate ties a==b, so the whole cave resolves and both
    // fully-qualified names must share trip1's coordinate for "a". No *export
    // and no *infer — a bare qualified equate is all cavern needs.
    const QString driver =
        QStringLiteral(
            "*begin %1\n"
            "  *begin %2\n"
            "    *fix fixpt 0 0 0\n"
            "    *data normal from to tape compass clino\n"
            "    fixpt a 10.0 0.0 0.0\n"
            "  *end %2\n"
            "  *begin %3\n"
            "    *data normal from to tape compass clino\n"
            "    b c 10.0 0.0 0.0\n"
            "  *end %3\n"
            "  *equate %2.a %3.b\n"
            "*end %1\n")
            .arg(caveName, trip1Name, trip2Name);

    const SolveOutcome outcome = solveDriver(driver);
    INFO("cavern log:\n" << outcome.log.toStdString());
    REQUIRE(outcome.solved);

    checkCoincident(outcome.lookup,
                    caveName + QLatin1Char('.') + trip1Name + QStringLiteral(".a"),
                    caveName + QLatin1Char('.') + trip2Name + QStringLiteral(".b"));
}

TEST_CASE("Cross-scope equate solves at region scope (no export, no infer)",
          "[EquateSpike]")
{
    const QUuid caveAId = QUuid::createUuid();
    const QUuid caveBId = QUuid::createUuid();

    const QString caveAName = cwCavernNaming::caveName(caveAId);
    const QString caveBName = cwCavernNaming::caveName(caveBId);

    // Two sibling caves inside the region's "*begin ;All the caves" wrapper.
    // caveA fixes "a"; caveB floats "b". The region-scope *equate uses fully
    // qualified cave_<hex>.<tail> operands — the form cwSurvexExporterRegion
    // would emit for a cross-cave tie.
    const QString driver =
        QStringLiteral(
            "*begin  ;All the caves\n"
            "  *begin %1\n"
            "    *fix fixpt 0 0 0\n"
            "    *data normal from to tape compass clino\n"
            "    fixpt a 10.0 0.0 0.0\n"
            "  *end %1\n"
            "  *begin %2\n"
            "    *data normal from to tape compass clino\n"
            "    b c 10.0 0.0 0.0\n"
            "  *end %2\n"
            "  *equate %1.a %2.b\n"
            "*end\n")
            .arg(caveAName, caveBName);

    const SolveOutcome outcome = solveDriver(driver);
    INFO("cavern log:\n" << outcome.log.toStdString());
    REQUIRE(outcome.solved);

    checkCoincident(outcome.lookup,
                    caveAName + QStringLiteral(".a"),
                    caveBName + QStringLiteral(".b"));
}

TEST_CASE("An N-way equate ties three cross-scope stations to one coordinate",
          "[EquateSpike]")
{
    // Import can produce a 3+-way join (the FIXME the model must fix). Prove a
    // single *equate with three qualified operands lands all three at one point,
    // so an N-way equate is one emitted line, not a chain of pairwise ones.
    const QUuid caveId = QUuid::createUuid();
    const QUuid t1 = QUuid::createUuid();
    const QUuid t2 = QUuid::createUuid();
    const QUuid t3 = QUuid::createUuid();

    const QString caveName = cwCavernNaming::caveName(caveId);
    const QString n1 = cwCavernNaming::tripName(t1);
    const QString n2 = cwCavernNaming::tripName(t2);
    const QString n3 = cwCavernNaming::tripName(t3);

    const QString driver =
        QStringLiteral(
            "*begin %1\n"
            "  *begin %2\n"
            "    *fix fixpt 0 0 0\n"
            "    *data normal from to tape compass clino\n"
            "    fixpt a 10.0 0.0 0.0\n"
            "  *end %2\n"
            "  *begin %3\n"
            "    *data normal from to tape compass clino\n"
            "    b c 10.0 0.0 0.0\n"
            "  *end %3\n"
            "  *begin %4\n"
            "    *data normal from to tape compass clino\n"
            "    d e 10.0 0.0 0.0\n"
            "  *end %4\n"
            "  *equate %2.a %3.b %4.d\n"
            "*end %1\n")
            .arg(caveName, n1, n2, n3);

    const SolveOutcome outcome = solveDriver(driver);
    INFO("cavern log:\n" << outcome.log.toStdString());
    REQUIRE(outcome.solved);

    const QString nameA = caveName + QLatin1Char('.') + n1 + QStringLiteral(".a");
    const QString nameB = caveName + QLatin1Char('.') + n2 + QStringLiteral(".b");
    const QString nameD = caveName + QLatin1Char('.') + n3 + QStringLiteral(".d");
    checkCoincident(outcome.lookup, nameA, nameB);
    checkCoincident(outcome.lookup, nameA, nameD);
}
