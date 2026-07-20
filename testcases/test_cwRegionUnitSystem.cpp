/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwUnitSettings.h"
#include "cwUnits.h"

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

namespace {

// Saves the root's project into a fresh .cwproj under tempDir and returns the
// actual on-disk path (saveAs moves the file into a sibling directory).
QString saveProject(cwRootData* root, const QTemporaryDir& tempDir, const QString& base)
{
    const QString projectPath = QDir(tempDir.path())
        .filePath(QStringLiteral("%1-%2.cwproj").arg(base).arg(QCoreApplication::applicationPid()));
    REQUIRE(root->project()->saveAs(projectPath));
    root->project()->waitSaveToFinish();
    return root->project()->filename();
}

} // namespace

TEST_CASE("A new project seeds its region unitSystem from the app default",
          "[RegionUnitSystem]")
{
    // The test harness namespaces QSettings per process (application name carries
    // the pid), so mutating the app default here can't leak to a concurrent test
    // process. Restore it afterward to keep it from leaking to sibling cases.
    auto* appDefault = cwUnitSettings::instance();
    REQUIRE(appDefault != nullptr);
    const cwUnits::UnitSystem original = appDefault->unitSystem();

    // A project created while the app default is Imperial seeds its region
    // Imperial; this fails if cwProject stops seeding (region would default Metric).
    appDefault->setUnitSystem(cwUnits::Imperial);
    auto root = std::make_unique<cwRootData>();
    CHECK(root->project()->cavingRegion()->unitSystem() == cwUnits::Imperial);

    // newProject() re-seeds from the current app default.
    appDefault->setUnitSystem(cwUnits::Metric);
    root->project()->newProject();
    CHECK(root->project()->cavingRegion()->unitSystem() == cwUnits::Metric);

    appDefault->setUnitSystem(original);
}

TEST_CASE("cwCavingRegion unitSystem survives save/load", "[RegionUnitSystem]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    region->setUnitSystem(cwUnits::Imperial);

    const QString actualPath = saveProject(root.get(), tempDir, QStringLiteral("unit-rt"));

    auto reloaded = std::make_unique<cwRootData>();
    reloaded->project()->loadFile(actualPath);
    reloaded->project()->waitLoadToFinish();

    CHECK(reloaded->project()->cavingRegion()->unitSystem() == cwUnits::Imperial);
}

TEST_CASE("Changing the region unitSystem persists without an explicit save",
          "[RegionUnitSystem]")
{
    // Regression: unitSystem lives in the project metadata file, so a change made
    // through the UI (the settings/project combobox) must reach disk on its own —
    // the metadata save handler has to watch unitSystemChanged, exactly as it does
    // globalCoordinateSystemChanged. Without that wiring the edit is silently lost
    // on close.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    region->setUnitSystem(cwUnits::Metric);

    // Establish the project on disk in Metric.
    const QString actualPath = saveProject(root.get(), tempDir, QStringLiteral("unit-incremental"));

    // Flip the default the way the combobox does — no second saveAs. This alone
    // must be persisted by the metadata save handler.
    region->setUnitSystem(cwUnits::Imperial);
    root->project()->waitSaveToFinish();

    auto reloaded = std::make_unique<cwRootData>();
    reloaded->project()->loadFile(actualPath);
    reloaded->project()->waitLoadToFinish();

    CHECK(reloaded->project()->cavingRegion()->unitSystem() == cwUnits::Imperial);
}

TEST_CASE("A Metric project loads as Metric even when the app default is Imperial",
          "[RegionUnitSystem]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto* appDefault = cwUnitSettings::instance();
    REQUIRE(appDefault != nullptr);
    const cwUnits::UnitSystem original = appDefault->unitSystem();

    auto root = std::make_unique<cwRootData>();
    root->project()->cavingRegion()->setUnitSystem(cwUnits::Metric);
    const QString actualPath = saveProject(root.get(), tempDir, QStringLiteral("unit-rt-metric"));

    // Bias the reload's app default to Imperial so a dropped Metric (the zero enum
    // value proto3 could omit) would surface as Imperial from the constructor seed.
    // A correct save/load overrides that seed with the stored Metric value.
    appDefault->setUnitSystem(cwUnits::Imperial);

    auto reloaded = std::make_unique<cwRootData>();
    reloaded->project()->loadFile(actualPath);
    reloaded->project()->waitLoadToFinish();

    CHECK(reloaded->project()->cavingRegion()->unitSystem() == cwUnits::Metric);

    appDefault->setUnitSystem(original);
}

TEST_CASE("A new trip is seeded with the region's survey-entry unit", "[RegionUnitSystem]")
{
    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    region->setUnitSystem(cwUnits::Imperial);

    region->addCave();
    cwCave* cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    REQUIRE(trip != nullptr);

    CHECK(trip->calibrations()->distanceUnit() == cwUnits::Feet);
}

TEST_CASE("Changing the region system leaves existing trips untouched", "[RegionUnitSystem]")
{
    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    // Region starts Metric; the first trip is entered in metres.
    region->setUnitSystem(cwUnits::Metric);
    region->addCave();
    cwCave* cave = region->cave(0);
    cave->addTrip();
    cwTrip* existingTrip = cave->trip(0);
    REQUIRE(existingTrip->calibrations()->distanceUnit() == cwUnits::Meters);

    // Flipping the project default must not retroactively rewrite that trip.
    region->setUnitSystem(cwUnits::Imperial);
    CHECK(existingTrip->calibrations()->distanceUnit() == cwUnits::Meters);

    // A trip added after the change is seeded with the new unit.
    cave->addTrip();
    cwTrip* newTrip = cave->trip(1);
    CHECK(newTrip->calibrations()->distanceUnit() == cwUnits::Feet);
}
