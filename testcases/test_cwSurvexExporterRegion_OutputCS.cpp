/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch2 includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwLinePlotManager.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwSurvexExporterRegion.h"
#include "cwTrip.h"

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

//Std includes
#include <memory>

namespace {

const QString kUtmZone13N = QStringLiteral("EPSG:32613");
const QString& kWgs84 = cwCoordinateTransform::Wgs84;

//! A point in Boulder, Colorado — UTM zone 13 north.
constexpr double kBoulderLongitude = -105.27;
constexpr double kBoulderLatitude = 40.01;
constexpr double kBoulderEasting = 478000.0;
constexpr double kBoulderNorthing = 4430000.0;
constexpr double kBoulderElevation = 1655.0;

std::unique_ptr<cwCavingRegion> makeRegion(const QString& globalCS)
{
    auto region = std::make_unique<cwCavingRegion>();
    region->geoReference()->setGlobalCoordinateSystem(globalCS);

    auto* cave = new cwCave();
    cave->setName(QStringLiteral("TestCave"));
    region->addCave(cave);

    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setName(QStringLiteral("TestTrip"));

    auto* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendNewShot();
    chunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("a1"));
    chunk->setData(cwSurveyChunk::StationNameRole, 1, QStringLiteral("a2"));
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, QStringLiteral("10"));
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("0"));
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, QStringLiteral("0"));

    return region;
}

void appendFix(cwCavingRegion* region,
               const QString& stationName,
               const QString& inputCS,
               double easting,
               double northing,
               double elevation)
{
    cwFixStation fix;
    fix.setStationName(stationName);
    fix.setInputCS(inputCS);
    fix.setCoordinate(easting, northing, elevation);
    region->cave(0)->fixStations()->appendFixStation(fix);
}

//! Append a fix that carries a coordinate system but no coordinate — the shape
//! "Mark Station as Fixed" leaves behind before anything is typed.
void appendEmptyFix(cwCavingRegion* region,
                    const QString& stationName,
                    const QString& inputCS)
{
    cwFixStation fix;
    fix.setStationName(stationName);
    fix.setInputCS(inputCS);
    region->cave(0)->fixStations()->appendFixStation(fix);
}

//! What the exported file names as *cs out, or an empty string when it names
//! nothing.
QString exportedOutputCS(const cwCavingRegion* region,
                         const cwSurvexExporterRegion::Options& options)
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("region-%1.svx")
                                          .arg(QCoreApplication::applicationPid()));

    const auto result = cwSurvexExporterRegion::exportRegion(region->data(), path, options);
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());

    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));

    const QString prefix = QStringLiteral("*cs out ");
    for (const QString& line : lines) {
        if (line.startsWith(prefix)) {
            return line.mid(prefix.size()).trimmed();
        }
    }
    return QString();
}

//! The exported *cs out under an explicitly named policy.
QString exportedOutputCS(const cwCavingRegion* region,
                         cwSurvexExporterRegion::OutputCSPolicy policy)
{
    cwSurvexExporterRegion::Options options;
    options.outputCSPolicy = policy;
    return exportedOutputCS(region, options);
}

} // namespace

TEST_CASE("cwSurvexExporterRegion resolves *cs out per policy",
          "[cwSurvexExporterRegion_OutputCS]")
{
    using Policy = cwSurvexExporterRegion::OutputCSPolicy;

    SECTION("the project's own coordinate system outranks a derived one") {
        auto region = makeRegion(kUtmZone13N);
        appendFix(region.get(), QStringLiteral("a1"), kWgs84,
                  kBoulderLongitude, kBoulderLatitude, kBoulderElevation);

        CHECK(exportedOutputCS(region.get(), Policy::Shareable) == kUtmZone13N);
        CHECK(exportedOutputCS(region.get(), Policy::WorkingFrame) == kUtmZone13N);
    }

    SECTION("a projected fix is shareable as it stands") {
        auto region = makeRegion(QString());
        appendFix(region.get(), QStringLiteral("a1"), kUtmZone13N,
                  kBoulderEasting, kBoulderNorthing, kBoulderElevation);

        CHECK(exportedOutputCS(region.get(), Policy::Shareable) == kUtmZone13N);
        CHECK(exportedOutputCS(region.get(), Policy::WorkingFrame) == kUtmZone13N);
    }

    // The whole point of the split: a geographic fix is no output system at all
    // to the solve — cavern refuses one — but the UTM zone containing it is a
    // perfectly good system to hand a reader.
    SECTION("a geographic fix yields its UTM zone to a shared file and nothing to the solve") {
        auto region = makeRegion(QString());
        appendFix(region.get(), QStringLiteral("a1"), kWgs84,
                  kBoulderLongitude, kBoulderLatitude, kBoulderElevation);

        CHECK(exportedOutputCS(region.get(), Policy::Shareable) == kUtmZone13N);
        CHECK(exportedOutputCS(region.get(), Policy::WorkingFrame).isEmpty());
    }

    SECTION("a fix that places nothing yet doesn't decide, so a later one does") {
        auto region = makeRegion(QString());
        appendEmptyFix(region.get(), QStringLiteral("a1"), kWgs84);
        // A coordinate still at the origin is "not entered yet" too — off the
        // west coast of Africa is not where anyone's cave is.
        appendFix(region.get(), QStringLiteral("a2"), kWgs84, 0.0, 0.0, 0.0);
        appendFix(region.get(), QStringLiteral("a3"), kWgs84,
                  kBoulderLongitude, kBoulderLatitude, kBoulderElevation);

        CHECK(exportedOutputCS(region.get(), Policy::Shareable) == kUtmZone13N);
    }

    SECTION("no coordinate system anywhere names none") {
        auto region = makeRegion(QString());

        CHECK(exportedOutputCS(region.get(), Policy::Shareable).isEmpty());
        CHECK(exportedOutputCS(region.get(), Policy::WorkingFrame).isEmpty());
    }

    // Asserted through the emitted file rather than by reading the field back:
    // File → Export passes a default-constructed Options and never names a
    // policy, so what matters is which system that lands on in the .svx.
    SECTION("a caller that names no policy gets the shareable one") {
        auto region = makeRegion(QString());
        appendFix(region.get(), QStringLiteral("a1"), kWgs84,
                  kBoulderLongitude, kBoulderLatitude, kBoulderElevation);

        CHECK(exportedOutputCS(region.get(), cwSurvexExporterRegion::Options())
              == kUtmZone13N);
    }
}

TEST_CASE("The solve never borrows the shared-file coordinate system",
          "[cwSurvexExporterRegion_OutputCS]")
{
    // The driver .svx isn't inspectable from here, so the assertion is on what
    // the solve does with a region that only the shareable policy can name a
    // system for: a geographic fix and no project CS. Cavern refuses a *cs
    // with no *cs out, and stopping is the right answer — solving in a UTM
    // zone nobody asked for would move every station in the scene. Once the
    // working frame is the project's own local projection there is always one
    // to name, and this becomes a test that the solve uses it.
    auto region = makeRegion(QString());
    appendFix(region.get(), QStringLiteral("a1"), kWgs84,
              kBoulderLongitude, kBoulderLatitude, kBoulderElevation);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(region.get());
    plotManager->waitToFinish();

    CHECK(plotManager->hasSolveError());
    CHECK_FALSE(region->cave(0)->stationPositionLookup().hasPosition(QStringLiteral("a2")));
}
