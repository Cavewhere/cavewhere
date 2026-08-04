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
#include "cwSurvexCS.h"
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

std::unique_ptr<cwCavingRegion> makeRegion()
{
    auto region = std::make_unique<cwCavingRegion>();

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

//! What the exported file names as *cs out, verbatim — a local projection is
//! written in survex's CUSTOM form, so compare against toSurvexCS(). Empty when
//! the file names nothing.
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

    SECTION("a projected fix is shareable as it stands") {
        auto region = makeRegion();
        appendFix(region.get(), QStringLiteral("a1"), kUtmZone13N,
                  kBoulderEasting, kBoulderNorthing, kBoulderElevation);

        CHECK(exportedOutputCS(region.get(), Policy::Shareable) == kUtmZone13N);
        // The working frame is the project's local projection, which the fix
        // anchored — never the fix's own zone.
        const QString frame = region->geoReference()->localCoordinateSystem();
        REQUIRE_FALSE(frame.isEmpty());
        CHECK(exportedOutputCS(region.get(), Policy::WorkingFrame)
              == cwSurvexCS::toSurvexCS(frame));
    }

    // The whole point of the split: a geographic fix would be no output system
    // at all to the solve — cavern refuses one — but the UTM zone containing it
    // is a perfectly good system to hand a reader, and the solve has the
    // project's own projection to work in either way.
    SECTION("a geographic fix yields its UTM zone to a shared file, not to the solve") {
        auto region = makeRegion();
        appendFix(region.get(), QStringLiteral("a1"), kWgs84,
                  kBoulderLongitude, kBoulderLatitude, kBoulderElevation);

        CHECK(exportedOutputCS(region.get(), Policy::Shareable) == kUtmZone13N);
        const QString frame = region->geoReference()->localCoordinateSystem();
        REQUIRE_FALSE(frame.isEmpty());
        CHECK(frame != kUtmZone13N);
        CHECK(exportedOutputCS(region.get(), Policy::WorkingFrame)
              == cwSurvexCS::toSurvexCS(frame));
    }

    SECTION("a fix that places nothing yet doesn't decide, so a later one does") {
        auto region = makeRegion();
        appendEmptyFix(region.get(), QStringLiteral("a1"), kWgs84);
        // A coordinate still at the origin is "not entered yet" too — off the
        // west coast of Africa is not where anyone's cave is.
        appendFix(region.get(), QStringLiteral("a2"), kWgs84, 0.0, 0.0, 0.0);
        appendFix(region.get(), QStringLiteral("a3"), kWgs84,
                  kBoulderLongitude, kBoulderLatitude, kBoulderElevation);

        CHECK(exportedOutputCS(region.get(), Policy::Shareable) == kUtmZone13N);
    }

    SECTION("no coordinate system anywhere names none") {
        auto region = makeRegion();

        CHECK(exportedOutputCS(region.get(), Policy::Shareable).isEmpty());
        CHECK(exportedOutputCS(region.get(), Policy::WorkingFrame).isEmpty());
    }

    // Asserted through the emitted file rather than by reading the field back:
    // File → Export passes a default-constructed Options and never names a
    // policy, so what matters is which system that lands on in the .svx.
    SECTION("a caller that names no policy gets the shareable one") {
        auto region = makeRegion();
        appendFix(region.get(), QStringLiteral("a1"), kWgs84,
                  kBoulderLongitude, kBoulderLatitude, kBoulderElevation);

        CHECK(exportedOutputCS(region.get(), cwSurvexExporterRegion::Options())
              == kUtmZone13N);
    }
}

TEST_CASE("The solve runs in the project's own local projection",
          "[cwSurvexExporterRegion_OutputCS]")
{
    // The driver .svx isn't inspectable from here, so the assertion is on what
    // the solve does with a region only the shareable policy could name a
    // system for: a geographic fix, which cavern refuses as *cs out. The solve
    // does not borrow that UTM zone — moving every station in the scene into a
    // zone nobody asked for — it uses the frame the fix anchored, and lands.
    auto region = makeRegion();
    appendFix(region.get(), QStringLiteral("a1"), kWgs84,
              kBoulderLongitude, kBoulderLatitude, kBoulderElevation);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(region.get());
    plotManager->waitToFinish();

    INFO(plotManager->solveErrorMessage().toStdString());
    CHECK_FALSE(plotManager->hasSolveError());
    CHECK(region->cave(0)->stationPositionLookup().hasPosition(QStringLiteral("a2")));
}
