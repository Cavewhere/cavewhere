/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "LoadProjectHelper.h"
#include "TestHelper.h"
#include "cwProject.h"
#include "cwCavingRegion.h"
#include "cwCavingRegionData.h"
#include "cwCave.h"
#include "cwCaveData.h"
#include "cwTrip.h"
#include "cwTripData.h"
#include "cwSurveyChunk.h"
#include "cwSurveyChunkData.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwLinePlotManager.h"
#include "cwStationPositionLookup.h"
#include "Account.h"

//Qt includes
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QSet>

//Std includes
#include <cmath>

// Generates a huge synthetic cave for profiling station label rendering
// (issue 714). Hidden from the default run because it is slow. Run with:
//   CAVEWHERE_LARGE_CAVE_OUTPUT=/abs/path/largeCave.cw cavewhere-test "[LargeCave]"

namespace {

constexpr int kTripCount = 1000;
constexpr int kStationsPerTrip = 50;
constexpr int kShotsPerTrip = kStationsPerTrip - 1;
constexpr double kTripLength = 200.0; // meters
constexpr double kShotLength = kTripLength / kShotsPerTrip;
constexpr double kShotLengthJitter = 0.5;
constexpr double kMaxInclination = 20.0;
constexpr double kFullCircle = 360.0;
constexpr double kMaxBearingWander = 30.0;
constexpr double kMinLrud = 0.5;
constexpr double kMaxLrud = 2.0;
constexpr quint32 kSeed = 714;
constexpr double kMinSolvedFraction = 0.95;

double uniform(QRandomGenerator& random, double min, double max)
{
    return min + random.generateDouble() * (max - min);
}

QString stationName(int tripIndex, int stationIndex)
{
    return QStringLiteral("T%1S%2").arg(tripIndex).arg(stationIndex);
}

cwStation makeStation(QRandomGenerator& random, const QString& name)
{
    cwStation station(name);
    station.setLeft(cwDistanceReading(uniform(random, kMinLrud, kMaxLrud)));
    station.setRight(cwDistanceReading(uniform(random, kMinLrud, kMaxLrud)));
    station.setUp(cwDistanceReading(uniform(random, kMinLrud, kMaxLrud)));
    station.setDown(cwDistanceReading(uniform(random, kMinLrud, kMaxLrud)));
    return station;
}

// Each trip is a simple chain that branches off a random station of a
// random earlier trip, so the whole cave is one loop-free tree.
cwCavingRegionData generateLargeCave()
{
    QRandomGenerator random(kSeed);

    cwCaveData cave;
    cave.name = QStringLiteral("Large Cave");
    cave.id = QUuid::createUuid();
    cave.trips.reserve(kTripCount);

    const QDateTime tripDate(QDate(2026, 1, 1), QTime(12, 0));

    for (int tripIndex = 0; tripIndex < kTripCount; ++tripIndex) {
        QString firstStationName;
        if (tripIndex == 0) {
            firstStationName = stationName(0, 0);
        } else {
            const int parentTrip = random.bounded(tripIndex);
            // Index 0 of a later trip is a borrowed name, so pick from the stations it owns.
            const int parentStation = random.bounded(1, kStationsPerTrip);
            firstStationName = stationName(parentTrip, parentStation);
        }

        cwSurveyChunkData chunk;
        chunk.id = QUuid::createUuid();
        chunk.stations.reserve(kStationsPerTrip);
        chunk.shots.reserve(kShotsPerTrip);
        chunk.stations.append(makeStation(random, firstStationName));

        const double baseBearing = uniform(random, 0.0, kFullCircle);
        for (int stationIndex = 1; stationIndex < kStationsPerTrip; ++stationIndex) {
            chunk.stations.append(makeStation(random, stationName(tripIndex, stationIndex)));

            double bearing = baseBearing + uniform(random, -kMaxBearingWander, kMaxBearingWander);
            bearing = std::fmod(bearing + kFullCircle, kFullCircle);

            cwShot shot;
            shot.setDistance(cwDistanceReading(kShotLength + uniform(random, -kShotLengthJitter, kShotLengthJitter)));
            shot.setCompass(cwCompassReading(bearing));
            shot.setClino(cwClinoReading(uniform(random, -kMaxInclination, kMaxInclination)));
            chunk.shots.append(shot);
        }

        cwTripData trip;
        trip.name = QStringLiteral("Trip %1").arg(tripIndex, 4, 10, QLatin1Char('0'));
        trip.id = QUuid::createUuid();
        trip.date = tripDate;
        trip.chunks.append(chunk);
        cave.trips.append(trip);
    }

    cwCavingRegionData region;
    region.name = QStringLiteral("Large Cave Region");
    region.caves.append(cave);
    return region;
}

QSet<QString> uniqueStationNames(const cwCave* cave)
{
    QSet<QString> names;
    for (const cwStation& station : cave->stations()) {
        names.insert(station.name().toLower());
    }
    return names;
}

} // namespace

TEST_CASE("Generate a large synthetic cave for label rendering profiling", "[.LargeCave]")
{
    QElapsedTimer stepTimer;
    QElapsedTimer totalTimer;
    totalTimer.start();

    QQuickGit::Account account;
    account.setName(QStringLiteral("Large Cave Generator"));
    account.setEmail(QStringLiteral("large.cave@example.com"));

    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    project->setGitAccount(&account);

    stepTimer.start();
    project->cavingRegion()->setData(generateLargeCave());
    const qint64 generateMs = stepTimer.elapsed();

    cwCavingRegion* region = project->cavingRegion();
    REQUIRE(region->caveCount() == 1);
    cwCave* cave = region->cave(0);
    REQUIRE(cave->tripCount() == kTripCount);

    int totalStations = 0;
    for (const cwTrip* trip : cave->trips()) {
        REQUIRE(trip->chunks().size() == 1);
        const cwSurveyChunk* chunk = trip->chunks().at(0);
        CHECK(chunk->stationCount() == kStationsPerTrip);
        CHECK(chunk->shotCount() == chunk->stationCount() - 1);
        totalStations += chunk->stationCount();
    }
    CHECK(totalStations == kTripCount * kStationsPerTrip);

    // Each trip after the first shares its first station with an earlier trip.
    const QSet<QString> stationNames = uniqueStationNames(cave);
    const int expectedUniqueStations = kTripCount * kShotsPerTrip + 1;
    CHECK(stationNames.size() == expectedUniqueStations);

    stepTimer.restart();
    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(region);
    plotManager->waitToFinish();
    const qint64 solveMs = stepTimer.elapsed();

    const cwStationPositionLookup lookup = cave->stationPositionLookup();
    int solvedCount = 0;
    for (const QString& name : stationNames) {
        if (lookup.hasPosition(name)) {
            ++solvedCount;
        }
    }
    INFO("Solved " << solvedCount << " of " << stationNames.size() << " stations");
    CHECK(solvedCount >= static_cast<int>(stationNames.size() * kMinSolvedFraction));

    // Detach the solver before saving so it doesn't re-run on save side effects.
    plotManager.reset();

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    QString outputPath = qEnvironmentVariable("CAVEWHERE_LARGE_CAVE_OUTPUT");
    if (outputPath.isEmpty()) {
        outputPath = tempDir.filePath(QStringLiteral("largeCave-%1.cw").arg(QCoreApplication::applicationPid()));
    } else if (QFileInfo::exists(outputPath)) {
        REQUIRE(QFile::remove(outputPath));
    }

    stepTimer.restart();
    REQUIRE(project->saveAs(outputPath));
    project->waitSaveToFinish();
    const qint64 saveMs = stepTimer.elapsed();

    REQUIRE(project->fileType() == cwProject::BundledGitFileType);
    REQUIRE(QFileInfo::exists(outputPath));

    stepTimer.restart();
    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(outputPath);
    reloaded->waitLoadToFinish();
    const qint64 loadMs = stepTimer.elapsed();

    REQUIRE(reloaded->cavingRegion()->caveCount() == 1);
    const cwCave* reloadedCave = reloaded->cavingRegion()->cave(0);
    CHECK(reloadedCave->tripCount() == kTripCount);
    CHECK(reloadedCave->stations().size() == totalStations);

    WARN("Large cave written to " << outputPath.toStdString()
         << " (" << QFileInfo(outputPath).size() << " bytes)"
         << "\n  stations: " << totalStations << " (" << stationNames.size() << " unique, "
         << solvedCount << " solved)"
         << "\n  generate: " << generateMs << " ms"
         << "\n  solve: " << solveMs << " ms"
         << "\n  save: " << saveMs << " ms"
         << "\n  reload: " << loadMs << " ms"
         << "\n  total: " << totalTimer.elapsed() << " ms");
}
