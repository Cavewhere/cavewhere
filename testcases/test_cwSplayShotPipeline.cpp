/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "TestHelper.h"
#include "cwCave.h"
#include "cwCavernRunner.h"
#include "cwCavingRegion.h"
#include "cwLinePlotTask.h"
#include "cwShot.h"
#include "cwShotMeasurement.h"
#include "cwStation.h"
#include "cwSurvex3DFileReader.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

cwShotMeasurement makeSplay(const QString& distance,
                            const QString& compass,
                            const QString& clino)
{
    return cwShotMeasurement(cwDistanceReading(distance),
                             cwCompassReading(compass),
                             cwClinoReading(clino));
}

//The first two splays off a4 in the TopoDroid export used as ground truth
//(~/Desktop/svx/a0-a34.svx), and the block they have to come out as
QList<cwShotMeasurement> a4Splays()
{
    return {
        makeSplay("5.88", "124.1", "4.6"),
        makeSplay("5.42", "118.8", "2.9")
    };
}

QStringList a4SplayLines()
{
    return {
        QStringLiteral("a4 .. 5.88 124.1 4.6"),
        QStringLiteral("a4 .. 5.42 118.8 2.9")
    };
}

//A one-shot a4-a5 chunk on \a trip, which starts out front-sights only
cwSurveyChunk* addChunk(cwTrip* trip)
{
    trip->calibrations()->setBackSights(false);

    auto chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendShot(cwStation(QStringLiteral("a4")),
                      cwStation(QStringLiteral("a5")),
                      cwShot(QStringLiteral("10.52"), QStringLiteral("52.2"),
                             QStringLiteral("232.2"), QStringLiteral("-31.5"),
                             QStringLiteral("31.5")));
    return chunk;
}

QString exportTrip(cwTrip* trip, QStringList* errors = nullptr)
{
    cwSurvexExporterTripTask exporter;

    QString output;
    QTextStream stream(&output);
    exporter.writeTrip(stream, trip);
    stream.flush();

    if(errors != nullptr) {
        *errors = exporter.errors();
    }

    return output;
}

//The lines a *flags splay ... *flags not splay block wraps, with the column
//padding squeezed out so the test reads like the file does.
QStringList splayBlockLines(const QString& survexFile)
{
    QStringList block;
    bool inBlock = false;
    const QStringList lines = survexFile.split(QLatin1Char('\n'));
    for(const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if(trimmed == QStringLiteral("*flags splay")) {
            inBlock = true;
        } else if(trimmed == QStringLiteral("*flags not splay")) {
            inBlock = false;
        } else if(inBlock) {
            block.append(trimmed.simplified());
        }
    }
    return block;
}

//Solves \a svxContents with cavern and reads the .3d back
cwSurvex3DFileReader::NetworkAndLookup solveAndRead(const QTemporaryDir& dir,
                                                    const char* svxContents)
{
    const QString svxPath = dir.filePath(QStringLiteral("splays.svx"));
    const QString output3dPath = dir.filePath(QStringLiteral("splays.3d"));

    QFile svx(svxPath);
    REQUIRE(svx.open(QIODevice::WriteOnly));
    svx.write(svxContents);
    svx.close();

    const auto cavernResult = cwCavernRunner::run(svxPath, output3dPath);
    REQUIRE_FALSE(cavernResult.hasError());

    cwSurvex3DFileReader reader;
    return reader.readNetworkAndLookup(cavernResult.value().output3dPath);
}

//Cavern stores .3d coordinates as int32 centimeters, so two independently
//rounded endpoints can miss the ideal reduction by about 1.5cm.
constexpr double kCentimeterMargin = 0.02;

bool hasTipNear(const QList<QVector3D>& tips, const QVector3D& expected)
{
    for(const QVector3D& tip : tips) {
        if((tip - expected).length() <= kCentimeterMargin) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("A trip's splays export as legs to survex's anonymous wall station",
          "[SplayShot]") {
    cwTrip trip;
    cwSurveyChunk* chunk = addChunk(&trip);
    chunk->setStationSplays(0, a4Splays());

    const QString output = exportTrip(&trip);

    CHECK(splayBlockLines(output) == a4SplayLines());

    //*flags splay is redundant with `..`, but the block has to say what it is
    CHECK(output.contains(QStringLiteral("*flags splay")));
    CHECK(output.contains(QStringLiteral("*flags not splay")));

    //A splay is not a duplicate shot — that mislabeling is what this replaces
    CHECK_FALSE(output.contains(QStringLiteral("*flags duplicate")));
}

TEST_CASE("A trip with no splays writes no splay block", "[SplayShot]") {
    cwTrip trip;
    addChunk(&trip);

    const QString output = exportTrip(&trip);

    CHECK_FALSE(output.contains(QStringLiteral("*flags splay")));
    CHECK_FALSE(output.contains(QStringLiteral("..")));
}

TEST_CASE("A trip's splays all land in one block", "[SplayShot]") {
    //Splays are station-attached data, so they follow the whole centerline as
    //a single block rather than being spliced between the chunks.
    cwTrip trip;
    cwSurveyChunk* first = addChunk(&trip);
    first->setStationSplays(0, {makeSplay("5.88", "124.1", "4.6")});

    auto second = new cwSurveyChunk();
    trip.addChunk(second);
    second->appendShot(cwStation(QStringLiteral("b1")),
                       cwStation(QStringLiteral("b2")),
                       cwShot(QStringLiteral("4.0"), QStringLiteral("10.0"),
                              QString(), QStringLiteral("0.0"), QString()));
    second->setStationSplays(0, {makeSplay("3.10", "200.0", "-5.0")});

    const QString output = exportTrip(&trip);

    CHECK(output.count(QStringLiteral("*flags splay")) == 1);
    CHECK(splayBlockLines(output) == QStringList({
        QStringLiteral("a4 .. 5.88 124.1 4.6"),
        QStringLiteral("b1 .. 3.10 200.0 -5.0")
    }));
}

TEST_CASE("Splays keep their front-sight reading order in a backsighted trip",
          "[SplayShot]") {
    //A backsighted trip declares four reading columns; splays only ever have
    //two, so the block names its own *data and hands the trip's back after.
    cwTrip trip;
    cwSurveyChunk* chunk = addChunk(&trip);
    trip.calibrations()->setFrontSights(true);
    trip.calibrations()->setBackSights(true);
    chunk->setStationSplays(0, a4Splays());

    const QString output = exportTrip(&trip);

    CHECK(splayBlockLines(output) == a4SplayLines());

    const QString tripDataLine =
        QStringLiteral("*data normal from to tape compass backcompass clino backclino");
    const QString splayDataLine = QStringLiteral("*data normal from to tape compass clino");

    //Declared for the trip, switched for the splays, then switched back
    const int tripDataAt = output.indexOf(tripDataLine);
    const int splayDataAt = output.indexOf(splayDataLine);
    REQUIRE(tripDataAt >= 0);
    REQUIRE(splayDataAt > tripDataAt);
    CHECK(output.indexOf(tripDataLine, splayDataAt) > splayDataAt);
}

TEST_CASE("A splay cavern can't solve is dropped with an error naming its station",
          "[SplayShot]") {
    cwTrip trip;
    cwSurveyChunk* chunk = addChunk(&trip);

    SECTION("no distance") {
        //Cavern rejects a leg with an omitted tape reading
        chunk->setStationSplays(0, {makeSplay("", "124.1", "4.6")});
    }

    SECTION("no compass on a slanted leg") {
        chunk->setStationSplays(0, {makeSplay("5.88", "", "4.6")});
    }

    QStringList errors;
    const QString output = exportTrip(&trip, &errors);

    CHECK(splayBlockLines(output).isEmpty());
    CHECK_FALSE(output.contains(QStringLiteral("*flags splay")));

    REQUIRE(errors.size() == 1);
    CHECK(errors.at(0).contains(QStringLiteral("a4")));
}

TEST_CASE("A vertical splay exports without a compass reading", "[SplayShot]") {
    cwTrip trip;
    cwSurveyChunk* chunk = addChunk(&trip);
    chunk->setStationSplays(0, {makeSplay("2.4", "", "90")});

    QStringList errors;
    const QString output = exportTrip(&trip, &errors);

    CHECK(splayBlockLines(output) == QStringList({QStringLiteral("a4 .. 2.4 - UP")}));
    CHECK(errors.isEmpty());
}

TEST_CASE("cwSurvex3DFileReader reads solved splay tips", "[cwSurvex3DFileReader][SplayShot]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const auto parsed = solveAndRead(dir,
                                     "*fix a0 0 0 0\n"
                                     "*data normal from to tape compass clino\n"
                                     "a0 a1 10.0 0.0 0.0\n"
                                     "*flags splay\n"
                                     "a1 .. 5.0 90.0 0.0\n"
                                     "a1 .. 3.0 180.0 0.0\n"
                                     "*flags not splay\n");

    //The anonymous ends of the splays are stations to nobody, so the lookup
    //and the network still see only the two named stations
    CHECK(parsed.lookup.positions().size() == 2);
    CHECK(parsed.network.stations().size() == 2);

    REQUIRE(parsed.splayTips.size() == 1);
    REQUIRE(parsed.splayTips.contains(QStringLiteral("a1")));

    const QVector3D a1 = parsed.lookup.position(QStringLiteral("a1"));
    const QList<QVector3D> tips = parsed.splayTips.value(QStringLiteral("a1"));
    REQUIRE(tips.size() == 2);
    CHECK(hasTipNear(tips, a1 + QVector3D(5.0f, 0.0f, 0.0f)));
    CHECK(hasTipNear(tips, a1 + QVector3D(0.0f, -3.0f, 0.0f)));
}

TEST_CASE("A splay leg between two named stations stays a leg",
          "[cwSurvex3DFileReader][SplayShot]") {
    //An external survex file can flag a leg between named stations as a splay.
    //Both ends are real stations, so the network keeps it and no tip comes back.
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const auto parsed = solveAndRead(dir,
                                     "*fix a0 0 0 0\n"
                                     "*data normal from to tape compass clino\n"
                                     "a0 a1 10.0 0.0 0.0\n"
                                     "*flags splay\n"
                                     "a1 a2 5.0 90.0 0.0\n"
                                     "*flags not splay\n");

    CHECK(parsed.splayTips.isEmpty());
    CHECK(parsed.lookup.positions().size() == 3);
    CHECK(parsed.network.neighbors(QStringLiteral("a1")).contains(QStringLiteral("a2")));
}

TEST_CASE("Splays make the round trip through cavern into the line plot's result",
          "[LinePlot][SplayShot]") {
    //End to end: the exporter writes the splays, cavern solves them, the reader
    //reads the tips back, and the worker hands each cave its own.
    cwCavingRegion region;

    auto cave = new cwCave();
    cave->setName(QStringLiteral("SplayCave"));
    region.addCave(cave);

    auto trip = new cwTrip();
    trip->setName(QStringLiteral("SplayTrip"));
    trip->calibrations()->setBackSights(false);
    cave->addTrip(trip);

    auto chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendShot(cwStation(QStringLiteral("a1")),
                      cwStation(QStringLiteral("a2")),
                      cwShot(QStringLiteral("10.0"), QStringLiteral("0.0"),
                             QString(), QStringLiteral("0.0"), QString()));

    //One splay due east of a2, one straight down from it
    chunk->setStationSplays(1, {makeSplay("5.0", "90.0", "0.0"),
                                makeSplay("2.0", "0.0", "-90.0")});

    auto future = cwLinePlotTask::run(cwLinePlotTask::buildInput(&region));
    future.waitForFinished();
    REQUIRE(future.resultCount() == 1);

    const cwLinePlotTask::LinePlotResultData result = future.result();
    REQUIRE_FALSE(result.hasSolveError());
    REQUIRE(result.Caves.contains(cave->id()));

    const cwLinePlotTask::LinePlotCaveData caveData = result.Caves.value(cave->id());
    const cwSplayTipsByStation splayTips = caveData.splayTips();

    //Keyed by the station the splay hangs off, in the lookup's key space
    REQUIRE(splayTips.size() == 1);
    REQUIRE(splayTips.contains(QStringLiteral("a2")));

    const QVector3D a2 = caveData.stationPositions().position(QStringLiteral("a2"));
    const QList<QVector3D> tips = splayTips.value(QStringLiteral("a2"));
    REQUIRE(tips.size() == 2);
    CHECK(hasTipNear(tips, a2 + QVector3D(5.0f, 0.0f, 0.0f)));
    CHECK(hasTipNear(tips, a2 + QVector3D(0.0f, 0.0f, -2.0f)));
}
