/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Commits 4 & 5 of plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html: cave-level
// and region-level *equate emission.
//
// Every equate lives in the region's one list (C4.3 of
// plans/SURVEY_TREE_PLAN.html) and renders at region scope, after every
// node's *begin block closes, with fully-qualified operands: the node's label
// path, then a Trip handle's scope prefix, then the tail. A tie inside one cave
// draws a native station and an externally-attached one coincident; a
// cross-cave tie does the same across the cave boundary.

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCavernNaming.h"
#include "cwScopeLabels.h"
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwLinePlotManager.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwStationHandle.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterRegion.h"
#include "cwSurveyExportManager.h"
#include "cwCavernRunner.h"
#include "cwSurvex3DFileReader.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>

// Std
#include <algorithm>

namespace {

// The cave export runs as a background task; this bounds the wait for its file.
constexpr qint64 kExportTimeoutMs = 10000;
constexpr int kExportPollMs = 10;

// A one-shot native trip (station `fromName` -> `toName`, level, along the
// given `compass` bearing). It carries no fix, so the exporter fallback-fixes
// its first station at the origin; the toName station is derived from the leg,
// so a cross-cave tie on it closes a loop rather than double-fixing a station.
cwTrip* addNativeTripWithShot(cwCave* cave,
                              const QString& name,
                              const QString& fromName,
                              const QString& toName,
                              double distance,
                              const QString& compass = QStringLiteral("0.0"))
{
    cwTrip* trip = addEmptyTrip(cave, name);
    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(distance)));
    shot.setCompass(cwCompassReading(compass));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    chunk->appendShot(cwStation(fromName), cwStation(toName), shot);
    return trip;
}

cwStationHandle nativeHandle(const cwCave* cave, const QString& tail)
{
    return cwStationHandle(cwStationHandle::NativeCave, cave->id(), tail);
}

cwStationHandle tripHandle(const cwTrip* trip, const QString& tail)
{
    return cwStationHandle(cwStationHandle::Trip, trip->id(), tail);
}

QString tripScopePrefix(const cwTrip* trip)
{
    return trip->scopePrefix();
}

//! The survey label this cave's *begin block carries, as the exporter assigns it
QString caveLabel(const cwCavingRegion& region, const cwCave* cave)
{
    return cwScopeLabels(region.data()).label(cave->id());
}

QString caveScopePrefix(const cwCavingRegion& region, const cwCave* cave)
{
    return cwScopeLabels(region.data()).prefix(cave->id());
}

// Runs the driver export the worker would run and returns the emitted .svx text.
QString driverTextFor(const cwCavingRegion& region,
                      const cwSurvexExporterRegion::Options& options,
                      const QString& outputPath)
{
    const cwCavingRegionData snapshot = region.data();
    REQUIRE(snapshot.caves.size() == 1);

    const auto result = cwSurvexExporterRegion::exportRegion(snapshot, outputPath, options);
    REQUIRE_FALSE(result.hasError());

    QFile file(outputPath);
    REQUIRE(file.open(QFile::ReadOnly));
    return QString::fromUtf8(file.readAll());
}

// The region driver the worker would run, whose region-scope operands are
// qualified with each cave's own label. Returns the emitted .svx text.
QString regionDriverText(const cwCavingRegion& region,
                         const cwSurvexExporterRegion::Options& options,
                         const QString& outputPath)
{
    const cwCavingRegionData snapshot = region.data();

    const auto result = cwSurvexExporterRegion::exportRegion(snapshot, outputPath, options);
    REQUIRE_FALSE(result.hasError());

    QFile file(outputPath);
    REQUIRE(file.open(QFile::ReadOnly));
    return QString::fromUtf8(file.readAll());
}

//! Every scope the driver opens, as a dotted label path from the region down:
//! "*begin a" then "*begin b" opens "a" and "a.b". An anonymous "*begin" (a
//! native trip, or the region itself) adds no naming level.
QSet<QString> openedScopes(const QString& driver)
{
    static const QRegularExpression beginLine(QStringLiteral("^\\*begin(?:\\s+([^\\s;]+))?"));
    static const QRegularExpression endLine(QStringLiteral("^\\*end\\b"));

    QSet<QString> scopes;
    QStringList stack; //one entry per open block, empty for an anonymous one
    for (const QString& rawLine : driver.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        const QRegularExpressionMatch begin = beginLine.match(line);
        if (begin.hasMatch()) {
            stack.append(begin.captured(1));
            QStringList path;
            for (const QString& label : std::as_const(stack)) {
                if (!label.isEmpty()) {
                    path.append(label);
                }
            }
            if (!begin.captured(1).isEmpty()) {
                scopes.insert(path.join(QLatin1Char('.')));
            }
        } else if (endLine.match(line).hasMatch() && !stack.isEmpty()) {
            stack.removeLast();
        }
    }
    return scopes;
}

//! Runs cavern on the driver at \a driverPath and reads back every station's
//! position, keyed by its full cavern name.
cwStationPositionLookup solveDriver(const QString& driverPath)
{
    const QString threeDPath = driverPath + QStringLiteral(".3d");
    const auto ran = cwCavernRunner::run(driverPath, threeDPath);
    INFO("cavern: " << ran.errorMessage().toStdString());
    REQUIRE_FALSE(ran.hasError());
    cwSurvex3DFileReader reader;
    return reader.readNetworkAndLookup(threeDPath).lookup;
}

void checkCoincident(const cwStationPositionLookup& lookup, const QString& first, const QString& second)
{
    INFO(first.toStdString() << " == " << second.toStdString());
    REQUIRE(lookup.hasPosition(first));
    REQUIRE(lookup.hasPosition(second));
    const QVector3D a = lookup.position(first);
    const QVector3D b = lookup.position(second);
    CHECK(a.x() == Catch::Approx(b.x()).margin(0.001));
    CHECK(a.y() == Catch::Approx(b.y()).margin(0.001));
    CHECK(a.z() == Catch::Approx(b.z()).margin(0.001));
}

} // namespace

TEST_CASE("A within-cave equate emits fully qualified after the cave block closes", "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    // seedAttachment returns the copied file path, not the dir; the
    // *include resolves against <attachDir>/<basename>, so register the
    // dir (see the coincidence case below for the same pattern).
    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("emit-attach"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(cave, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    // A cross-scope tie (native 1 == the external centerline's simple.a1)
    // and a within-native tie (1 == 2), so both operand renderings are
    // exercised in one driver.
    region.equates()->appendEquate(cwEquate({nativeHandle(cave, QStringLiteral("1")),
                                             tripHandle(attached, QStringLiteral("simple.a1"))}));
    region.equates()->appendEquate(cwEquate({nativeHandle(cave, QStringLiteral("1")),
                                             nativeHandle(cave, QStringLiteral("2"))}));

    cwSurvexExporterRegion::Options options;
    options.tripAttachmentDirs.insert(attached->id(), attachDir);

    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = driverTextFor(region, options, driverPath);

    const QString cavePrefix = caveScopePrefix(region, cave);
    const QString crossScopeLine = QStringLiteral("*equate ") + cavePrefix + QStringLiteral("1 ")
                                   + cavePrefix + tripScopePrefix(attached) + QStringLiteral("simple.a1");
    const QString nativeLine = QStringLiteral("*equate ") + cavePrefix + QStringLiteral("1 ")
                               + cavePrefix + QStringLiteral("2");
    INFO("driver:\n" << driver.toStdString());
    CHECK(driver.contains(crossScopeLine));
    CHECK(driver.contains(nativeLine));

    // The ties sit at region scope, after the cave's *end, where every
    // fully-qualified operand is in scope.
    const QString caveEnd = QStringLiteral("*end %1").arg(caveLabel(region, cave));
    const int equateIndex = driver.indexOf(QStringLiteral("*equate"));
    const int caveEndIndex = driver.indexOf(caveEnd);
    REQUIRE(equateIndex >= 0);
    REQUIRE(caveEndIndex >= 0);
    CHECK(caveEndIndex < equateIndex);
}

TEST_CASE("Structurally invalid or unresolvable equates emit nothing", "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(cave, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);

    // A Trip handle naming a trip that is in no node cannot be rendered, so
    // the whole equate is dropped rather than emitting a name cavern would
    // invent a station for.
    const QUuid strangerTripId = QUuid::createUuid();
    region.equates()->appendEquate(
        cwEquate({nativeHandle(cave, QStringLiteral("1")),
                  cwStationHandle(cwStationHandle::Trip, strangerTripId, QStringLiteral("x"))}));

    // A station tied only to itself is structurally invalid (one distinct
    // endpoint), so writeEquates drops it at the isValid() guard before any
    // operand is rendered.
    region.equates()->appendEquate(
        cwEquate({nativeHandle(cave, QStringLiteral("1")),
                  nativeHandle(cave, QStringLiteral("1"))}));

    cwSurvexExporterRegion::Options options;
    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = driverTextFor(region, options, driverPath);

    CHECK_FALSE(driver.contains(QStringLiteral("*equate")));
}

TEST_CASE("A within-cave equate draws a native and an external station coincident",
          "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    // seedAttachment copies the fixture into attachDir; the *include path is
    // <attachDir>/<basename>, so the attachment dir — not the copied file —
    // is what the worker resolves against.
    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("coincident-attach"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(cave, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    // Tie native "1" to the external centerline's simple.a1. Both survey
    // runs carry their own fix (the native cave via the exporter's
    // first-station fallback, the external file via its own *fix A1), so
    // the equate is what identifies the two stations — a broken operand
    // would make cavern report simple.a1 as undefined instead of solving.
    region.equates()->appendEquate(cwEquate({nativeHandle(cave, QStringLiteral("1")),
                                             tripHandle(attached, QStringLiteral("simple.a1"))}));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    INFO("solve error: " << manager.solveErrorMessage().toStdString());
    INFO("driver:\n" << manager.driverSource().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    const cwStationPositionLookup& lookup = cave->stationPositionLookup();
    const QString externalKey = tripScopePrefix(attached) + QStringLiteral("simple.a1");

    REQUIRE(lookup.hasPosition(QStringLiteral("1")));
    REQUIRE(lookup.hasPosition(externalKey));

    const QVector3D nativePos = lookup.position(QStringLiteral("1"));
    const QVector3D externalPos = lookup.position(externalKey);
    CHECK(nativePos.x() == Catch::Approx(externalPos.x()).margin(0.001));
    CHECK(nativePos.y() == Catch::Approx(externalPos.y()).margin(0.001));
    CHECK(nativePos.z() == Catch::Approx(externalPos.z()).margin(0.001));

    // The native run stayed intact through the tie: "2" is 10 m from "1".
    REQUIRE(lookup.hasPosition(QStringLiteral("2")));
    const QVector3D twoPos = lookup.position(QStringLiteral("2"));
    CHECK((twoPos - nativePos).length() == Catch::Approx(10.0).margin(0.01));
}

TEST_CASE("A single-cave export carries the ties that stay inside the cave",
          "[Equate][Emission]")
{
    cwCavingRegion region;
    cwCave* alpha = addEmptyCave(region, QStringLiteral("Alpha"));
    cwCave* bravo = addEmptyCave(region, QStringLiteral("Bravo"));
    addNativeTripWithShot(alpha, QStringLiteral("First"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    addNativeTripWithShot(bravo, QStringLiteral("Other"),
                          QStringLiteral("7"), QStringLiteral("8"), 10.0);

    region.equates()->appendEquate(cwEquate({nativeHandle(alpha, QStringLiteral("1")),
                                             nativeHandle(alpha, QStringLiteral("2"))}));
    region.equates()->appendEquate(cwEquate({nativeHandle(alpha, QStringLiteral("2")),
                                             nativeHandle(bravo, QStringLiteral("7"))}));

    cwSurvexExporterCaveTask exporter;
    exporter.setEquates(region.equates()->equates());
    QString text;
    QTextStream stream(&text);
    REQUIRE(exporter.writeCave(stream, alpha->data()));
    stream.flush();

    // The exported file stands alone, so the tie into Bravo names a scope it
    // never opens and is left out; the tie inside Alpha follows Alpha's block.
    const QString alphaLabel = cwScopeLabels::forNode(alpha->data()).label(alpha->id());
    const QString insideLine = QStringLiteral("*equate %1.1 %1.2").arg(alphaLabel);
    CHECK(text.count(QStringLiteral("*equate")) == 1);
    REQUIRE(text.contains(insideLine));
    CHECK(text.indexOf(QStringLiteral("*end ") + alphaLabel) < text.indexOf(insideLine));
}

TEST_CASE("The cave export menu takes the ties from the cave's own region",
          "[Equate][Emission]")
{
    cwCavingRegion region;
    cwCave* alpha = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(alpha, QStringLiteral("First"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    region.equates()->appendEquate(cwEquate({nativeHandle(alpha, QStringLiteral("1")),
                                             nativeHandle(alpha, QStringLiteral("2"))}));

    // The manager's region is a property of its own, which can be cleared
    // while the cave stays chosen.
    cwSurveyExportManager manager;
    manager.setCave(alpha);
    manager.setCavingRegion(nullptr);
    REQUIRE(manager.cave() == alpha);

    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());
    const QString outPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("alpha.svx"));
    manager.exportSurvexCave(outPath);

    const QString alphaLabel = cwScopeLabels::forNode(alpha->data()).label(alpha->id());
    const QString insideLine = QStringLiteral("*equate %1.1 %1.2").arg(alphaLabel);
    QElapsedTimer timer;
    timer.start();
    QString text;
    while (!text.contains(insideLine) && timer.elapsed() < kExportTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, kExportPollMs);
        QFile file(outPath);
        if (file.open(QIODevice::ReadOnly)) {
            text = QString::fromUtf8(file.readAll());
        }
    }
    CHECK(text.contains(insideLine));
}

TEST_CASE("A region equate emits a fully-qualified *equate at region scope",
          "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* caveA = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(caveA, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    cwCave* caveB = addEmptyCave(region, QStringLiteral("Beta"));
    addNativeTripWithShot(caveB, QStringLiteral("Native"),
                          QStringLiteral("x"), QStringLiteral("y"), 10.0);

    // A cross-cave tie: Alpha's native "2" == Beta's native "y". Neither cave
    // knows the other's namespace, so each operand must carry its own
    // cave-label qualifier for the region-scope *equate to resolve it.
    region.equates()->appendEquate(cwEquate({nativeHandle(caveA, QStringLiteral("2")),
                                             nativeHandle(caveB, QStringLiteral("y"))}));

    cwSurvexExporterRegion::Options options;
    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = regionDriverText(region, options, driverPath);

    const QString equateLine =
        QStringLiteral("*equate ")
        + caveScopePrefix(region, caveA) + QStringLiteral("2 ")
        + caveScopePrefix(region, caveB) + QStringLiteral("y");
    CHECK(driver.contains(equateLine));

    // The tie must sit at region scope: after BOTH cave blocks close (so both
    // qualified operands are declared) and before the region's outer *end.
    const QString caveAEnd = QStringLiteral("*end %1").arg(caveLabel(region, caveA));
    const QString caveBEnd = QStringLiteral("*end %1").arg(caveLabel(region, caveB));
    const int caveAEndIndex = driver.indexOf(caveAEnd);
    const int caveBEndIndex = driver.indexOf(caveBEnd);
    const int equateIndex = driver.indexOf(QStringLiteral("*equate"));
    const int outerEndIndex = driver.lastIndexOf(QStringLiteral("*end"));
    REQUIRE(caveAEndIndex >= 0);
    REQUIRE(caveBEndIndex >= 0);
    REQUIRE(equateIndex >= 0);
    CHECK(caveAEndIndex < equateIndex);
    CHECK(caveBEndIndex < equateIndex);
    CHECK(equateIndex < outerEndIndex);
}

TEST_CASE("A region equate draws stations in two caves coincident",
          "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* caveA = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(caveA, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    cwCave* caveB = addEmptyCave(region, QStringLiteral("Beta"));
    // Beta's leg runs east while Alpha's runs north, so before the tie the two
    // derived stations sit 10 m apart on different axes — the tie is what makes
    // them coincident, not an accident of both landing at the origin.
    addNativeTripWithShot(caveB, QStringLiteral("Native"),
                          QStringLiteral("x"), QStringLiteral("y"), 10.0,
                          QStringLiteral("90.0"));

    region.equates()->appendEquate(cwEquate({nativeHandle(caveA, QStringLiteral("2")),
                                             nativeHandle(caveB, QStringLiteral("y"))}));

    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();

    INFO("solve error: " << manager.solveErrorMessage().toStdString());
    INFO("driver:\n" << manager.driverSource().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    // The tie crosses the cave boundary: each qualified operand decodes back
    // into its own cave's lookup (splitLookupByNode), so the coincidence check
    // reads "2" from Alpha and "y" from Beta.
    const cwStationPositionLookup& lookupA = caveA->stationPositionLookup();
    const cwStationPositionLookup& lookupB = caveB->stationPositionLookup();
    REQUIRE(lookupA.hasPosition(QStringLiteral("2")));
    REQUIRE(lookupB.hasPosition(QStringLiteral("y")));

    const QVector3D posA = lookupA.position(QStringLiteral("2"));
    const QVector3D posB = lookupB.position(QStringLiteral("y"));
    CHECK(posA.x() == Catch::Approx(posB.x()).margin(0.001));
    CHECK(posA.y() == Catch::Approx(posB.y()).margin(0.001));
    CHECK(posA.z() == Catch::Approx(posB.z()).margin(0.001));
}

TEST_CASE("A region equate ties three caves' stations coincident in one line",
          "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwCave* caveA = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(caveA, QStringLiteral("Native"),
                          QStringLiteral("1"), QStringLiteral("2"), 10.0);
    cwCave* caveB = addEmptyCave(region, QStringLiteral("Beta"));
    addNativeTripWithShot(caveB, QStringLiteral("Native"),
                          QStringLiteral("x"), QStringLiteral("y"), 10.0,
                          QStringLiteral("90.0"));
    cwCave* caveC = addEmptyCave(region, QStringLiteral("Gamma"));
    addNativeTripWithShot(caveC, QStringLiteral("Native"),
                          QStringLiteral("p"), QStringLiteral("q"), 10.0,
                          QStringLiteral("180.0"));

    // One region equate ties THREE stations, one per cave. The emission loop
    // must render all three qualified operands onto a single *equate line and
    // cavern must merge all three to one point. The three legs run north, east,
    // and south, so pre-tie the derived ends sit at distinct positions — only
    // the 3-way tie collapses them. Guards the N>2 operand path the deleted
    // cross-scope spike used to cover end-to-end.
    region.equates()->appendEquate(cwEquate({nativeHandle(caveA, QStringLiteral("2")),
                                             nativeHandle(caveB, QStringLiteral("y")),
                                             nativeHandle(caveC, QStringLiteral("q"))}));

    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();

    INFO("solve error: " << manager.solveErrorMessage().toStdString());
    INFO("driver:\n" << manager.driverSource().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    // All three operands land on one line, in cave order — a dropped or
    // reordered operand would fail here even if the solve still converged.
    const QString equateLine =
        QStringLiteral("*equate ")
        + caveScopePrefix(region, caveA) + QStringLiteral("2 ")
        + caveScopePrefix(region, caveB) + QStringLiteral("y ")
        + caveScopePrefix(region, caveC) + QStringLiteral("q");
    CHECK(manager.driverSource().contains(equateLine));

    // Each qualified operand decodes back into its own cave's lookup, all three
    // coincident.
    const cwStationPositionLookup& lookupA = caveA->stationPositionLookup();
    const cwStationPositionLookup& lookupB = caveB->stationPositionLookup();
    const cwStationPositionLookup& lookupC = caveC->stationPositionLookup();
    REQUIRE(lookupA.hasPosition(QStringLiteral("2")));
    REQUIRE(lookupB.hasPosition(QStringLiteral("y")));
    REQUIRE(lookupC.hasPosition(QStringLiteral("q")));

    const QVector3D posA = lookupA.position(QStringLiteral("2"));
    const QVector3D posB = lookupB.position(QStringLiteral("y"));
    const QVector3D posC = lookupC.position(QStringLiteral("q"));
    CHECK(posA.x() == Catch::Approx(posB.x()).margin(0.001));
    CHECK(posA.y() == Catch::Approx(posB.y()).margin(0.001));
    CHECK(posA.z() == Catch::Approx(posB.z()).margin(0.001));
    CHECK(posA.x() == Catch::Approx(posC.x()).margin(0.001));
    CHECK(posA.y() == Catch::Approx(posC.y()).margin(0.001));
    CHECK(posA.z() == Catch::Approx(posC.z()).margin(0.001));
}

TEST_CASE("A region equate qualifies native-node and trip handles at any depth",
          "[Equate][Emission]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());
    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("dome-attach"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    // Kentucky field seasons (Folder) > Side Cave > Upper level (Section), and a
    // top-level Fisher Ridge. Each operand has to carry every label from the top
    // down: a region-scope *equate resolves names from the region itself.
    cwCavingRegion region;
    cwCave* fisherRidge = addEmptyCave(region, QStringLiteral("Fisher Ridge"));
    addNativeTripWithShot(fisherRidge, QStringLiteral("Entrance"),
                          QStringLiteral("f1"), QStringLiteral("f2"), 10.0);
    cwCave* folder = addChildNode(region.rootNode(), QStringLiteral("Kentucky field seasons"),
                                  cwSurveyNode::Kind::Folder);
    cwCave* sideCave = addChildNode(folder, QStringLiteral("Side Cave"), cwSurveyNode::Kind::Cave);
    addNativeTripWithShot(sideCave, QStringLiteral("Sump dig"),
                          QStringLiteral("s1"), QStringLiteral("s2"), 10.0, QStringLiteral("90.0"));
    cwCave* section = addChildNode(sideCave, QStringLiteral("Upper level"),
                                   cwSurveyNode::Kind::Folder);
    addNativeTripWithShot(section, QStringLiteral("Upper survey"),
                          QStringLiteral("u1"), QStringLiteral("u2"), 10.0, QStringLiteral("180.0"));
    cwTrip* domeClimb = addAttachedTrip(section, QStringLiteral("Dome climb"));

    region.equates()->appendEquate(cwEquate({nativeHandle(section, QStringLiteral("u1")),
                                             nativeHandle(fisherRidge, QStringLiteral("f2"))}));
    region.equates()->appendEquate(cwEquate({tripHandle(domeClimb, QStringLiteral("simple.a1")),
                                             nativeHandle(sideCave, QStringLiteral("s2"))}));

    cwSurvexExporterRegion::Options options;
    options.tripAttachmentDirs.insert(domeClimb->id(), attachDir);
    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = regionDriverText(region, options, driverPath);
    INFO("driver:\n" << driver.toStdString());

    const QString nativeNodeLine = QStringLiteral(
        "*equate kentucky_field_seasons.side_cave.upper_level.u1 fisher_ridge.f2");
    const QString tripLine = QStringLiteral(
        "*equate kentucky_field_seasons.side_cave.upper_level.dome_climb.simple.a1"
        " kentucky_field_seasons.side_cave.s2");
    CHECK(driver.contains(nativeNodeLine));
    CHECK(driver.contains(tripLine));

    // Both sit at region scope, after the deepest block they name has closed.
    const int folderEnd = driver.indexOf(QStringLiteral("*end kentucky_field_seasons"));
    REQUIRE(folderEnd >= 0);
    CHECK(folderEnd < driver.indexOf(nativeNodeLine));
    CHECK(folderEnd < driver.indexOf(tripLine));

    const cwStationPositionLookup solved = solveDriver(driverPath);
    checkCoincident(solved, QStringLiteral("kentucky_field_seasons.side_cave.upper_level.u1"),
                    QStringLiteral("fisher_ridge.f2"));
    checkCoincident(solved,
                    QStringLiteral("kentucky_field_seasons.side_cave.upper_level.dome_climb.simple.a1"),
                    QStringLiteral("kentucky_field_seasons.side_cave.s2"));
}

TEST_CASE("Every region equate operand names a scope the driver opens",
          "[Equate][Emission]")
{
    // The region equate path once qualified its operands with one scheme while
    // the "*begin" lines used another, so the operands named scopes the file
    // never opened — and cavern does not reject such a name, it creates the
    // station. Colliding sanitized names and a trip-scope handle are where two
    // derivations of a label would part ways.
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    cwSurvexExporterRegion::Options options;
    QList<cwTrip*> attachedTrips;
    for (const QString& caveName : {QStringLiteral("Big Cave"), QStringLiteral("Big-Cave")}) {
        cwCave* cave = addEmptyCave(region, caveName);
        addNativeTripWithShot(cave, QStringLiteral("Native"),
                              QStringLiteral("1"), QStringLiteral("2"), 10.0);
        cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Topo 1"));
        const QString attachDir = tempSubdir(tempRoot, cwCavernNaming::sanitizeToCavernIdentifier(caveName)
                                                           + QStringLiteral("_2"));
        seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));
        options.tripAttachmentDirs.insert(attached->id(), attachDir);
        attachedTrips.append(attached);
    }
    cwCave* bigCave = region.cave(0);
    cwCave* bigCaveTwin = region.cave(1);

    region.equates()->appendEquate(cwEquate({tripHandle(attachedTrips.at(1), QStringLiteral("simple.a2")),
                                             nativeHandle(bigCave, QStringLiteral("2"))}));
    region.equates()->appendEquate(cwEquate({tripHandle(attachedTrips.at(0), QStringLiteral("simple.a3")),
                                             nativeHandle(bigCaveTwin, QStringLiteral("2"))}));

    const QString driverPath = QDir(tempRoot.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    const QString driver = regionDriverText(region, options, driverPath);
    INFO("driver:\n" << driver.toStdString());

    const QSet<QString> scopes = openedScopes(driver);
    CHECK(scopes.contains(QStringLiteral("big_cave.topo_1")));
    CHECK(scopes.contains(QStringLiteral("big_cave_2.topo_1")));

    CHECK(driver.contains(QStringLiteral("*equate big_cave_2.topo_1.simple.a2 big_cave.2")));
    CHECK(driver.contains(QStringLiteral("*equate big_cave.topo_1.simple.a3 big_cave_2.2")));

    // Every operand on every *equate line sits inside a scope the driver opens.
    const QString equateKeyword = QStringLiteral("*equate");
    int equateLineCount = 0;
    for (const QString& rawLine : driver.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (!line.startsWith(equateKeyword)) {
            continue;
        }
        ++equateLineCount;
        const QStringList operands = line.mid(equateKeyword.size()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString& operand : operands) {
            INFO("operand: " << operand.toStdString());
            const bool insideOpenedScope = std::any_of(scopes.cbegin(), scopes.cend(),
                                                       [&operand](const QString& scope) {
                                                           return operand.startsWith(scope + QLatin1Char('.'));
                                                       });
            CHECK(insideOpenedScope);
        }
    }
    CHECK(equateLineCount == 2);

    const cwStationPositionLookup solved = solveDriver(driverPath);
    checkCoincident(solved, QStringLiteral("big_cave_2.topo_1.simple.a2"), QStringLiteral("big_cave.2"));
    checkCoincident(solved, QStringLiteral("big_cave.topo_1.simple.a3"), QStringLiteral("big_cave_2.2"));
}
