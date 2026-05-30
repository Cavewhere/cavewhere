/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch includes
#include <catch2/catch_test_macros.hpp>

// Cavewhere includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurveyExportManager.h"
#include "cwTrip.h"
#include "cwSignalSpy.h"

// Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>

namespace {

// Seed every cave/trip with a single non-degenerate shot so the underlying
// exporter has something to write when the gate happens to let it through —
// otherwise a write-failure could masquerade as a successful gate block.
cwCave* addCaveWithOneShot(cwCavingRegion& region, const QString& caveName)
{
    cwCave* cave = new cwCave();
    cave->setName(caveName);
    region.addCave(cave);

    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Trip"));
    cave->addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);

    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("10.0")));
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    chunk->appendShot(cwStation(QStringLiteral("A1")),
                      cwStation(QStringLiteral("A2")),
                      shot);

    return cave;
}

QString uniqueExportPath(const QTemporaryDir& dir, const QString& stem)
{
    return dir.filePath(QStringLiteral("%1_%2.svx")
                            .arg(stem)
                            .arg(QCoreApplication::applicationPid()));
}

} // namespace

TEST_CASE("Pure-Native region exports normally: canExport true and reason empty",
          "[Export][Disable]")
{
    cwCavingRegion region;
    addCaveWithOneShot(region, QStringLiteral("Native"));

    cwSurveyExportManager manager;
    manager.setCavingRegion(&region);

    CHECK(manager.canExport() == true);
    CHECK(manager.exportDisabledReason().isEmpty());
}

TEST_CASE("Region with one Attached trip refuses export and reports a reason",
          "[Export][Disable]")
{
    cwCavingRegion region;
    cwCave* cave = addCaveWithOneShot(region, QStringLiteral("WithAttachedTrip"));

    cwSurveyExportManager manager;
    manager.setCavingRegion(&region);
    REQUIRE(manager.canExport() == true);

    cwTrip* trip = cave->trips().first();
    trip->setExternalCenterline(cwExternalCenterline(QStringLiteral("trip.svx")));

    CHECK(manager.canExport() == false);
    CHECK_FALSE(manager.exportDisabledReason().isEmpty());
    CHECK(manager.exportDisabledReason().contains(QStringLiteral("external"),
                                                  Qt::CaseInsensitive));
}

TEST_CASE("Region with one Attached cave refuses export and reports a reason",
          "[Export][Disable]")
{
    cwCavingRegion region;
    cwCave* cave = addCaveWithOneShot(region, QStringLiteral("WithAttachedCave"));

    cwSurveyExportManager manager;
    manager.setCavingRegion(&region);
    REQUIRE(manager.canExport() == true);

    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("cave.svx")));

    CHECK(manager.canExport() == false);
    CHECK_FALSE(manager.exportDisabledReason().isEmpty());
    CHECK(manager.exportDisabledReason().contains(QStringLiteral("external"),
                                                  Qt::CaseInsensitive));
}

TEST_CASE("Programmatic export call early-returns when canExport is false",
          "[Export][Disable]")
{
    // The QML menu binds enabled to canExport, but a programmatic caller
    // (script / test / future automation) can still reach the export slot
    // directly. The internal guard must refuse — no .svx file should land
    // on disk, even though the cave has valid centerline shots to export.
    cwCavingRegion region;
    cwCave* cave = addCaveWithOneShot(region, QStringLiteral("Blocked"));
    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("cave.svx")));

    cwSurveyExportManager manager;
    manager.setCavingRegion(&region);
    manager.setCave(cave);
    REQUIRE(manager.canExport() == false);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString outPath = uniqueExportPath(tempDir, QStringLiteral("blocked_cave"));

    manager.exportSurvexCave(outPath);

    // The export uses cwSurvexExporterCaveTask asynchronously when allowed
    // through, so a real export would create the file synchronously inside
    // exporterFinished after task launch. The gate short-circuits before
    // either path runs - if the file appears, the guard is missing.
    CHECK_FALSE(QFileInfo::exists(outPath));
}

TEST_CASE("canExportChanged fires on attach and again on detach",
          "[Export][Disable]")
{
    cwCavingRegion region;
    cwCave* cave = addCaveWithOneShot(region, QStringLiteral("Toggling"));

    cwSurveyExportManager manager;
    manager.setCavingRegion(&region);
    REQUIRE(manager.canExport() == true);

    cwSignalSpy gateSpy(&manager, &cwSurveyExportManager::canExportChanged);

    // Attach an entry: gate flips true -> false, one emission.
    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("cave.svx")));
    CHECK(gateSpy.size() == 1);
    CHECK(manager.canExport() == false);

    // Detach the entry: gate flips false -> true, another emission.
    cave->setExternalCenterline(cwExternalCenterline());
    CHECK(gateSpy.size() == 2);
    CHECK(manager.canExport() == true);

    // No-op set (assigning the same value) must not emit because the
    // gate state didn't flip - prevents UI thrash on benign re-saves.
    cave->setExternalCenterline(cwExternalCenterline());
    CHECK(gateSpy.size() == 2);
}

TEST_CASE("Attaching a trip added after region setup is still tracked",
          "[Export][Disable]")
{
    // Verifies the rewire-on-insertedTrips path: the manager must catch
    // attachments on trips that didn't exist when setCavingRegion ran.
    cwCavingRegion region;
    cwCave* cave = addCaveWithOneShot(region, QStringLiteral("GrowingCave"));

    cwSurveyExportManager manager;
    manager.setCavingRegion(&region);
    REQUIRE(manager.canExport() == true);

    cwTrip* lateTrip = new cwTrip();
    lateTrip->setName(QStringLiteral("Late"));
    cave->addTrip(lateTrip);
    REQUIRE(manager.canExport() == true);

    lateTrip->setExternalCenterline(cwExternalCenterline(QStringLiteral("late.svx")));
    CHECK(manager.canExport() == false);
}

TEST_CASE("Attaching a cave added after region setup is still tracked",
          "[Export][Disable]")
{
    // Verifies the rewire-on-insertedCaves path symmetrically with the trip
    // case above.
    cwCavingRegion region;
    addCaveWithOneShot(region, QStringLiteral("Original"));

    cwSurveyExportManager manager;
    manager.setCavingRegion(&region);
    REQUIRE(manager.canExport() == true);

    cwCave* lateCave = addCaveWithOneShot(region, QStringLiteral("LateCave"));
    REQUIRE(manager.canExport() == true);

    lateCave->setExternalCenterline(cwExternalCenterline(QStringLiteral("late.svx")));
    CHECK(manager.canExport() == false);
}
