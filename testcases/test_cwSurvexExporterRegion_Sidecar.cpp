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
#include "cwCavernRunner.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwSurveyChunk.h"
#include "cwSurvexExporterRegion.h"
#include "cwTrip.h"

//Qt includes
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

//Std includes
#include <memory>

namespace {

//! A low-distortion transverse Mercator centered on Mammoth Cave, on
//! NAD83(2011). PROJ's own WKT2:2019 for it. This is the shape of frame the
//! project derives, and the reason a sidecar exists at all: written as a PROJ
//! string the datum comes back "Unknown based on GRS 1980 ellipsoid", and
//! written inline the quotes end survex's string at PROJCRS[.
const QString kFrameWkt = QString::fromUtf8(
    R"WKT(PROJCRS["Mammoth Cave LDP",BASEGEOGCRS["NAD83(2011)",DATUM["NAD83 (National Spatial Reference System 2011)",ELLIPSOID["GRS 1980",6378137,298.257222101,LENGTHUNIT["metre",1]],ANCHOREPOCH[2010]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]],ID["EPSG",6318]],CONVERSION["Transverse Mercator",METHOD["Transverse Mercator",ID["EPSG",9807]],PARAMETER["Latitude of natural origin",37.1866,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8801]],PARAMETER["Longitude of natural origin",-86.1005,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8802]],PARAMETER["Scale factor at natural origin",1,SCALEUNIT["unity",1],ID["EPSG",8805]],PARAMETER["False easting",0,LENGTHUNIT["Metre",1],ID["EPSG",8806]],PARAMETER["False northing",0,LENGTHUNIT["Metre",1],ID["EPSG",8807]]],CS[Cartesian,2],AXIS["(E)",east,ORDER[1],LENGTHUNIT["Metre",1]],AXIS["(N)",north,ORDER[2],LENGTHUNIT["Metre",1]]])WKT");

//! UTM zone 16N as WKT2:2019 — the shape a GIS hands somebody who then pastes
//! it into a fix's coordinate system. A different system from the frame, so a
//! file naming both needs two sidecars.
const QString kFixWkt = QString::fromUtf8(
    R"WKT(PROJCRS["WGS 84 / UTM zone 16N",BASEGEOGCRS["WGS 84",ENSEMBLE["World Geodetic System 1984 ensemble",MEMBER["World Geodetic System 1984 (Transit)"],MEMBER["World Geodetic System 1984 (G730)"],MEMBER["World Geodetic System 1984 (G873)"],MEMBER["World Geodetic System 1984 (G1150)"],MEMBER["World Geodetic System 1984 (G1674)"],MEMBER["World Geodetic System 1984 (G1762)"],MEMBER["World Geodetic System 1984 (G2139)"],ELLIPSOID["WGS 84",6378137,298.257223563,LENGTHUNIT["metre",1]],ENSEMBLEACCURACY[2.0]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]],ID["EPSG",4326]],CONVERSION["UTM zone 16N",METHOD["Transverse Mercator",ID["EPSG",9807]],PARAMETER["Latitude of natural origin",0,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8801]],PARAMETER["Longitude of natural origin",-87,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8802]],PARAMETER["Scale factor at natural origin",0.9996,SCALEUNIT["unity",1],ID["EPSG",8805]],PARAMETER["False easting",500000,LENGTHUNIT["metre",1],ID["EPSG",8806]],PARAMETER["False northing",0,LENGTHUNIT["metre",1],ID["EPSG",8807]]],CS[Cartesian,2],AXIS["(E)",east,ORDER[1],LENGTHUNIT["metre",1]],AXIS["(N)",north,ORDER[2],LENGTHUNIT["metre",1]],ID["EPSG",32616]])WKT");

const QString kUtmZone16N = QStringLiteral("EPSG:32616");

//! A point in UTM zone 16, over Mammoth Cave.
constexpr double kMammothEasting = 578000.0;
constexpr double kMammothNorthing = 4116000.0;
constexpr double kMammothElevation = 200.0;

//! One cave, one trip, one shot, and a fix on its first station in \a fixCS.
std::unique_ptr<cwCavingRegion> makeRegion(const QString& fixCS)
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

    cwFixStation fix;
    fix.setStationName(QStringLiteral("a1"));
    fix.setInputCS(fixCS);
    fix.setCoordinate(kMammothEasting, kMammothNorthing, kMammothElevation);
    cave->fixStations()->appendFixStation(fix);

    return region;
}

//! The region as the exporter sees it, with \a frameCS standing in for the
//! project's derived frame.
cwCavingRegionData snapshotWithFrame(const cwCavingRegion* region, const QString& frameCS)
{
    cwCavingRegionData data = region->data();
    data.geoReference.localCoordinateSystem = frameCS;
    return data;
}

//! Export \a data to `region.svx` in \a dir, naming the frame in `*cs out`.
QStringList exportToDir(const cwCavingRegionData& data, const QTemporaryDir& dir)
{
    cwSurvexExporterRegion::Options options;
    options.outputCSPolicy = cwSurvexExporterRegion::OutputCSPolicy::WorkingFrame;

    const auto result =
        cwSurvexExporterRegion::exportRegion(data, dir.filePath(QStringLiteral("region.svx")),
                                             options);
    INFO(result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());

    QFile file(dir.filePath(QStringLiteral("region.svx")));
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
}

//! The contents of \a fileName in \a dir, with the trailing newline dropped.
QString sidecarText(const QTemporaryDir& dir, const QString& fileName)
{
    QFile file(dir.filePath(fileName));
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QString::fromUtf8(file.readAll()).trimmed();
}

//! Solve the exported file, and say what cavern said about it.
void solves(const QTemporaryDir& dir)
{
    const auto result = cwCavernRunner::run(dir.filePath(QStringLiteral("region.svx")),
                                            dir.filePath(QStringLiteral("region.3d")));
    INFO((result.hasError() ? result.errorMessage() : result.value().logText).toStdString());
    REQUIRE_FALSE(result.hasError());
    CHECK(QFile::exists(dir.filePath(QStringLiteral("region.3d"))));
}

} // namespace

TEST_CASE("A WKT frame is exported as a .prj sidecar cavern can read",
          "[cwSurvexExporterRegion_Sidecar]")
{
    auto region = makeRegion(kUtmZone16N);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QStringList lines = exportToDir(snapshotWithFrame(region.get(), kFrameWkt), dir);

    //The frame goes to a file named after the .svx, and the *cs out points at
    //it by bare name — cavern resolves it relative to the .svx it is reading.
    CHECK(lines.contains(QStringLiteral("*cs out CUSTOM @region.prj")));
    CHECK(sidecarText(dir, QStringLiteral("region.prj")) == kFrameWkt);

    //The fix's own system has an inline spelling, so it takes no file.
    CHECK(lines.contains(QStringLiteral("*cs ") + kUtmZone16N));
    CHECK_FALSE(QFile::exists(dir.filePath(QStringLiteral("region-2.prj"))));

    solves(dir);
}

TEST_CASE("Two systems with no inline spelling take a sidecar each",
          "[cwSurvexExporterRegion_Sidecar]")
{
    //One stem for the whole file would let the fix's system overwrite the
    //frame's, and both *cs lines would then name whichever wrote last — a cave
    //solved in the wrong system, with nothing in the file saying so.
    auto region = makeRegion(kFixWkt);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QStringList lines = exportToDir(snapshotWithFrame(region.get(), kFrameWkt), dir);

    CHECK(lines.contains(QStringLiteral("*cs out CUSTOM @region.prj")));
    CHECK(lines.contains(QStringLiteral("*cs CUSTOM @region-2.prj")));

    CHECK(sidecarText(dir, QStringLiteral("region.prj")) == kFrameWkt);
    CHECK(sidecarText(dir, QStringLiteral("region-2.prj")) == kFixWkt);

    solves(dir);
}

TEST_CASE("A pretty-printed sidecar solves too",
          "[cwSurvexExporterRegion_Sidecar]")
{
    //PROJ prints WKT across indented lines unless told otherwise, and that is
    //what the project's derived frame carries. Cavern joins the lines it reads
    //with a single space, which is safe because neither WKT nor PROJJSON allows
    //a newline inside a quoted name.
    QString pretty = kFrameWkt;
    pretty.replace(QStringLiteral(",CONVERSION["),
                   QStringLiteral(",\n    CONVERSION["));
    pretty.replace(QStringLiteral(",PARAMETER["),
                   QStringLiteral(",\n        PARAMETER["));
    pretty.replace(QStringLiteral(",CS[Cartesian"),
                   QStringLiteral(",\n    CS[Cartesian"));
    REQUIRE(pretty.contains(QLatin1Char('\n')));

    auto region = makeRegion(kUtmZone16N);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QStringList lines = exportToDir(snapshotWithFrame(region.get(), pretty), dir);

    CHECK(lines.contains(QStringLiteral("*cs out CUSTOM @region.prj")));
    CHECK(sidecarText(dir, QStringLiteral("region.prj")) == pretty);

    solves(dir);
}

TEST_CASE("One system named twice keeps one sidecar",
          "[cwSurvexExporterRegion_Sidecar]")
{
    //The frame and the fix are the same system here, so the second request for
    //it gets the file the first one took.
    auto region = makeRegion(kFrameWkt);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QStringList lines = exportToDir(snapshotWithFrame(region.get(), kFrameWkt), dir);

    CHECK(lines.contains(QStringLiteral("*cs out CUSTOM @region.prj")));
    CHECK(lines.contains(QStringLiteral("*cs CUSTOM @region.prj")));
    CHECK_FALSE(QFile::exists(dir.filePath(QStringLiteral("region-2.prj"))));

    solves(dir);
}
